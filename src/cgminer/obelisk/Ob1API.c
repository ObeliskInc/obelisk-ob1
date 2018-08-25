// Obelisk Ob1 API for SC1 and DCR1
#include "Ob1API.h"
#include "Ob1Utils.h"
#include "Ob1Hashboard.h"
#include "Ob1FanCtrl.h"
#include "CSS_SC1_hal.h"
#include "CSS_DCR1_hal.h"
#include "err_codes.h"
#include "MCP9903_hal.h"
#include "MCP23S17_hal.h"
#include "ads1015.h"
#include "miner.h"
#include <stdio.h>
#include <stdlib.h>

// Globals
HashboardModel gBoardModel;

// Last value written to the OCR register
// We save the last clock divider and bias for each ASIC on each board
uint64_t gLastOCRWritten[MAX_NUMBER_OF_HASH_BOARDS][NUM_CHIPS_PER_STRING];

// Maintain shadow registers so that we avoid writing SPI registers that already
// have the same value.
Job gShadowJobRegs[MAX_NUMBER_OF_HASH_BOARDS];

// TODO: Would be nice to split these up into interfaces and call device-specific functions instead
//       of combining both device types into each function.


void readAndPrintAllJobRegs(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum) {
    ApiError error;
    if (chipNum == ALL_CHIPS) {
        chipNum = 0;
    }
    if (engineNum == ALL_ENGINES) {
        engineNum = 0;
    }

    for (int i = 0; i < E_DCR1_NUM_VREGS; i++) {
        uint32_t data;
        ob1SpiReadReg(boardNum, chipNum, engineNum, E_DCR1_REG_V00 + i, &data);
        applog(LOG_ERR, "    V%d: 0x%08lX (readback)", i, data);
    }

    // The header tail is in the M regsiters
    for (int i = 0; i < E_DCR1_NUM_MREGS; i++) {
        uint32_t data;
        int regAddr = E_DCR1_REG_M0 + i;
        // For some reason, CSS decided to skip register 0x20, so regs M10 onwards are bigger by 1
        if (i >= 10)  {
            regAddr += (E_DCR1_REG_M10 - (E_DCR1_REG_M9 + 1));
        }
        ob1SpiReadReg(boardNum, chipNum, engineNum,  regAddr, &data);
        applog(LOG_ERR, "    M%02d: 0x%08lX (readback)", i, data);
    }

    // The Match register has the same value as V7
    uint32_t data;
    ob1SpiReadReg(boardNum, chipNum, engineNum, E_DCR1_REG_V0MATCH, &data);
    applog(LOG_ERR, "    V0MATCH: 0x%08lX", data);
}

//==================================================================================================
// Chip-level API
//==================================================================================================

