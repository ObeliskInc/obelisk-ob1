// Obelisk Ob1 API for SC1 and DCR1
#include "Ob1Utils.h"
#include "CSS_SC1_hal.h"
#include "CSS_DCR1_hal.h"
#include "err_codes.h"
#include "gpio_bsp.h"
#include "TWI_Support.h"
#include "spi.h"
#include "SPI_Support.h"
#include "MiscSupport.h"
#include "miner.h"
#include <string.h>
#include <pthread.h>

// Locks for thread safety of the API
pthread_mutex_t spiLock;
pthread_mutex_t statusLock; // Lock temperature & voltage device access

void ob1SetRedLEDOff()
{
    gpio_set_pin_level(CONTROLLER_RED_LED, false);
}

void ob1SetRedLEDOn()
{
    gpio_set_pin_level(CONTROLLER_RED_LED, true);
}

void ob1ToggleRedLED()
{
    gpio_toggle_pin_level(CONTROLLER_RED_LED);
}

void ob1SetGreenLEDOff()
{
    gpio_set_pin_level(CONTROLLER_GREEN_LED, false);
}

void ob1SetGreenLEDOn()
{
    gpio_set_pin_level(CONTROLLER_GREEN_LED, true);
}

void ob1ToggleGreenLED()
{
    gpio_toggle_pin_level(CONTROLLER_GREEN_LED);
}

ApiError ob1InitializeControlBoard()
{
    // Initialize locks
    INIT_LOCK(&spiLock);
    INIT_LOCK(&statusLock);

    int iResult = gpio_init();
    if (ERR_NONE != iResult) {
        return GENERIC_ERROR;
    }

    iResult = iTWIPortInit(NUM_TWI_PORTS); // initialize any TWI ports for hashing board comms
    if (ERR_NONE != iResult) {
        return GENERIC_ERROR;
    }

    iResult = spi_setup();
    if (ERR_NONE != iResult) {
        return GENERIC_ERROR;
    }
    return SUCCESS;
}

ApiError ob1InitializeHashBoards()
{
    HBSetSpiSelects(MAX_NUMBER_OF_HASH_BOARDS, true);
    int iResult = iHashBoardInit(MAX_NUMBER_OF_HASH_BOARDS);
    if (ERR_NONE != iResult) {
        return GENERIC_ERROR;
    }
    if (ERR_NONE != iResult) {
        return GENERIC_ERROR;
    }
    return SUCCESS;
}

//========================================================================================================
// NOTE: We will be running one thread per hashboard, so these SPI functions need to be thread-safe
//       Add a mutex around all low-level functions that access shared hardware.
//========================================================================================================

