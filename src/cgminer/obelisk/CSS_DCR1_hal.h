/**
 * \file CSS_DCR1_hal.h
 *
 * \brief CSS ASIC DCR1 firmware related declaration.
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

#ifndef _CSS_DCR1_HAL_H_INCLUDED
#define _CSS_DCR1_HAL_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>
#include "CSS_DCR1Defines.h"

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
    E_CSS_DCR1_CMD_MODE_T eMode; // transaction mode type
    uint8_t uiBoard; // hash board [0 to (MAX_NUMBER_OF_HASH_BOARDS-1)]
    uint8_t uiChip; // chip number [0 to (MAX_DCR1_CHIPS_PER_STRING-1)]; ignored if multicast (eMode = E_DCR1_MODE_MULTICAST)
    uint8_t uiCore; // engine/core number [0 to (MAX_NUMBER_OF_DCR1_CORES-1)] and 0x80; ignored if eMode = E_DCR1_MODE_CHIP_WRITE
    uint8_t uiReg; // register offset
    uint32_t uiData; // data container for transfer; DCR1 is 32-bits
} S_DCR1_TRANSFER_T;

// Struct to fill in to define problem data to submit to ASIC core(s)
// For DCR1 assumes step register is always 1 so it isn't included here
typedef struct {
    uint32_t uiaMReg[13]; // M[12:0] register array; (M3 is not used)
    uint32_t uiLBReg; // lower bound register
    uint32_t uiUBReg; // upper bound register
    uint32_t uiaVReg[16]; // V[15:0] register array
    uint32_t uiMatchReg; // V0 match register
    uint32_t uiSolution; // Expected solution (for testing)
} S_DCR1_JOB_T;

/***   GLOBAL DATA DECLARATIONS     ***/
// Task pending globals; for ISR callbacks, etc

/***   GLOBAL FUNCTION PROTOTYPES   ***/

extern int iDCR1StringStartup(uint8_t uiBoard);
extern int iDCR1DeviceInit(uint8_t uiBoard);
extern int iDCR1SpiTransfer(S_DCR1_TRANSFER_T* psDCR1Transfer, clock_t* transfer_time);
extern int iDCR1MRegCheck(uint8_t uiHashBoard, uint8_t uiChip, clock_t* transfer_time);
extern int iDCR1StartupOCRInitValue(uint8_t uiHashBoard, uint8_t uiChip);
extern int iDCR1TestJobs(void);

extern int iSetDCR1OCRDividerAndBias(uint8_t uiBoard, uint8_t uiDivider, int8_t iVcoBias, clock_t* transfer_time);
extern int iDCR1TestJobs(void);
extern int iDCR1TestReg(uint8_t uiHashBoard, uint8_t uiChip);
extern int iDCR1EMCTestJob(int iBeginState);

#if 0
extern int iDCR1TestReg(uint8_t uiHashBoard, uint8_t uiChip);
#endif

#endif /* ifndef _CSS_DCR1_HAL_H_INCLUDED */