// Program a job the specified engine(s).
ApiError ob1LoadJob(int* spiLoadJobTime, uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, Job* pJob)
{
    ApiError error = GENERIC_ERROR;
    int writesAvoided = 0;

    switch (gBoardModel) {
    case MODEL_SC1: {
        // Loop over M regs and write them to the engine
        Blake2BJob* pBlake2BJob = &(pJob->blake2b);
        for (int i = 0; i < E_SC1_NUM_MREGS; i++) {
            // Skip M4 (the nonce)
            if (i != E_SC1_REG_M4_RSV) {
                uint64_t data = pBlake2BJob->m[i];
                // Only write if the M register differs from the shadow register
                if (chipNum != ALL_CHIPS || gShadowJobRegs[boardNum].blake2b.m[i] != data) {
                    // applog(LOG_ERR, "    M%d: 0x%016llX", i, pBlake2BJob->m[i]);
                    error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_M0 + i, &data);
                    if (error != SUCCESS) {
                        return error;
                    }
                    // Update the shadow register(s)
                    gShadowJobRegs[boardNum].blake2b.m[i] = data;
                } else {
                    writesAvoided++;
                }
            }
        }
        break;
    }
    case MODEL_DCR1: {
        cgtimer_t start_WriteReg, end_WriteReg, duration_WriteReg;
        // Loop over M regs and write them to the engine
        Blake256Job* pBlake256Job = &(pJob->blake256);

        // The Match register has the same value as V7
        // We handle this first so that the check of the shadow register works correctly.  If we did the check after
        // setting the V registers, then V7 would have already been updated.
        uint32_t data = pBlake256Job->v[7];
        // Only write if the M register differs from the shadow register
        // TODO: If we decide to start sending separate jobs to each engine for Decred, then we can extend
        // the shadow register code to keep copies of all engine registers.
        if (chipNum != ALL_CHIPS || gShadowJobRegs[boardNum].blake256.v[7] != data) {
            // applog(LOG_ERR, "    V0MATCH: 0x%08lX", data);
            cgtimer_time(&start_WriteReg);
            error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_V0MATCH, &data);
            cgtimer_time(&end_WriteReg);
            cgtimer_sub(&end_WriteReg, &start_WriteReg, &duration_WriteReg);
            *spiLoadJobTime += cgtimer_to_ms(&duration_WriteReg);
            if (error != SUCCESS) {
                return error;
            }
            // V7 will be updated in the shadow registers below, so no need to do anything here
        } else {
            writesAvoided++;
        }

        // The midstate is in the V registers
        for (int i = 0; i < E_DCR1_NUM_VREGS; i++) {
            if (i < 8) {
                uint32_t data = pBlake256Job->v[i];
                // Only write if the V register differs from the shadow register
                if (chipNum != ALL_CHIPS || gShadowJobRegs[boardNum].blake256.v[i] != data) {
                    // applog(LOG_ERR, "    V%d: 0x%08lX", i, data);
                    cgtimer_time(&start_WriteReg);
                    error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_V00 + i, &data);
                    cgtimer_time(&end_WriteReg);
                    cgtimer_sub(&end_WriteReg, &start_WriteReg, &duration_WriteReg);
                    *spiLoadJobTime += cgtimer_to_ms(&duration_WriteReg);
                    if (error != SUCCESS) {
                        return error;
                    }
                    // Update the shadow register(s)
                    // TODO: Make this work for individual engines
                    gShadowJobRegs[boardNum].blake256.v[i] = data;
                } else {
                    writesAvoided++;
                }
            }
        }

        // The header tail is in the M regsiters
        for (int i = 0; i < E_DCR1_NUM_MREGS; i++) {
            if (i != 3) {
                uint32_t data = pBlake256Job->m[i];
                int regAddr = E_DCR1_REG_M0 + i;
                // For some reason, CSS decided to skip register 0x20, so regs M10 onwards are bigger by 1
                if (i >= 10)  {
                    regAddr += (E_DCR1_REG_M10 - (E_DCR1_REG_M9 + 1));
                }
                // Only write if the M register differs from the shadow register
                if (chipNum != ALL_CHIPS || gShadowJobRegs[boardNum].blake256.m[i] != data) {
                    // applog(LOG_ERR, "    M%02d: 0x%08lX  (regAddr = 0x%02X)", i, data, regAddr);
                    cgtimer_time(&start_WriteReg);
                    error = ob1SpiWriteReg(boardNum, chipNum, engineNum,  regAddr, &data);
                    cgtimer_time(&end_WriteReg);
                    cgtimer_sub(&end_WriteReg, &start_WriteReg, &duration_WriteReg);
                    *spiLoadJobTime += cgtimer_to_ms(&duration_WriteReg);
                    if (error != SUCCESS) {
                        return error;
                    }

                    // Update the shadow register(s)
                    // TODO: Make this work for individual engines
                    gShadowJobRegs[boardNum].blake256.m[i] = data;
                } else {
                    writesAvoided++;
                }
            }
        }
        break;
    }
    }

    // readAndPrintAllJobRegs(boardNum, chipNum, engineNum);
    if (writesAvoided) {
        // applog(LOG_ERR, "++++++++++ writesAvoided= %d (due to shadow registers)", writesAvoided);
    }

    return error;
}