// Write the specified data
// Supports writing to ALL_BOARDS, ALL_CHIPS and ALL_ENGINES.
ApiError ob1SpiWriteReg(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, uint8_t registerId, void* pData)
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

    for (int i = firstBoard; i <= lastBoard; i++) {
        switch (gBoardModel) {
        case MODEL_SC1: {
            S_SC1_TRANSFER_T xfer;
            xfer.uiBoard = i;
            xfer.eRegister = (E_SC1_CORE_REG_T)registerId;

            // Set address/mode
            if (chipNum == ALL_CHIPS) {
                xfer.uiChip = 0; // Don't care - set to zero
                if (engineNum == ALL_ENGINES) {
                    xfer.eMode = E_SC1_MODE_MULTICAST;
                    xfer.uiCore = 0; // Don't care - set to zero
                } else {
                    // Not possible to write to a specific engine on all chips in one write
                    return GENERIC_ERROR;
                }
            } else if (engineNum == ALL_ENGINES) {
                xfer.eMode = E_SC1_MODE_CHIP_WRITE;
                xfer.uiChip = chipNum;
                xfer.uiCore = 0; // Don't care - set to zero
            } else {
                // Single chip, single engine
                xfer.eMode = E_SC1_MODE_REG_WRITE;
                xfer.uiChip = chipNum;
                xfer.uiCore = engineNum;
            }

            // Do the write
            memcpy(&xfer.uiData, pData, sizeof(xfer.uiData));
            LOCK(&spiLock);
            int result = iSC1SpiTransfer(&xfer);
            UNLOCK(&spiLock);
            if (result != ERR_NONE) {
                return GENERIC_ERROR;
            }
            break;
        }

        case MODEL_DCR1: {
            S_DCR1_TRANSFER_T xfer;
            xfer.uiBoard = i;
            xfer.uiReg = registerId;

            // Set address/mode
            if (chipNum == ALL_CHIPS) {
                xfer.uiChip = 0; // Don't care - set to zero
                if (engineNum == ALL_ENGINES) {
                    xfer.eMode = E_DCR1_MODE_MULTICAST;
                    xfer.uiCore = 0; // Don't care - set to zero
                } else {
                    // Not possible to write to a specific engine on all chips in one write
                    return GENERIC_ERROR;
                }
            } else if (engineNum == ALL_ENGINES) {
                xfer.eMode = E_DCR1_MODE_CHIP_WRITE;
                xfer.uiChip = chipNum;
                xfer.uiCore = 0; // Don't care - set to zero
            } else {
                // Single chip, single engine
                xfer.eMode = E_DCR1_MODE_REG_WRITE;
                xfer.uiChip = chipNum;
                xfer.uiCore = engineNum;
            }

            // Do the write
            memcpy(&xfer.uiData, pData, sizeof(xfer.uiData));
            LOCK(&spiLock);
            int result = iDCR1SpiTransfer(&xfer);
            UNLOCK(&spiLock);
            if (result != ERR_NONE) {
                return GENERIC_ERROR;
            }
            break;
        }
        }
    }

    return SUCCESS;
}

// Decred: It will copy 32 bits of data into the pData
// Sia: It will copy 64 bits of data into the pData
ApiError ob1SpiReadReg(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum, uint8_t registerId, void* pData)
{
    // Can only read from a specific board on a specific chip on a specific engine
    if (boardNum == ALL_BOARDS || chipNum == ALL_CHIPS || engineNum == ALL_ENGINES) {
        return GENERIC_ERROR;
    }

    switch (gBoardModel) {
    case MODEL_SC1: {
        S_SC1_TRANSFER_T xfer;
        xfer.eMode = E_DCR1_MODE_REG_READ;
        xfer.uiBoard = boardNum;
        xfer.uiChip = chipNum;
        xfer.uiCore = engineNum;
        xfer.eRegister = (E_SC1_CORE_REG_T)registerId;

        LOCK(&spiLock);
        int result = iSC1SpiTransfer(&xfer);
        UNLOCK(&spiLock);
        if (result != ERR_NONE) {
            return GENERIC_ERROR;
        }
        memcpy(pData, &xfer.uiData, sizeof(xfer.uiData));
        break;
    }

    case MODEL_DCR1: {
        S_DCR1_TRANSFER_T xfer;
        xfer.eMode = E_DCR1_MODE_REG_READ;
        xfer.uiBoard = boardNum;
        xfer.uiChip = chipNum;
        xfer.uiCore = engineNum;
        xfer.uiReg = registerId;

        LOCK(&spiLock);
        int result = iDCR1SpiTransfer(&xfer);
        UNLOCK(&spiLock);
        if (result != ERR_NONE) {
            return GENERIC_ERROR;
        }
        memcpy(pData, &xfer.uiData, sizeof(xfer.uiData));
        break;
    }
    }

    return SUCCESS;
}

// Read one of the "chip-level" registers instead of the engine-level registers
ApiError ob1SpiReadChipReg(uint8_t boardNum, uint8_t chipNum, uint8_t registerId, void* pData)
{
    switch (gBoardModel) {
    case MODEL_SC1:
        return ob1SpiReadReg(boardNum, chipNum, E_SC1_CHIP_REGS, registerId, pData);
    case MODEL_DCR1:
        return ob1SpiReadReg(boardNum, chipNum, E_DCR1_CHIP_REGS, registerId, pData);
    }
    return GENERIC_ERROR;
}

