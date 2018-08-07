/**
 * \file Ob1Hashboard.h
 *
 * \brief Hash board related firmware related declaration.
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

#ifndef _HASH_BOARD_H_INCLUDED
#define _HASH_BOARD_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>

/***  ASSOCIATED DEFINES      ***/
#define MAX_NUMBER_OF_HASH_BOARDS 3 // Number of boards supported
#define MAX_SC1_CHIPS_PER_STRING 15 // SC1 hash board supports 15 per board
#define MAX_DCR1_CHIPS_PER_STRING 15 // DCR1 hash board supports 15 per board
#define NUM_CHIPS_PER_STRING 15
#define MIN_WORKING_SC1_CHIPS_PER_STRING 12 // must pass RW test on this many on startup to attempt to start string

#define LAST_SC1_CHIP (MAX_SC1_CHIPS_PER_STRING - 1)
#define LAST_DCR1_CHIP (MAX_DCR1_CHIPS_PER_STRING - 1)
#define LAST_HASH_BOARD (MAX_NUMBER_OF_HASH_BOARDS - 1)

// Mask bits for hash board SPI comms
#define HASH_BOARD_1_SPIMASK 0x01
#define HASH_BOARD_2_SPIMASK 0x02
#define HASH_BOARD_3_SPIMASK 0x04

// Enum defines what type of asic: SIA SC1 or Decred DCR1
typedef enum {
    E_TYPE_SC1 = 0x0,
    E_TYPE_DCR1 = 0x1
} E_ASIC_TYPE_T;

/***  ASSOCIATED STRUCTURES/ENUMS      ***/
// Enum defines what the SPI CS will do on the hash board.  The boards have sub-nets of the SPI comms
// so as to limit routing fan-out and clocking to devices.
typedef enum {
    E_SPI_RESET = 0x0, // SPI cs will reset the board's GPIO expander
    E_SPI_ASIC = 0x1, // SPI comms go to the ASICs
    E_SPI_GPIO = 0x2, // SPI comms go to the GPIO expanders
    E_SPI_INVALID = 0x3 // SPI comms target undefined
} E_SC1_SPISEL_T;

/***   GLOBAL DATA DECLARATIONS     ***/
// Task pending globals; for ISR callbacks, etc

/***   GLOBAL FUNCTION PROTOTYPES   ***/
extern void HashBoardReset(uint8_t uiBoard);
extern void DeassertSPISelects(void);
extern int iIsBoardValid(uint8_t uiBoard, bool bBeStrict);
extern int iHashBoardInit(uint8_t uiBoard);
extern int iReadHashBoardUID(uint8_t uiBoard);
extern void SetHashBoardSpiMask(uint8_t uiBoardMask);
extern uint8_t uiGetHashBoardSpiMask(void);
extern int iStringStartup(uint8_t uiBoard);
extern void ReportSysVoltages(uint8_t uiBoard);
extern int iGetBoardStatus(uint8_t uiBoard);
extern int iSetBoardStatus(uint8_t uiBoard, int iStatus);
E_ASIC_TYPE_T eGetBoardType(uint8_t uiBoard);
extern int iSetPSControl(uint8_t uiBoard, uint64_t uiPSControlValue);
extern bool isBoardPresent(uint8_t uiBoard);
extern uint8_t getBoardRevision(uint8_t uiBoard);
extern uint64_t getBoardUniqueId(uint8_t uiBoard);

#endif /* ifndef _HASH_BOARD_H_INCLUDED */