// Set the upper and lower bounds for the specified engine(s).
ApiError ob1SetNonceRange(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, Nonce lowerBound, Nonce upperBound)
{
    switch (gBoardModel) {
    case MODEL_SC1: {
        ApiError error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_LB, &lowerBound);
        if (error == SUCCESS) {
            error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_UB, &upperBound);
        }
        return error;
    }
    case MODEL_DCR1: {
        ApiError error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_LB, &lowerBound);
        if (error == SUCCESS) {
            error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_UB, &upperBound);
            // applog(LOG_ERR, "Set nonce range: 0x%08lX -> 0x%08lX (%u:%u:%u)", nonce32bitLower, nonce32bitUpper, boardNum, chipNum, engineNum);
        }

        // TEST
        // uint32_t readLowerBound;
        // error = ob1SpiReadReg(boardNum, chipNum, engineNum, E_SC1_REG_LB, &readLowerBound);
        // if (readLowerBound != lowerBound) {
        //     applog(LOG_ERR, "lower bound MISMATCH!!!: read=0x%08lx  nonce32bit=0x%08lx  lower=0x%016llx", readLowerBound, nonce32bitLower, lowerBound);
        // } else {
        //     applog(LOG_ERR, "lower bound GOOD!!!: read=0x%08lx  nonce32bit=0x%08lx  lower=0x%016llx", readLowerBound, nonce32bitLower, lowerBound);
        // }
        // TEST
        return error;
    }
    }

    return SUCCESS;
}

// Set the upper and lower bounds of each engine in each chip in the chain,
// splitting up the provided nonce range based on the given step_size.  Each
// engine will be given the next chunk of the range (e.g., if the lower bound is
// 10 and the subrange_size is 5, the first engine would get 10-14, the second
// would get 15-19, the third would get 20-24, etc.).
ApiError ob1SpreadNonceRange(uint8_t boardNum, uint8_t chipNum, Nonce lowerBound, Nonce subrangeSize, Nonce* pNextNonce)
{
    int firstBoard;
    int lastBoard;
    if (boardNum == ALL_BOARDS) {
        firstBoard = 0;
        lastBoard = MAX_NUMBER_OF_HASH_BOARDS - 1;
    } else {
        firstBoard = boardNum;
        lastBoard = boardNum;
    }

    int firstChip;
    int lastChip;
    if (chipNum == ALL_CHIPS) {
        firstChip = 0;
        lastChip = NUM_CHIPS_PER_STRING - 1;
    } else {
        firstChip = chipNum;
        lastChip = chipNum;
    }

    Nonce currLowerBound = lowerBound;
    Nonce currUpperBound = currLowerBound + subrangeSize - 1;
    for (uint8_t board = firstBoard; board <= lastBoard; board++) {
        for (uint8_t chip = firstChip; chip <= lastChip; chip++) {
            for (uint8_t engine = 0; engine < ob1GetNumEnginesPerChip(); engine++) {

                ApiError error = ob1SetNonceRange(board, chip, engine, currLowerBound, currUpperBound);
                if (error != SUCCESS) {
                    return error;
                }
                currLowerBound = currUpperBound + 1;
                currUpperBound = currLowerBound + subrangeSize - 1;
            }
        }
    }

    if (pNextNonce) {
        *pNextNonce = currUpperBound + 1;
    }
    return SUCCESS;
}

