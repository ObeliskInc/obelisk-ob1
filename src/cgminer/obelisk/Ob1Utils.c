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
    uint64_t data = DCR1_ECR_READ_COMPLETE;
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

// TODO: avoid redefining these functions

#define MIN_BIAS -5
#define MAX_BIAS 5

static void increaseBias(int8_t* currentBias, uint8_t* currentDivider)
{
    if (*currentBias == MAX_BIAS) {
        switch (*currentDivider) {
        case 2: case 4: case 8:
            *currentDivider /= 2;
            *currentBias = MIN_BIAS;
        }
    } else {
        *currentBias += 1;
    }
}

static void decreaseBias(int8_t* currentBias, uint8_t* currentDivider)
{
    if (*currentBias == MIN_BIAS) {
        switch (*currentDivider) {
        case 1: case 2: case 4:
            *currentDivider *= 2;
            *currentBias = MAX_BIAS;
        }
    } else {
        *currentBias -= 1;
    }
}

static void normalizeString(int8_t biases[], uint8_t dividers[], size_t n)
{
    for (;;) {
        // the string is normalized if at least one chip has minimum bias
        for (int i = 0; i < n; i++) {
            if (biases[i] == MIN_BIAS && dividers[i] == 8) {
                return;
            }
        }
        // otherwise, decrease the bias of every chip by 1
        for (int i = 0; i < n; i++) {
            decreaseBias(&biases[i], &dividers[i]);
        }
    }
}

static uint8_t findWorstChild(ControlLoopState *state)
{
    uint8_t worst = 0;
    for (uint8_t i = 0; i < POPULATION_SIZE; i++) {
        if (state->population[i].fitness < state->population[worst].fitness) {
            worst = i;
        }
    }
    return worst;
}

static GenChild breedChild(ControlLoopState *state)
{
    // read all of the randomness we need up-front
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (!urandom) {
        applog(LOG_ERR, "No /dev/urandom! Returning first child");
        return state->population[0];
    }
    uint8_t buf[64];
    fread(buf, 1, sizeof(buf), urandom);
    fclose(urandom);
    uint8_t *randByte = buf;

    // pick two random parents
    GenChild *parent1 = &state->population[(*randByte++) % state->populationSize];
    GenChild *parent2 = &state->population[(*randByte++) % state->populationSize];


    // for each trait in the child, choosing randomly whether to take the trait
    // from parent1 or parent2
    GenChild child;
    child.voltageLevel = ((*randByte++) & 1) ? parent1->voltageLevel : parent2->voltageLevel;
    for (uint8_t i = 0; i < sizeof(child.chipBiases); i++) {
        uint8_t r = *randByte++;
        child.chipBiases[i]   = (r & 1) ? parent1->chipBiases[i]   : parent2->chipBiases[i];
        child.chipDividers[i] = (r & 1) ? parent1->chipDividers[i] : parent2->chipDividers[i];
    }

    // mutate each trait by choosing randomly whether to increment it,
    // decrement it, or leave it unchanged
    //
    // TODO: this code doesn't take into account the minimum/maximum levels
    // for the model. When setVoltageLevel is called, it may overwrite
    // state->currentVoltageLevel with the minimum or maximum, so we have to
    // update child.voltageLevel to reflect that.
    uint8_t r = *randByte++;
    if (r % 16 == 0) {
        child.voltageLevel++;
    } else if (r % 16 == 1) {
        child.voltageLevel--;
    }
    for (uint8_t i = 0; i < sizeof(child.chipBiases); i++) {
        r = *randByte++;
        if (r % 16 == 0) {
            increaseBias(&child.chipBiases[i], &child.chipDividers[i]);
        } else if (r % 16 == 1) {
            decreaseBias(&child.chipBiases[i], &child.chipDividers[i]);
        }
    }

    return child;
}