uint64_t getSC1DividerBits(uint8_t divider)
{
    uint64_t bits = 0;
    switch (divider) {
    case 1:
        bits = SC1_OCR_DIV1_VAL;
        break;
    case 2:
        bits = SC1_OCR_DIV2_VAL;
        break;
    case 4:
        bits = SC1_OCR_DIV4_VAL;
        break;
    default:
    case 8:
        bits = SC1_OCR_DIV8_VAL;
        break;
    }

    return bits;
}

uint64_t getDCR1DividerBits(uint8_t divider)
{
    uint64_t bits = 0;
    switch (divider) {
    case 1:
        bits = DCR1_OCR_CLK_DIV_1;
        break;
    case 2:
        bits = DCR1_OCR_CLK_DIV_2;
        break;
    case 4:
        bits = DCR1_OCR_CLK_DIV_4;
        break;
    default:
    case 8:
        bits = DCR1_OCR_CLK_DIV_8;
        break;
    }

    return bits;
}

uint64_t getSC1BiasBits(int8_t bias)
{
    uint64_t bits = 0;
    switch (bias) {
    case -5: // slowest has 5 bits bits set
        bits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_5 << SC1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -4: // 4 bits slow set
        bits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_4 << SC1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -3: // 3 bits slow set
        bits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_3 << SC1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -2: // 2 bits slow set
        bits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_2 << SC1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -1: // 1 bits slow set
        bits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_1 << SC1_OCR_VCO_BIAS_SLOW_POS;
        break;
    default:
    case 0: // 0 bits bits set; unbiased
        bits = 0;
        break;
    case 1: // 1 bit fast set
        bits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_1 << SC1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 2: // 2 bits fast set
        bits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_2 << SC1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 3: // 3 bits fast set
        bits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_3 << SC1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 4: // 4 bits fast set
        bits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_4 << SC1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 5: // 5 bits fast set
        bits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_5 << SC1_OCR_VCO_BIAS_FAST_POS;
        break;
    }
    return bits;
}

uint64_t getDCR1BiasBits(int8_t bias)
{
    uint64_t bits = 0;
    switch (bias) {
    case -5: // slowest has 5 bits bits set
        bits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_5 << DCR1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -4: // 4 bits slow set
        bits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_4 << DCR1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -3: // 3 bits slow set
        bits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_3 << DCR1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -2: // 2 bits slow set
        bits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_2 << DCR1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -1: // 1 bits slow set
        bits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_1 << DCR1_OCR_VCO_BIAS_SLOW_POS;
        break;
    default:
    case 0: // 0 bits bits set; unbiased
        bits = 0;
        break;
    case 1: // 1 bit fast set
        bits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_1 << DCR1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 2: // 2 bits fast set
        bits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_2 << DCR1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 3: // 3 bits fast set
        bits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_3 << DCR1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 4: // 4 bits fast set
        bits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_4 << DCR1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 5: // 5 bits fast set
        bits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_5 << DCR1_OCR_VCO_BIAS_FAST_POS;
        break;
    }
    return bits;
}

ApiError pulseSC1DataValid(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum)
{
    uint64_t data = E_SC1_ECR_VALID_DATA;
    ApiError error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
    if (error != SUCCESS) {
        return error;
    }

    data = 0;
    error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
    if (error != SUCCESS) {
        return error;
    }

    // // Wait for BUSY
    // int timeout = 100;
    // do {
    //     data = 0;
    //     // applog(LOG_ERR, "timeout=%d", timeout);
    //     delay_ms(1);
    //     error = ob1SpiReadReg(boardNum, chipNum, engineNum, E_SC1_REG_ESR, &data);
    //     if (error != SUCCESS) {
    //         return error;
    //     }
    //     if (data & (E_SC1_ESR_DONE | E_SC1_ESR_BUSY) != 0) {
    //         applog(LOG_ERR, "chip=%u engine=%u  data=0x%016llX", chipNum, engineNum, data);
    //         //if (chipNum == 0) {
    //         //}
    //         break;
    //     }
    // } while (--timeout > 0);

    return SUCCESS;
}