// Read the nonces of the specified engine
ApiError managedOB1ReadNonces(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, NonceSet* nonceSet)
{
    Nonce nonce;
    switch (gBoardModel) {
    case MODEL_SC1: {
        uint64_t fsr;
        ApiError error = managedOB1SpiReadReg(boardNum, chipNum, engineNum, E_SC1_REG_FSR, &fsr);
        if (error != SUCCESS) {
            return error;
        }

        int n = 0;
        uint8_t fsr_mask = (uint8_t)(fsr & 0xFFULL);
        // Quick check to early out if no nonces have been found
        if (fsr_mask > 0) {
            uint8_t fifoDataReg = E_SC1_REG_FDR0;
            for (uint8_t i = 0; i < MAX_NONCE_FIFO_LENGTH; i++) {
                // See which bits are set and extract the nonces for them
                if ((1 << i) & fsr_mask) {
                    error = managedOB1SpiReadReg(boardNum, chipNum, engineNum, fifoDataReg + i, &(nonceSet->nonces[n]));
                    if (error != SUCCESS) {
                        return error;
                    }
                    n++;
                }
            }
        }
        nonceSet->count = n;

		uint64_t data = E_SC1_ECR_READ_COMPLETE;
		error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
		if (error != SUCCESS) {
			return error;
		}

		data = 0;
		error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
		if (error != SUCCESS) {
			return error;
		}

        break;
    }
    case MODEL_DCR1: {
        uint32_t fsr;
        ApiError error = managedOB1SpiReadReg(boardNum, chipNum, engineNum, E_DCR1_REG_FSR, &fsr);
        if (error != SUCCESS) {
            return error;
        }

        int n = 0;
        uint8_t fsr_mask = (uint8_t)(fsr & 0xFFULL);
        // Quick check to early out if no nonces have been found
        if (fsr_mask > 0) {
            uint8_t fifoDataReg = E_DCR1_REG_FDR0;
            for (uint8_t i = 0; i < MAX_NONCE_FIFO_LENGTH; i++) {
                // See which bits are set and extract the nonces for them
                if ((1 << i) & fsr_mask) {
                    Nonce nonce;
                    error = managedOB1SpiReadReg(boardNum, chipNum, engineNum, fifoDataReg + i, &nonce);
                    if (error != SUCCESS) {
                        return error;
                    }
                    // applog(LOG_ERR, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
                    // applog(LOG_ERR, "readnonce: 0x%08lX", nonce);
                    // applog(LOG_ERR, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
                    nonceSet->nonces[n] = nonce;
                    n++;
                }
            }
        }
        nonceSet->count = n;

		uint64_t data = DCR1_ECR_READ_COMPLETE;
		error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
		if (error != SUCCESS) {
			return error;
		}

		data = 0;
		error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
		if (error != SUCCESS) {
			return error;
		}

        break;
    }
    }

    return SUCCESS;
}

// For Sia, pass a pointer to a single uint64_t
// For Decred, pass a pointer to an array of two uint64_t's
ApiError ob1GetDoneEngines(uint8_t boardNum, uint8_t chipNum, uint64_t* pData)
{
    ApiError error = GENERIC_ERROR;

    switch (gBoardModel) {
    case MODEL_SC1:
        error = ob1SpiReadChipReg(boardNum, chipNum, E_SC1_REG_EDR, pData);
        break;

    case MODEL_DCR1: {
        uint32_t dataLow = 0;
        uint32_t dataHigh = 0;
        // applog(LOG_ERR, "Reading EDR registers for %u.%u", boardNum, chipNum);
        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EDR0, &dataLow);
        if (error != SUCCESS) {
            return error;
        }
        // applog(LOG_ERR, "gdr: low1=0x%04lX", dataLow);

        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EDR1, &dataHigh);
        if (error != SUCCESS) {
            return error;
        }
        // applog(LOG_ERR, "gdr: high1=0x%04lX", dataHigh);
        uint64_t dataLow64 = dataLow;
        uint64_t dataHigh64 = dataHigh;
        dataHigh64 = dataHigh64 << 32;
        *(&pData[1]) = dataHigh64 | dataLow64;
        // *(&pData[1]) = (((uint64_t)dataHigh) << 32) | (uint64_t)dataLow;

        dataLow = 0;
        dataHigh = 0;
        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EDR2, &dataLow);
        if (error != SUCCESS) {
            return error;
        }
        // applog(LOG_ERR, "gdr: low2=0x%04lX", dataLow);

        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EDR3, &dataHigh);
        if (error != SUCCESS) {
            return error;
        }
        dataLow64 = dataLow;
        dataHigh64 = dataHigh;
        dataHigh64 = dataHigh64 << 32;
        *(&pData[0]) = dataHigh64 | dataLow64;
        // *(&pData[0]) = (((uint64_t)dataHigh) << 32) | (uint64_t)dataLow;
        break;
    }
    }

    return error;
}