void geneticAlgoIter(ControlLoopState *state)
{
    // depreciate the fitness of current population
    //
    // NOTE: this prevents the algo from falling into local maxima, and also
    // helps it adapt to changing environmental factors (e.g. ambient
    // temperature)
    for (int i = 0; i < state->populationSize; i++) {
        state->population[i].fitness *= 0.998;
    }

    applog(LOG_ERR, "Current generation (%u):", state->populationSize);
    for (int i = 0; i < state->populationSize; i++) {
        GenChild c = state->population[i];
        int8_t *b = c.chipBiases;
        uint8_t *d = c.chipDividers;
        applog(LOG_ERR, "  %u: fitness %f, voltage %u, chips = %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i ",
            i, c.fitness, c.voltageLevel, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++);
    }
    GenChild c = state->curChild;
    int8_t *b = c.chipBiases;
    uint8_t *d = c.chipDividers;
    applog(LOG_ERR, "Current child: fitness %f, voltage %u, chips = %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i ",
            c.fitness, c.voltageLevel, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++);

	// Normailze the current child before saving, so that the population is
	// always normalized.
    normalizeString(state->curChild.chipBiases, state->curChild.chipDividers, sizeof(child.chipBiases));
    // evalulate performance of current child
    if (state->populationSize < POPULATION_SIZE) {
        state->population[state->populationSize++] = state->curChild;
    } else {
        uint8_t worst = findWorstChild(state);
        if (state->curChild.fitness > state->population[worst].fitness) {
            state->population[worst] = state->curChild;
        }
    }

    // breed two random parents and mutate the offspring to produce a new child
    state->curChild = breedChild(state);

    c = state->curChild;
    b = c.chipBiases;
    d = c.chipDividers;
    applog(LOG_ERR, "New child: voltage %u, chips = %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i %u.%i ",
            c.voltageLevel, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++, *d++, *b++);


    // set voltage and chip biases according to new child
    // TODO: replace these fields entirely?
    state->currentVoltageLevel = state->curChild.voltageLevel;
    memcpy(state->chipBiases, state->curChild.chipBiases, sizeof(state->chipBiases));
    memcpy(state->chipDividers, state->curChild.chipDividers, sizeof(state->chipDividers));
}

ApiError loadThermalConfig(char *name, int boardID, ControlLoopState *state)
{
    char path[64];
    snprintf(path, sizeof(path), "/root/.cgminer/settings_v1.4_%s_%d.bin", name, boardID);
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return GENERIC_ERROR;
    }
    fread(&state->populationSize, sizeof(uint8_t), 1, file);
    if (state->populationSize >= POPULATION_SIZE) {
        return GENERIC_ERROR;
    }
    if (fread(state->population, sizeof(GenChild), state->populationSize, file) != state->populationSize) {
        return GENERIC_ERROR;
    }
    if (fread(&state->curChild, sizeof(GenChild), 1, file) != 1) {
        return GENERIC_ERROR;
    }
    fclose(file);
    if (ferror(file) != 0) {
        return GENERIC_ERROR;
    }

    // set voltage and chip biases according to curChild
    state->currentVoltageLevel = state->curChild.voltageLevel;
    memcpy(state->chipBiases, state->curChild.chipBiases, sizeof(state->chipBiases));
    memcpy(state->chipDividers, state->curChild.chipDividers, sizeof(state->chipDividers));

    return SUCCESS;
}

ApiError saveThermalConfig(char *name, int boardID, ControlLoopState *state)
{
    char path[64];
    char tmppath[64];
    snprintf(path, sizeof(path), "/root/.cgminer/settings_v1.4_%s_%d.bin", name, boardID);
    snprintf(tmppath, sizeof(tmppath), "/root/.cgminer/settings_v1.4_%s_%d.bin_tmp", name, boardID);
    FILE *file = fopen(tmppath, "wb");
    if (file == NULL) {
        return GENERIC_ERROR;
    }
    fwrite(&state->populationSize, sizeof(uint8_t), 1, file);
    fwrite(state->population, sizeof(GenChild), state->populationSize, file);
    fwrite(&state->curChild, sizeof(GenChild), 1, file);
    fflush(file);
    fclose(file);
    if (ferror(file) != 0) {
        return GENERIC_ERROR;
    }
    if (rename(tmppath, path) != 0) {
        return GENERIC_ERROR;
    }
    return SUCCESS;
}
