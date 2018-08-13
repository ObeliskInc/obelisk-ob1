/**
 * \file Usermain.h
 *
 * \brief Header for Usermain.c
 * \date STARTED: 2018/03/20
 *
 */
#ifndef _USERMAIN_H_INCLUDED
#define _USERMAIN_H_INCLUDED

/**
 * @file Usermain.h
 *
 * @brief Main application functions
 *
 * @section Legal
 * **Copyright Notice:**
 * Copyright (c) 2018 All rights reserved.
 *
 * **Proprietary Notice:**
 *       This document contains proprietary information and neither the document
 *       nor said proprietary information shall be published, reproduced, copied,
 *       disclosed or used for any purpose other than the consideration of this
 *       document without the expressed written permission of a duly authorized
 *       representative of said Company.
 */

/***   INCLUDE FILES               ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#if (HW_ATSAMG55_XP_BOARD == HARDWARE_PLATFORM)
#include "Console.h" // development console/uart support
#endif // #if (HW_ATSAMG55_XP_BOARD == HARDWARE_PLATFORM)

#include "GenericDefines.h"

/***   GLOBAL DEFINITIONS          ***/
#define FW_VERSION_STRING "0.02.01"
#define FW_BUILD_TIMEDATE_STRING __DATE__ " "__TIME__

// Defines for possible target platforms
#define HW_ATSAMG55_XP_BOARD 0 // Atmel eval board
#define HW_SAMA5D27_SOM 1 // Microchip Arm-A5 SOM

#define HARDWARE_PLATFORM HW_SAMA5D27_SOM //NOTE: #TODO Tom - Unless I'm crazy, this is the only place you tell the project to use the actual board, not the atmel dev board

#if (HW_ATSAMG55_XP_BOARD == HARDWARE_PLATFORM)
#define HELLO_WORLD_MSG "\r\nATSAMG55XP Test Controller\r\n"
#define BOARD_NAME "ATSAMG55 XPlained Pro"
// On ATSAMG55XP board mapping of IO used for interrupts
//     #define PIO_BUTTON1 PIO_PA2_IDX     // user button is PA2
//     #define PIO_EXT3PIN9_IRQ PIO_PB3_IDX // IRQ from AFE ADS129X is PB3 on EXT3
//     SPI/TWI transfers are interrupt driven using async modes (with callbacks)
// #define ENABLE_TEST_CLK_OUT 1   // define this to turn on test clock output on asics (not for production due to EMI concerns)

#elif (HW_SAMA5D27_SOM == HARDWARE_PLATFORM)
#define HELLO_WORLD_MSG "\r\nSAMA527 Test Controller\r\n"
#define BOARD_NAME "SAMA5D27 SOM"
#else
#error "Undefined hardware"
#endif

#define ENABLE_EMC_TEST_FUNCTIONS 1 // don't define to disable this capability

#define STARTUP_MESSAGE "FW v:" FW_VERSION_STRING " (Compiled: " FW_BUILD_TIMEDATE_STRING " for " BOARD_NAME ")\r\n"

#define SYSTEM_BASE_TICK_PER_SEC 1000 // we are using a 1msec base timer tick
#define ONE_SEC (SYSTEM_BASE_TICK_PER_SEC)
#define ONE_MIN (60 * SYSTEM_BASE_TICK_PER_SEC)

// Fan related
#define MAX_NUM_FANS 2
// Struct for fan status information
typedef struct {
    int iError; // ERR_NONE if basic fan test passes (uses err_codes.h)
    int iSpeed; // placeholder for set speed as percent full speed; 0 off, 100 is full on
    int iRPM; // placeholder for recent measured speed as percent of full speed
} S_FAN_STATUS_T;

#include "err_codes.h"

/***  GLOBAL FUNCTION PROTOTYPES ***/
extern int usermain(void);

extern void InitFanStatus(uint8_t uiFan);
extern int iTestFan(uint8_t uiFan);
extern void SetFanError(uint8_t uiFan, int iError);

#ifdef ENABLE_EMC_TEST_FUNCTIONS
extern void SetEMCTest(void);
#endif // #ifdef ENABLE_EMC_TEST_FUNCTIONS

#if (HW_ATSAMG55_XP_BOARD == HARDWARE_PLATFORM)
extern void cb_TIMER_TC0_task0(void);
extern void Dummy_Handler_Msg(void);
#endif // #if (HW_ATSAMG55_XP_BOARD == HARDWARE_PLATFORM)

#endif /* ifndef _USERMAIN_H_INCLUDED */