// For Sia, pass a pointer to a single uint64_t
// For Decred, pass a pointer to an array of two uint64_t's
ApiError ob1GetBusyEngines(uint8_t boardNum, uint8_t chipNum, uint64_t* pData)
{
    ApiError error = GENERIC_ERROR;

    switch (gBoardModel) {
    case MODEL_SC1:
        error = ob1SpiReadChipReg(boardNum, chipNum, E_SC1_REG_EBR, pData);
        break;

    case MODEL_DCR1: {
        uint32_t dataLow;
        uint32_t dataHigh;
        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EBR0, &dataLow);
        if (error != SUCCESS) {
            return error;
        }
        // applog(LOG_ERR, "gdr: low1=0x%04lX", dataLow);

        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EBR1, &dataHigh);
        if (error != SUCCESS) {
            return error;
        }
        // applog(LOG_ERR, "gdr: high1=0x%04lX", dataHigh);
        *(&pData[1]) = ((uint64_t)dataHigh << 32) | (uint64_t)dataLow;

        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EBR2, &dataLow);
        if (error != SUCCESS) {
            return error;
        }
        // applog(LOG_ERR, "gdr: low2=0x%04lX", dataLow);

        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EBR3, &dataHigh);
        if (error != SUCCESS) {
            return error;
        }
        *(&pData[0]) = ((uint64_t)dataHigh << 32) | (uint64_t)dataLow;
        // applog(LOG_ERR, "gdr: high2=0x%04lX", dataHigh);
        break;
    }
    }

    return error;
}

// Start the job and wait for the engine(s) to indicate busy.
ApiError managedOB1StartJob(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum)
{
    ApiError error = GENERIC_ERROR;
    switch (gBoardModel) {
    case MODEL_SC1: {
        // Need to set these bits high, then clear them to signal the start
        uint64_t data = E_SC1_ECR_RESET_SPI_FSM | E_SC1_ECR_RESET_CORE;
        error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
        if (error != SUCCESS) {
            return error;
        }

        data = 0;
        error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
        if (error != SUCCESS) {
            return error;
        }

        // Unmask bits that define the nonce fifo masks
        data = 0;
        error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_FCR, &data);
        if (error != SUCCESS) {
            return error;
        }

		data = E_SC1_ECR_VALID_DATA;
		ApiError error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
		if (error != SUCCESS) {
			return error;
		}

		data = 0;
		error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
		if (error != SUCCESS) {
			return error;
		}

        break;
    }
    case MODEL_DCR1: {
        // Need to set these bits high, then clear them to signal the start
        uint32_t data = DCR1_ECR_RESET_SPI_FSM | DCR1_ECR_RESET_CORE;
        error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
        if (error != SUCCESS) {
            return error;
        }

        data = 0;
        error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
        if (error != SUCCESS) {
            return error;
        }

        // Unmask bits that define the nonce fifo masks
        data = 0;
        error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_FCR, &data);
        if (error != SUCCESS) {
            return error;
        }

		data = DCR1_ECR_VALID_DATA;
		ApiError error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
		if (error != SUCCESS) {
			return error;
		}

		data = 0;
		error = managedOB1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
		if (error != SUCCESS) {
			return error;
		}

        break;
    }
    }

    return error;
}

// Start the job and wait for the engine(s) to indicate busy.
ApiError ob1StartJob(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum)
{
    ApiError error = GENERIC_ERROR;
    switch (gBoardModel) {
    case MODEL_SC1: {
        // Need to set these bits high, then clear them to signal the start
        uint64_t data = E_SC1_ECR_RESET_SPI_FSM | E_SC1_ECR_RESET_CORE;
        error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
        if (error != SUCCESS) {
            return error;
        }

        data = 0;
        error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
        if (error != SUCCESS) {
            return error;
        }

        // Unmask bits that define the nonce fifo masks
        data = 0;
        error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_FCR, &data);
        if (error != SUCCESS) {
            return error;
        }

        error = pulseSC1DataValid(boardNum, chipNum, engineNum);
        break;
    }
    case MODEL_DCR1: {
        // Need to set these bits high, then clear them to signal the start
        uint32_t data = DCR1_ECR_RESET_SPI_FSM | DCR1_ECR_RESET_CORE;
        error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
        if (error != SUCCESS) {
            return error;
        }

        data = 0;
        error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
        if (error != SUCCESS) {
            return error;
        }

        // Unmask bits that define the nonce fifo masks
        data = 0;
        error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_FCR, &data);
        if (error != SUCCESS) {
            return error;
        }
        error = pulseDCR1DataValid(boardNum, chipNum, engineNum);
        break;
    }
    }

    return error;
}

// Stop the specified engine(s) from running (turns off the clock).
ApiError ob1StopChip(uint8_t boardNum, uint8_t chipNum)
{
    switch (gBoardModel) {
    case MODEL_SC1: {
        break;
    }
    case MODEL_DCR1: {
        break;
    }
    }

    return SUCCESS;
}

