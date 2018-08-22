/**
 * \file CSS_SC1_hal.h
 *
 * \brief CSS ASIC SC1 firmware related declaration.
 *
 * \section Legal
 * **Copyright Notice:**
 * Copyright (c) 2018 All rights reserved.
 *
 * **Proprietary Notice:**
 *       This document contains proprietary information and neither the document
 *       nor said proprietary information shall be published, reproduced, copied,
 *       disclosed or used for any purpose other than the consideration of this
 *       document without the expressed written permission of a duly authorized
 *       representative of said Company.
 *
 */

#ifndef _CSS_SC1_HAL_H_INCLUDED
#define _CSS_SC1_HAL_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>
#include "CSS_SC1Defines.h"
#include <time.h>

/***  ASSOCIATED STRUCTURES/ENUMS      ***/
#define EMC_TEST_STATE_RUN 0
#define EMC_TEST_STATE_START 2
#define EMC_TEST_STATE_END 1

// Struct to fill in to transfer data (write/read) on SPI to the hashboard
// To use:
// Define the mode of transfer and define the target or range for transfer.
// Set uiBoard to define the board(s) to send data to.
// Set uiChip; to define the ASIC(s) to send data to.
// Set uiCore; to define the Core/engines(s) to send data to.
// Set eRegister; to define the register to target for transfer
// Set uiData; a data container with data to write or where to put any read data.
typedef struct {
    E_CSS_CMD_MODE_T eMode; // transaction mode type
    uint8_t uiBoard; // hash board [0 to (MAX_NUMBER_OF_HASH_BOARDS-1)]
    uint8_t uiChip; // chip number [0 to (MAX_SC1_CHIPS_PER_STRING-1)]; ignored if multicast (eMode = E_SC1_MODE_MULTICAST)
    uint8_t uiCore; // engine/core number [0 to (MAX_NUMBER_OF_SC1_CORES-1)] and 0x80; ignored if eMode = E_SC1_MODE_CHIP_WRITE
    E_SC1_CORE_REG_T eRegister; // register offset as enumerated value
    uint64_t uiData; // data container for transfer; SC1 is 64-bits
} S_SC1_TRANSFER_T;

// Struct to fill in to define job data to submit to ASIC core(s)
// assumes step register is always 1 so it isn't included here
typedef struct {
    uint64_t uiaMReg[10]; // M[9:0] register array; (M4 is not used)
    uint64_t uiLBReg; // lower bound register
    uint64_t uiUBReg; // upper bound register
    uint64_t uiSolution; // Expected solution (for testing)
} S_SC1_JOB_T;

/***   GLOBAL DATA DECLARATIONS     ***/
// Task pending globals; for ISR callbacks, etc

/***   GLOBAL FUNCTION PROTOTYPES   ***/
extern int iSC1StringStartup(uint8_t uiBoard);
extern int iSC1DeviceInit(uint8_t uiBoard);
extern int iSC1SpiTransfer(S_SC1_TRANSFER_T* psSC1Transfer, clock_t* transfer_time);
extern int iSC1MRegCheck(uint8_t uiHashBoard, uint8_t uiChip, clock_t* transfer_time);
extern int iStartupOCRInitValue(uint8_t uiHashBoard, uint8_t uiChip, clock_t* transfer_time);

// extern void SC1OCRBias(void);        // test function disabled
int iSetSC1OCRDividerAndBias(uint8_t uiBoard, uint8_t uiDivider, int8_t iVcoBias, clock_t* transfer_time);
extern int iSC1TestJobs(clock_t* transfer_time);
extern void SC1CmdTestRegRW2Read(void);
extern int iSC1TestReg(uint8_t uiHashBoard, uint8_t uiChip);
extern int iSC1EMCTestJob(int iBeginState);

#endif /* ifndef _CSS_SC1_HAL_H_INCLUDED */