ApiError pulseDCR1DataValid(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum)
{
    uint64_t data = DCR1_ECR_VALID_DATA;
    ApiError error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
    if (error != SUCCESS) {
        return error;
    }

    data = 0;
    error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
    if (error != SUCCESS) {
        return error;
    }

    // Wait for BUSY
    // int timeout = 100;
    // do {
    //     data = 0;
    //     error = ob1SpiReadReg(boardNum, chipNum, engineNum, E_DCR1_REG_ESR, &data);
    //     if (error != SUCCESS) {
    //         return error;
    //     }
    //     if (data & (E_DCR1_ESR_DONE | E_DCR1_ESR_BUSY) != 0) {
    //         break;
    //     }
    //     delay_ms(1);
    // } while (--timeout > 0);
    return SUCCESS;
}

ApiError pulseSC1ReadComplete(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum)
{
    uint64_t data = E_SC1_ECR_READ_COMPLETE;
    ApiError error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
    if (error != SUCCESS) {
        return error;
    }

    data = 0;
    error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_SC1_REG_ECR, &data);
    if (error != SUCCESS) {
        return error;
    }

    return SUCCESS;
}

ApiError pulseDCR1ReadComplete(uint8_t boardNum, uint8_t chipNum, uint8_t engineNum)
{
    uint64_t data = E_SC1_ECR_READ_COMPLETE;
    ApiError error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
    if (error != SUCCESS) {
        return error;
    }

    data = 0;
    error = ob1SpiWriteReg(boardNum, chipNum, engineNum, E_DCR1_REG_ECR, &data);
    if (error != SUCCESS) {
        return error;
    }

    return SUCCESS;
}

void logNonceSet(NonceSet* pNonceSet, char* prefix)
{
    applog(LOG_ERR, "%s: NonceSet.count=%u", prefix, pNonceSet->count);
    for (uint8_t i = 0; i < pNonceSet->count; i++) {
        applog(LOG_ERR, "    nonces[%u] = 0x%016llX", i, pNonceSet->nonces[i]);
    }
}

// Functions for adding/subtracting bias and dividers
void incrementDivider(ControlLoopState* clState)
{
    switch (clState->currDivider) {
    case 1:
    case 2:
    case 4:
        clState->currDivider *= 2;
        break;
    default:
        // 8 or any other value means no change
        break;
    }
    clState->lastChangeUp = true;
}

void decrementDivider(ControlLoopState* clState)
{
    switch (clState->currDivider) {
    case 2:
    case 4:
    case 8:
        clState->currDivider /= 2;
        break;
    default:
        // 1 or any other value means no change
        break;
    }
    clState->lastChangeUp = false;
}

void incrementBias(ControlLoopState* clState)
{
    if (clState->currBias == MAX_BIAS) {
        int currDivider = clState->currDivider;
        decrementDivider(clState);
        // Prevent overflow
        if (currDivider > 1) {
            clState->currBias = MIN_BIAS;
        }
    } else {
        clState->currBias += 1;
    }
    clState->lastChangeUp = true;
}

void decrementBias(ControlLoopState* clState)
{
    if (clState->currBias == MIN_BIAS) {
        int currentDivider = clState->currDivider;
        incrementDivider(clState);
        // Prevent underflow of the bias
        if (currentDivider < 8) {
            clState->currBias = MAX_BIAS;
        }
    } else {
        clState->currBias -= 1;
    }
    clState->lastChangeUp = false;
}

void formatControlLoopState(char* buffer, ControlLoopState* clState)
{
    (void)sprintf(buffer, "HB%u: %3.1fC %c /%u%s%d bias",
        clState->boardId + 1,
        clState->lastTemp,
        clState->lastChangeUp ? '^' : 'v',
        clState->currDivider,
        clState->currBias < 0 ? "" : "+",
        clState->currBias);
}

void formatDividerAndBias(char* buffer, ControlLoopState* clState)
{
    (void)sprintf(buffer, "/%u%s%d",
        clState->currDivider,
        clState->currBias < 0 ? "" : "+",
        clState->currBias);
}