// Register a function that will be called when a nonce is found.  The system
// will be interrupt driven, so the provided handler function will be called
// from an interrupt.  This means the handler should be short and quick.
ApiError ob1RegisterNonceHandler(NonceHandler handler)
{
    return GENERIC_ERROR;
}

// Register a function that will be called when a job exhausts the assigned
// nonce search space.  The system will be interrupt driven, so the provided
// handler function will be called from an interrupt.  This means the handler
// should be short and quick.
ApiError ob1RegisterJobCompleteHandler(JobCompleteHandler handler)
{
    // TODO: What does the handler get called with?
    return GENERIC_ERROR;
}

// Set the frequency bias for the specified engines.
//
// The bias must be a value between -5 and +5
ApiError ob1SetClockBias(uint8_t boardNum, uint8_t chipNum, int8_t bias)
{
    switch (gBoardModel) {
    case MODEL_SC1: {
        uint64_t newOCRValue = (gLastOCRWritten[boardNum][chipNum] & ~SC1_OCR_BIAS_MSK) | getSC1BiasBits(bias);
        gLastOCRWritten[boardNum][chipNum] = newOCRValue;
        // Always mask in the clock enable bits
        newOCRValue = (newOCRValue & ~SC1_OCR_CORE_MSK) | SC1_OCR_CORE_ENB;
        return ob1SpiWriteReg(boardNum, chipNum, 0, E_SC1_REG_OCR, &newOCRValue);
    }
    case MODEL_DCR1: {
        uint64_t newOCRValue = (gLastOCRWritten[boardNum][chipNum] & ~DCR1_OCR_BIAS_64BIT_MASK) | getDCR1BiasBits(bias);
        // Always mask in the clock enable bits
        newOCRValue = (newOCRValue & DCR1_OCR_CORE_MSK) | DCR1_OCR_CORE_ENB;
        gLastOCRWritten[boardNum][chipNum] = newOCRValue;
        uint32_t ocrA = newOCRValue & 0xFFFFFFFFL;
        ApiError error = ob1SpiWriteReg(boardNum, chipNum, ALL_ENGINES, E_DCR1_REG_OCRA, &ocrA);
        if (error == SUCCESS) {
            uint32_t ocrB = newOCRValue >> 32;
            gLastOCRWritten[boardNum][chipNum] = newOCRValue;
            ApiError error = ob1SpiWriteReg(boardNum, chipNum, ALL_ENGINES, E_DCR1_REG_OCRB, &ocrB);
        }
        return error;
    }
    }
}

// Set the clock divider for the specified engine(s).
//
// The divider must be a value of 1, 2, 4 or 8.
ApiError ob1SetClockDivider(uint8_t boardNum, uint8_t chipNum, uint8_t divider)
{
    switch (gBoardModel) {
    case MODEL_SC1: {
        uint64_t newOCRValue = gLastOCRWritten[boardNum][chipNum] & ~(SC1_OCR_CLK_DIV_MSK) | getSC1DividerBits(divider);
        // Always mask in the clock enable bits
        newOCRValue = (newOCRValue & ~SC1_OCR_CORE_MSK) | SC1_OCR_CORE_ENB;
        gLastOCRWritten[boardNum][chipNum] = newOCRValue;
        return ob1SpiWriteReg(boardNum, chipNum, ALL_ENGINES, E_SC1_REG_OCR, &newOCRValue);
    }
    case MODEL_DCR1: {
        uint64_t newOCRValue = gLastOCRWritten[boardNum][chipNum] & ~(DCR1_OCR_CLK_DIV_MSK) | getDCR1DividerBits(divider);
        // Always mask in the clock enable bits
        newOCRValue = (newOCRValue & DCR1_OCR_CORE_MSK) | DCR1_OCR_CORE_ENB;
        gLastOCRWritten[boardNum][chipNum] = newOCRValue;
        return ob1SpiWriteReg(boardNum, chipNum, ALL_ENGINES, E_DCR1_REG_OCRA, &newOCRValue);
        // No need to write to E_DCR1_REG_OCRA for divider
    }
    }
}

