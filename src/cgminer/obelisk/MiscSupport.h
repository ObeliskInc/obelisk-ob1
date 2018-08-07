/**
 * \file MiscSupport.h
 *
 * \brief Miscellaneous Support functions related declaration.
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

#ifndef _MISC_SUPPORT_H_INCLUDED
#define _MISC_SUPPORT_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>

#include <signal.h>
#include <time.h>

//NOTE: HBFuncTest must be defined for hashboard functional testing
// #define HB_FUNC_TEST 1

/***  ASSOCIATED STRUCTURES/ENUMS      ***/

/***   GLOBAL DATA DECLARATIONS     ***/
// Time Flags
extern bool dsFlag;
extern bool TenthSecFlag;
extern bool OneSecFlag;
extern bool FiveSecFlag;

/***   GLOBAL FUNCTION PROTOTYPES   ***/
extern void EndSwap16bit(uint16_t* puiValue);
extern void EndSwap32bit(uint32_t* pulValue);
extern void EndSwap64bit(uint64_t* pullValue);
extern uint16_t uiArrayToUint16(uint8_t* puiaValue, bool bEndianMSBFirst);
extern uint32_t uiArrayToUint32(uint8_t* puiaValue, bool bEndianMSBFirst);
extern uint64_t uiArrayToUint64(uint8_t* puiaValue, bool bEndianMSBFirst);
extern void Uint16ToArray(uint16_t uiData, uint8_t* puiaValue, bool bEndianSwap);
extern void Uint32ToArray(uint32_t uiData, uint8_t* puiaValue, bool bEndianSwap);
extern void Uint64ToArray(uint64_t uiData, uint8_t* puiaValue, bool bEndianSwap);

#ifdef HB_FUNC_TEST
//Simple Hashboard Function Testing Flag accessor functions
extern void set_HBFuncTestFlag(bool fState);
extern bool get_HBFuncTestFlag(void);
extern void set_HBFuncTestErrorFlag(bool fState);
extern bool get_HBFuncTestErrorFlag(void);
#endif //HB_FUNC_TEST

//Simple Timer Flag accessor functions
//should be set in dsTimerCB
//expected to be read during Usermain's forever loop
extern void set_dsFlag(bool fState);
extern bool get_dsFlag(void);
extern void set_OneSecFlag(bool fState);
extern bool get_OneSecFlag(void);
extern void set_TenthSecFlag(bool fState);
extern bool get_TenthSecFlag(void);
extern void set_FiveSecFlag(bool fState);
extern bool get_FiveSecFlag(void);

// Time Functions
extern int create_timer(long long timer_value, void (*p_function)(int, siginfo_t*, void*));
extern int start_dsTimer(void);
extern void delay_us(uint16_t uSec);
extern void delay_ms(uint16_t mSec);

static __inline __uint16_t
__bswap16(__uint16_t _x)
{

    return ((__uint16_t)((_x >> 8) | ((_x << 8) & 0xff00)));
}

static __inline __uint32_t
__bswap32(__uint32_t _x)
{

    return ((__uint32_t)((_x >> 24) | ((_x >> 8) & 0xff00) | ((_x << 8) & 0xff0000) | ((_x << 24) & 0xff000000)));
}

static __inline __uint64_t
__bswap64(__uint64_t _x)
{

    return ((__uint64_t)((_x >> 56) | ((_x >> 40) & 0xff00) | ((_x >> 24) & 0xff0000) | ((_x >> 8) & 0xff000000) | ((_x << 8) & ((__uint64_t)0xff << 32)) | ((_x << 24) & ((__uint64_t)0xff << 40)) | ((_x << 40) & ((__uint64_t)0xff << 48)) | ((_x << 56))));
}

#endif /* ifndef _MISC_SUPPORT_H_INCLUDED */
