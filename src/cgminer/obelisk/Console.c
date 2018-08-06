/**
 * \file Console.h
 *
 * \brief Header for Console.c
 * \date STARTED: 2017/09/29
 *
 * \section Legal
 * **Copyright Notice:**
 * Copyright (c) 2017 All rights reserved.
 *
 * **Proprietary Notice:**
 *       This document contains proprietary information and neither the document
 *       nor said proprietary information shall be published, reproduced, copied,
 *       disclosed or used for any purpose other than the consideration of this
 *       document without the expressed written permission of a duly authorized
 *       representative of said Company.
 *
 */
#ifndef _CONSOLE_H_INCLUDED
#define _CONSOLE_H_INCLUDED

#define DEBUG_CONSOLE true // define to enable messages to debug console uart

// #define ATSAMG55 true   // for testing with SAMG55 (undefine for SAMA5D27)

#ifdef DEBUG_CONSOLE // don't need any of this if we don't have console support
// #warning "DEBUG_CONSOLE ENABLED"
#endif // #ifdef DEBUG_CONSOLE

/***   INCLUDE FILES               ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/***   GLOBAL DEFINITIONS          ***/
// CONSOLE_LINE_SIZE controls buffer sizes for console output
#define CONSOLE_LINE_SIZE 1000
#define DOT_CHAR '.'
#define STAR_CHAR '*'

// Defines for console message prefix tokens; callers can prepend these if desired to a message.
// These are intended to aid machine parsing by adding special prefix sequences.
#define MSG_TKN_INFO "-I- "
#define MSG_TKN_ERROR "-E- "
#define MSG_TKN_INDENT4 "    "

#ifdef DEBUG_CONSOLE
// Macros for console output support
// pstr is ptr to the string to send to console
// Not checking any return values here. Assuming we don't get buffer full.  Worst case is
// that output would be lost.

// Output to console; iUse this if the console output handler is periodically being
// called and is sending out the buffer.
#define CONSOLE_OUTPUT(pstr) ({ \
    (void)printf(pstr);         \
})

// Immediate output to console; includes flush of output buffer.
// Use this if handler function is not being called periodically (e.g. during startup)
/*#define CONSOLE_OUTPUT_IMMEDIATE(pstr) ({\
    (void)printf(pstr); \
    ConsoleOutFlush(); \
    })*/
#define CONSOLE_OUTPUT_IMMEDIATE(pstr) ({ \
    (void)printf(pstr);                   \
})

#else // #ifdef DEBUG_CONSOLE
// Macros for console output; are defined as null if debug console is disabled.
#define CONSOLE_OUTPUT(pstr) ""
#define CONSOLE_OUTPUT_IMMEDIATE(pstr) ""

#endif // #ifdef DEBUG_CONSOLE

#ifdef ATSAMG55
/***  GLOBAL FUNCTION PROTOTYPES ***/
extern void DebugConsoleInitialize(void);
extern bool bStringToConsole(char* pzString);
extern bool bPutCToConsole(char* pcChar);
extern int iConsoleOutHandler(void);
extern void ConsoleOutFlush(void);
extern int iConsoleInHandler(void);
#endif // #ifdef ATSAMG55

/***  GLOBAL DATA DECLARATIONS ***/

// Buffer for general public use for string building
// Take care that this is used monotonically. Shared use by functions
// may result in issues.  The intent is that small strings are built and pushed into
// the console ring buffer.
#ifdef DEBUG_CONSOLE
static char caStringVar[CONSOLE_LINE_SIZE];
#endif
FILE* dbgLog;
#endif /* ifndef _CONSOLE_H_INCLUDED */