// Set the clock divider for the specified engine(s).
//
// The divider must be a value of 1, 2, 4 or 8.
ApiError ob1SetClockDividerAndBias(uint8_t boardNum, uint8_t chipNum, uint8_t divider, int8_t bias)
{
    switch (gBoardModel) {
    case MODEL_SC1: {
        uint64_t newOCRValue = (gLastOCRWritten[boardNum][chipNum] & ~(SC1_OCR_CLK_DIV_MSK | SC1_OCR_BIAS_MSK))
            | getSC1DividerBits(divider)
            | getSC1BiasBits(bias);
        // Always mask in the clock enable bits
        newOCRValue = newOCRValue | SC1_OCR_CORE_ENB;

        gLastOCRWritten[boardNum][chipNum] = newOCRValue;
        // applog(LOG_ERR, "Setting new OCR to 0x%016llX (divider=%u dividerBits=0x%016llX bias=%d biasBits=0x%016llX)", newOCRValue, divider, getSC1DividerBits(divider), bias, getSC1BiasBits(bias));
        // applog(LOG_ERR, "CH%u: Setting divider/bias:  /%u%s%d", boardNum, divider, bias < 0 ? "" : "+", bias);
        return ob1SpiWriteReg(boardNum, chipNum, ALL_ENGINES, E_SC1_REG_OCR, &newOCRValue);
    }
    case MODEL_DCR1: {
        // applog(LOG_ERR, "DCR1_OCR_BIAS_64BIT_MASK = 0x%016llX", DCR1_OCR_BIAS_64BIT_MASK);
        uint64_t newOCRValue = (gLastOCRWritten[boardNum][chipNum] & ~(DCR1_OCR_CLK_DIV_MSK | DCR1_OCR_BIAS_64BIT_MASK))
            | getDCR1DividerBits(divider)
            | getDCR1BiasBits(bias);
        // Always mask in the clock enable bits
        newOCRValue = newOCRValue | DCR1_OCR_CORE_ENB;
        gLastOCRWritten[boardNum][chipNum] = newOCRValue;
        uint32_t ocrA = newOCRValue & 0xFFFFFFFFL;
        ApiError error = ob1SpiWriteReg(boardNum, chipNum, ALL_ENGINES, E_DCR1_REG_OCRA, &ocrA);
        if (error == SUCCESS) {
            uint32_t ocrB = newOCRValue >> 32;
            gLastOCRWritten[boardNum][chipNum] = newOCRValue;
            ApiError error = ob1SpiWriteReg(boardNum, chipNum, ALL_ENGINES, E_DCR1_REG_OCRB, &ocrB);
        }
        return error;
    }
    }
}

void ob1EnableMasterHashClock(uint8_t boardNum, bool isEnabled)
{
    iSetHashClockEnable(boardNum, isEnabled);
}

bool ob1IsMasterHashClockEnabled(uint8_t boardNum)
{
    bool isEnabled;
    iGetHashClockEnable(boardNum, &isEnabled);
    return isEnabled;
}

//==================================================================================================
// Miner/Board-level API
//==================================================================================================

// Run a basic initialization process for the system.
// Checks that each board is working and are all of the same type and saves the type into
// gHashboardModel (HashboardModel::SC1 or HashboardModel::DCR1).
// If an error is returned, do not start the system any further (e.g., different board models
// installed, or fatal error of some sort).
ApiError ob1Initialize()
{
	// Set the lights to off while initializing the boards.
	ob1SetRedLEDOff();
	ob1SetGreenLEDOff();

	// Intialize control board.
    ApiError error = ob1InitializeControlBoard();
	if (error != SUCCESS) {
		ob1SetRedLEDOn();
		return error;
    }

    // Prepare fan controls, but don't start yet
    ob1InitializeFanCtrl();

	// Initialize hashing board.
	error = ob1InitializeHashBoards();
	if (error != SUCCESS) {
        ob1SetRedLEDOn();
    }

	// Leave the LEDs off if initialization is successful.
    return SUCCESS;
}

