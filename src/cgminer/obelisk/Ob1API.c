// Obelisk Ob1 API for SC1 and DCR1
#include "Ob1API.h"
#include "Ob1Utils.h"
#include "Ob1Hashboard.h"
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

//==================================================================================================
// Chip-level API
//==================================================================================================

// Program a job the specified engine(s).
ApiError ob1LoadJob(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, Job* pJob)
{
    ApiError error = GENERIC_ERROR;
    switch (gBoardModel) {
    case MODEL_SC1: {
        // Loop over M regs and write them to the engine
        Blake2BJob* pBlake2BJob = &(pJob->blake2b);
        for (int i = 0; i < E_SC1_NUM_MREGS; i++) {
            // Skip M4 (the nonce)
            if (i != E_SC1_REG_M4_RSV) {
                applog(LOG_ERR, "    M%d: 0x%016llX", i, pBlake2BJob->m[i]);
                error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_M0 + i, &(pBlake2BJob->m[i]));
                if (error != SUCCESS) {
                    return error;
                }
            }
        }
        break;
    }
    case MODEL_DCR1: {
        // Loop over M regs and write them to the engine
        Blake256Job* pBlake256Job = &(pJob->blake256);
        for (int i = 0; i < E_DCR1_NUM_MREGS; i++) {
            // Skip M4 (the nonce)
            if (i != E_SC1_REG_M4_RSV) {
                applog(LOG_ERR, "    M%d: 0x%08lX", i, pBlake256Job->m[i]);
                error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_M0 + i, &(pBlake256Job->m[i]));
                if (error != SUCCESS) {
                    return error;
                }
            }
        }
        break;
    }
    }

    return SUCCESS;
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
        uint32_t nonce32bitLower = (uint32_t)(lowerBound & 0xFFFFFFFFULL);
        ApiError error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_LB, &nonce32bitLower);
        if (error == SUCCESS) {
            uint32_t nonce32bitUpper = (uint32_t)(upperBound & 0xFFFFFFFFULL);
            error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_UB, &nonce32bitUpper);
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
ApiError ob1ReadNonces(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, NonceSet* nonceSet)
{
    Nonce nonce;
    switch (gBoardModel) {
    case MODEL_SC1: {
        uint64_t fsr;
        ApiError error = ob1SpiReadReg(boardNum, chipNum, engineNum, E_SC1_REG_FSR, &fsr);
        if (error != SUCCESS) {
            return GENERIC_ERROR;
        }
        int n = 0;
        uint8_t fsr_mask = (uint8_t)(fsr & 0xFFULL);
        // Quick check to early out if no nonces have been found
        if (fsr_mask > 0) {
            if (fsr_mask > 1) {
                applog(LOG_ERR, "***** Found more than 1 nonce!  fsr=0x%016llX", fsr);
            }

            uint8_t fifoDataReg = E_SC1_REG_FDR0;
            for (uint8_t i = 0; i < MAX_NONCE_FIFO_LENGTH; i++) {
                // See which bits are set and extract the nonces for them
                if ((1 << i) & fsr_mask) {
                    error = ob1SpiReadReg(boardNum, chipNum, engineNum, fifoDataReg + i, &(nonceSet->nonces[n]));
                    if (error != SUCCESS) {
                        return GENERIC_ERROR;
                    }
                    n++;
                }
            }
        }
        nonceSet->count = n;
        // applog(LOG_ERR, "nonceSet->count=%u", nonceSet->count);

        pulseSC1ReadComplete(boardNum, chipNum, engineNum);
        break;
    }
    case MODEL_DCR1: {
        uint32_t fsr;
        ApiError error = ob1SpiReadReg(boardNum, chipNum, engineNum, E_DCR1_REG_FSR, &fsr);
        if (error != SUCCESS) {
            return 0;
        }
        int n = 0;
        // Quick check to early out if no nonces have been found
        if (fsr != 0) {
            if ((fsr & 0x8ULL) > 1) {
                applog(LOG_ERR, "***** Found more than 1 nonce!  fsr=0x%016llX", fsr);
            }

            uint8_t fifoDataReg = E_DCR1_REG_FDR0;
            for (uint8_t i = 0; i < MAX_NONCE_FIFO_LENGTH; i++) {
                // See which bits are set and extract the nonces for them
                if (1 << i & fsr) {
                    uint32_t nonce32bit;
                    error = ob1SpiReadReg(boardNum, chipNum, engineNum, fifoDataReg + i, &nonce32bit);
                    if (error != SUCCESS) {
                        return 0;
                    }
                    nonceSet->nonces[n] = (uint64_t)nonce32bit;
                    n++;
                }
            }
        }
        nonceSet->count = n;

        pulseDCR1ReadComplete(boardNum, chipNum, engineNum);
        break;
    }
    }

    return SUCCESS;
}