// If there is an error reading any of the values, the returned object's error
// field will contain an error code.
HashboardStatus ob1GetHashboardStatus(uint8_t boardNum)
{
    LOCK(&statusLock);
    // TODO: Wrap in a mutex to control access to the temp/voltage sensors
    HashboardStatus status;
    iUpdateTempSensors(boardNum);
    status.boardTemp = (double)getBoardTempInt(boardNum);
    status.chipTemp = (double)getAsicTempInt(boardNum);
    status.powerSupplyTemp = (double)getPSTempInt(boardNum);

    iADS1015ReadVoltages(boardNum); // Update the voltages
    status.asicV1 = getASIC_V1(boardNum);
    status.asicV15 = getASIC_V15(boardNum);
    status.asicV15IO = getASIC_V15IO(boardNum);
    status.powerSupply12V = getPS12V(boardNum);
    UNLOCK(&statusLock);
    return status;
}

// Valid values for voltage range from 8.5 to 10.0.  Since voltage is actually
// controlled in a discrete, stepwise fashion, the code will select the
// nearest value without going over the specified voltage.
// Values for voltage are about 17 to 127, and 127 is the lowest voltage.
// 17 gives about 10.5V, which is about the max we should set.
ApiError ob1SetStringVoltage(uint8_t boardNum, uint8_t voltage)
{
    LOCK(&statusLock);
    applog(LOG_ERR, "Set string voltage epot to %u", voltage);
    int result = iSetPSControl(boardNum, voltage);
    if (result != ERR_NONE) {
        UNLOCK(&statusLock);
        return GENERIC_ERROR;
    }
    UNLOCK(&statusLock);
    return SUCCESS;
}

// Read the Done status of the ASICs for one of the boards.  Returned value reflects
// which ASICs on the board are signaling they are done (corresponding bit is 1)
ApiError ob1ReadBoardDoneFlags(uint8_t boardNum, uint16_t* pValue)
{
    LOCK(&spiLock);
    int result = iReadPexPins(boardNum, PEX_DONE_ADR, pValue);
    UNLOCK(&spiLock);
    return result == ERR_NONE ? SUCCESS : GENERIC_ERROR;
}

// Read the Nonce status of the ASICs for one of the boards.  Returned value reflects
// which ASICs on the board are signaling they have a Nonce (corresponding bit is 1).
ApiError ob1ReadBoardNonceFlags(uint8_t boardNum, uint16_t* pValue)
{
    LOCK(&spiLock);
    int result = iReadPexPins(boardNum, PEX_NONCE_ADR, pValue);
    UNLOCK(&spiLock);
    return result == ERR_NONE ? SUCCESS : GENERIC_ERROR;
}


//==================================================================================================
// Controller-level API
//==================================================================================================

// Returns the number of hashboard slots in the miner.  Hardcoded to 3 for now.
uint8_t ob1GetNumHashboardSlots()
{
    return 3;
}

uint8_t ob1GetNumPresentHashboards()
{
    uint8_t numPresentHashboards = 0;
    for (int boardNum = 0; boardNum < ob1GetNumHashboardSlots(); boardNum++) {
        HashboardInfo info = ob1GetHashboardInfo(boardNum);
        if (info.isPresent) {
            numPresentHashboards++;
        }
    }

    return numPresentHashboards;
}

// Read the board info for the specified slot_num.  If there is an error or the
// board is not present, then the populated field will be set to false.
HashboardInfo ob1GetHashboardInfo(uint8_t boardNum)
{
    HashboardInfo info;
    info.isPresent = isBoardPresent(boardNum);
    info.id = getBoardUniqueId(boardNum);
    info.revision = getBoardRevision(boardNum);
    info.model = eGetBoardType(boardNum);
    info.numChains = 1;
    info.numChipsPerChain = NUM_CHIPS_PER_STRING;
    return info;
}

/*
bool isPresent; // The other fields are only reliable if isPresent is true
    uint64_t id;
    HashboardModel model;
    int revision;
    int numChains;
    int numChipsPerChain;
    */
HashboardModel ob1GetHashboardModel()
{
    return gBoardModel;
}

// Return the number of fans in the units
uint8_t ob1GetNumFans()
{
    return 2;
}

uint8_t ob1GetNumEnginesPerChip()
{
    // TODO: Make constants
    switch (gBoardModel) {
    case MODEL_SC1:
        return 64;
    case MODEL_DCR1:
        return 128;
    }
}

//==================================================================================================
// Diagnostics API
//==================================================================================================

// Read the specified TDR register
uint64_t ob1ReadTDR(uint8_t regNum)
{
    return GENERIC_ERROR;
}