// For Sia, pass a pointer to a single uint64_t
// For Decred, pass a pointer to an array of two uint64_t's
// The caller can iterate these bits to see which engines need a new job.
// This should be checked AFTER checking for nonces, but there is still a small
// race condition, where an engine could find a nonce and then assert done
// right after you checked it for nonces.  We could do a final quick nonce check
// if this is an issue before giving it another job.  Alternative is to ONLY
// check for nonces when DONE is asserted.  If we keep job size fo about 2^32 or 2^33,
// then the engines finish jobs in about 20 seconds, so as long as latency in the
// nonces is not a huge deal, that would avoid a huge amount of polling.
ApiError ob1GetDoneEngines(uint8_t boardNum, uint8_t chipNum, uint64_t* pData)
{
    ApiError error = GENERIC_ERROR;

    switch (gBoardModel) {
    case MODEL_SC1:
        error = ob1SpiReadChipReg(boardNum, chipNum, E_SC1_REG_EDR, pData);
        break;

    case MODEL_DCR1: {
        uint32_t dataLow;
        uint32_t dataHigh;
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
        *(&pData[1]) = ((uint64_t)dataHigh << 32) | (uint64_t)dataLow;

        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EDR2, &dataLow);
        if (error != SUCCESS) {
            return error;
        }
        // applog(LOG_ERR, "gdr: low2=0x%04lX", dataLow);

        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EDR3, &dataHigh);
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

// For Sia, pass a pointer to a single uint64_t
// For Decred, pass a pointer to an array of two uint64_t's
// The caller can iterate these bits to see which engines need a new job.
// This should be checked AFTER checking for nonces, but there is still a small
// race condition, where an engine could find a nonce and then assert done
// right after you checked it for nonces.  We could do a final quick nonce check
// if this is an issue before giving it another job.  Alternative is to ONLY
// check for nonces when DONE is asserted.  If we keep job size fo about 2^32 or 2^33,
// then the engines finish jobs in about 20 seconds, so as long as latency in the
// nonces is not a huge deal, that would avoid a huge amount of polling.
ApiError ob1GetBusyEngines(uint8_t boardNum, uint8_t chipNum, uint64_t* pData)
{
    ApiError error = GENERIC_ERROR;

    switch (gBoardModel) {
    case MODEL_SC1:
        error = ob1SpiReadChipReg(boardNum, chipNum, E_SC1_REG_EBR, pData);
        break;

    case MODEL_DCR1:
        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EBR0, &pData[0]);
        if (error != SUCCESS) {
            return error;
        }
        error = ob1SpiReadChipReg(boardNum, chipNum, E_DCR1_REG_EBR1, &pData[1]);
        break;
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
        uint32_t ocrA = newOCRValue && 0xFFFFFFFFL;
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
        uint64_t newOCRValue = (gLastOCRWritten[boardNum][chipNum] & ~(DCR1_OCR_CLK_DIV_MSK | DCR1_OCR_BIAS_64BIT_MASK))
            | getDCR1DividerBits(divider)
            | getDCR1BiasBits(bias);
        // Always mask in the clock enable bits
        newOCRValue = newOCRValue | DCR1_OCR_CORE_ENB;
        gLastOCRWritten[boardNum][chipNum] = newOCRValue;
        uint32_t ocrA = newOCRValue && 0xFFFFFFFFL;
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
    // Control board setup and POST functions
    ApiError error = ob1InitializeControlBoard();
    if (error == SUCCESS) {
        ob1SetRedLEDOff();
        ob1SetGreenLEDOff();
        // Detect and initialize hashing boards
        error = ob1InitializeHashBoards();
    }

    // Figure out what type of board is in slot 0
    gBoardModel = eGetBoardType(0);
    applog(LOG_ERR, "================================================================================\n");
    applog(LOG_ERR, "Setting gBoardModel to %s (%d) \n", gBoardModel == MODEL_SC1 ? "SC1" : "DCR1", gBoardModel);
    applog(LOG_ERR, "================================================================================\n");

    if (error == SUCCESS) {
        ob1SetRedLEDOff();
        ob1SetGreenLEDOn();
    } else {
        ob1SetRedLEDOn();
        ob1SetGreenLEDOff();
    }

    return error;
}

// If there is an error reading any of the values, the returned object's error
// field will contain an error code.
HashboardStatus ob1GetHashboardStatus(uint8_t boardNum)
{
    LOCK(&statusLock);
    // TODO: Wrap in a mutex to control access to the temp/voltage sensors
    HashboardStatus status;
    iUpdateTempSensors(boardNum);
    status.boardTemp = getIntTemp(boardNum);
    status.chipTemp = (double)getAsicTempInt(boardNum);
    // int16_t chipTempFracs = getAsicTempFracs(boardNum);
    // applog(LOG_ERR, "chipTemp=%f", status.chipTemp);
    status.powerSupplyTemp = getPSTemp(boardNum);

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

// Returns the fan speed in rpm
uint32_t ob1GetFanRPM(uint8_t fanNum)
{
    return GENERIC_ERROR;
}

// The percent can be 10-100
ApiError ob1SetFanSpeed(uint8_t fanNum, uint8_t percent)
{
    return GENERIC_ERROR;
}

//==================================================================================================
// Diagnostics API
//==================================================================================================

// Read the specified TDR register
uint64_t ob1ReadTDR(uint8_t regNum)
{
    return GENERIC_ERROR;
}
