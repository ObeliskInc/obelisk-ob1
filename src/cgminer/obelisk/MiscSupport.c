/**
 * \file MiscSupport.c
 *
 * \brief Miscellaneous Support functions
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
 * \section Purpose
 * Some general use functions
 * - Endian swap wrapper functions
 * - Convert 16, 32, 64 bit type uints to byte arrays
 *
 * \section Module History
 * - 20180521 Rev 0.1 Started; using AS7 (build 7.0.1417)
 *
 */

/***    INCLUDE FILES       ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
//#include <machine/_endian.h>

//#include "atmel_start.h"
//#include "atmel_start_pins.h"

#include "MiscSupport.h"
#include "gpio_bsp.h"

#define CLOCKID CLOCK_REALTIME
#define SIG SIGRTMIN
/***    LOCAL DEFINITIONS     ***/
#define MAX_MS_COUNT 60000 //Maximum timer: 1 minute

/***    LOCAL STRUCTURES/ENUMS      ***/

/***    LOCAL FUNCTION PROTOTYPES   ***/
void dsTimerCB(int sig, siginfo_t* si, void* uc);
#ifdef ENABLE_EMC_TEST_FUNCTIONS
//static void SetEMCTest(void);

#endif // #ifdef ENABLE_EMC_TEST_FUNCTIONS

/***    LOCAL DATA DECLARATIONS     ***/
#ifdef HB_FUNC_TEST
static bool HBFuncTestFlag = true;
static bool HBFuncTestErrorFlag = false;
#endif //HB_FUNC_TEST
static uint16_t dsCount = 1; //tenth of second count

/***    GLOBAL DATA DECLARATIONS   ***/
bool dsFlag = false;
bool TenthSecFlag = false;
bool OneSecFlag = false;
bool FiveSecFlag = false;
/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/

/** *************************************************************
 * \brief Endian swap 16-bit; wrapper function so we can easily change
 * behavior in the future for porting or experimentation.
 * \param pullValue is ptr to 16-bit (typ unsigned int) to swap
 * \return none
 */
void EndSwap16bit(uint16_t* puiValue)
{
    *puiValue = __bswap16(*puiValue);
} // EndSwap16bit()

/** *************************************************************
 * \brief Endian swap 32-bit; wrapper function so we can easily change
 * behavior in the future for porting or experimentation.
 * \param pullValue is ptr to 32-bit (typ unsigned long) to swap
 * \return none
 */
void EndSwap32bit(uint32_t* pulValue)
{
    *pulValue = __bswap32(*pulValue);
} // EndSwap32bit()

/** *************************************************************
 * \brief Endian swap 64-bit; wrapper function so we can easily change
 * behavior in the future for porting or experimentation.
 * \param pullValue is ptr to 64-bit (typ. unsigned long long) to swap
 * \return none
 */
void EndSwap64bit(uint64_t* pullValue)
{
    *pullValue = __bswap64(*pullValue);
} // EndSwap64bit()

/** *************************************************************
 * \brief uiArrayToUint16; converts a 2-byte array of 8-bit values
 * into a 16-bit unsigned value; supports choice of endian order.
 * Should be usable for signed ints also.
 * \param puiaValue is ptr to bytes (unsigned int array) to convert
 * \param bEndianSwap is true to swap byte order
 * \return uint16_t converted value
 */
uint16_t uiArrayToUint16(uint8_t* puiaValue, bool bEndianSwap)
{
    uint16_t uiResult;
    uint16_t* pui16Value;

    uiResult = (uint16_t)*puiaValue;
    uiResult |= (uint16_t) * (puiaValue + 1) << 8;

    if (true == bEndianSwap) { // swap endian; network byte order/big-endian <-> little endian
        EndSwap16bit(&uiResult);
    }
    return (uiResult);
} // uiArrayToUint16()

/** *************************************************************
 * \brief uiArrayToUint32; converts a 4-byte array of 8-bit values
 * into a 32-bit unsigned value; supports optional swap of endian order.
 * Should be usable for signed ints also.
 * \param puiaValue is ptr to bytes (unsigned int array) to convert
 * \param bEndianSwap is true to swap byte order
 * \return uint32_t converted value
 */
uint32_t uiArrayToUint32(uint8_t* puiaValue, bool bEndianSwap)
{
    uint32_t ulResult = 0;
    int ixI;

    for (ixI = 0; ixI < sizeof(uint32_t); ixI++) {
        ulResult |= (uint32_t) * (puiaValue + ixI) << (8 * ixI);
    }

    if (true == bEndianSwap) { // swap endian; network byte order/big-endian <-> little endian
        EndSwap32bit(&ulResult);
    }
    return (ulResult);
} // uiArrayToUint32()

/** *************************************************************
 * \brief uiArrayToUint64; converts an 8-byte array of 8-bit values
 * into a 64-bit unsigned value; supports choice of endian order.
 * Should be usable for signed ints also.
 * \param puiaValue is ptr to bytes (unsigned int array) to convert
 * \param bEndianSwap is true to swap byte order
 * \return uint64_t converted value
 */
uint64_t uiArrayToUint64(uint8_t* puiaValue, bool bEndianSwap)
{
    uint64_t ullResult = 0;
    int ixI;

    for (ixI = 0; ixI < sizeof(uint64_t); ixI++) {
        ullResult |= (uint64_t) * (puiaValue + ixI) << (8 * ixI);
    }

    if (true == bEndianSwap) { // swap endian; network byte order/big-endian <-> little endian
        EndSwap64bit(&ullResult);
    }
    return (ullResult);
} // uiArrayToUint64()

/** *************************************************************
 * \brief Uint16ToArray; converts a 16-bit value into a 2-byte array of
 * 8-bit values; supports choice of endian order.
 * \param uiData is uint16_t data value to convert
 * \param puiaValue is ptr to bytes (uint8_t array) to put the values
 * \param bEndianSwap is true to swap byte order
 * \return none
 */
void Uint16ToArray(uint16_t uiData, uint8_t* puiaValue, bool bEndianSwap)
{
    uint16_t uiResult = uiData;

    if (true == bEndianSwap) { // swap endian; network byte order/big-endian <-> little endian
        EndSwap16bit(&uiResult);
    }
    puiaValue[0] = (uint8_t)(uiResult & 0xFF);
    puiaValue[1] = (uint8_t)((uiResult >> 8) & 0xFF);

} // Uint16ToArray()

/** *************************************************************
 * \brief Uint32ToArray; converts a 32-bit value into a 4-byte array of
 * 8-bit values; supports choice of endian order.
 * \param uiData is uint32_t data value to convert
 * \param puiaValue is ptr to bytes (uint8_t array) to put the values
 * \param bEndianSwap is true to swap byte order
 * \return none
 */
void Uint32ToArray(uint32_t uiData, uint8_t* puiaValue, bool bEndianSwap)
{
    uint32_t uiResult = uiData;

    if (true == bEndianSwap) { // swap endian; network byte order/big-endian <-> little endian
        EndSwap32bit(&uiResult);
    }
    puiaValue[0] = (uint8_t)(uiResult & 0xFF);
    puiaValue[1] = (uint8_t)((uiResult >> 8) & 0xFF);
    puiaValue[2] = (uint8_t)((uiResult >> 16) & 0xFF);
    puiaValue[3] = (uint8_t)((uiResult >> 24) & 0xFF);

} // Uint32ToArray()

/** *************************************************************
 * \brief Uint64ToArray; converts a 64-bit value into an 8-byte array of
 * 8-bit values; supports choice of endian order.
 * \param uiData is uint64_t data value to convert
 * \param puiaValue is ptr to bytes (uint8_t array) to put the values
 * \param bEndianSwap is true to swap byte order
 * \return none
 */
void Uint64ToArray(uint64_t uiData, uint8_t* puiaValue, bool bEndianSwap)
{
    uint64_t uiResult = uiData;

    if (true == bEndianSwap) { // swap endian; network byte order/big-endian <-> little endian
        EndSwap64bit(&uiResult);
    }
    puiaValue[0] = (uint8_t)(uiResult & 0xFF);
    puiaValue[1] = (uint8_t)((uiResult >> 8) & 0xFF);
    puiaValue[2] = (uint8_t)((uiResult >> 16) & 0xFF);
    puiaValue[3] = (uint8_t)((uiResult >> 24) & 0xFF);
    puiaValue[4] = (uint8_t)((uiResult >> 32) & 0xFF);
    puiaValue[5] = (uint8_t)((uiResult >> 40) & 0xFF);
    puiaValue[6] = (uint8_t)((uiResult >> 48) & 0xFF);
    puiaValue[7] = (uint8_t)((uiResult >> 56) & 0xFF);

} // Uint64ToArray()

/*	TIMER Functions
EXAMPLE timer timeout_event:

static void timeout_event(int sig, siginfo_t *si, void *uc)
{
    printf("timer expired\n");
}

create_timer(, void (
*/

int create_timer(long long timer_value, void (*p_function)(int, siginfo_t*, void*))
{
    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;
    sigset_t mask;
    struct sigaction sa;

    /* Establish handler for timer signal */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = p_function;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIG, &sa, NULL) == -1) {
        printf("timer create sigaction failed\n");
        return -1;
    }
    sigemptyset(&mask);
    sigaddset(&mask, SIG);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
        printf("timer create sigprocmask failed\n");
        return -1;
    }

    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIG;
    sev.sigev_value.sival_ptr = &timerid;
    timer_create(CLOCKID, &sev, &timerid);

    /* Start the timer */
    its.it_value.tv_sec = timer_value / 1000000000;
    its.it_value.tv_nsec = timer_value % 1000000000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        printf("timer create settime error\n");
        return -1;
    }

    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
        printf("timer create sigprocmask error\n");
        return -1;
    }

    printf("-I- Created Timer\n");

    return 0;
}

//Initializes and starts the tenth of a second timer
//Callback is dsTimerCB
int start_dsTimer(void)
{
    return create_timer(100000000, dsTimerCB);
}

void set_dsFlag(bool fState)
{
    dsFlag = fState;
}

bool get_dsFlag(void)
{
    return dsFlag;
}

void set_TenthSecFlag(bool fState)
{
    TenthSecFlag = fState;
}

bool get_TenthSecFlag(void)
{
    return TenthSecFlag;
}

void set_OneSecFlag(bool fState)
{
    OneSecFlag = fState;
}

bool get_OneSecFlag(void)
{
    return OneSecFlag;
}

void set_FiveSecFlag(bool fState)
{
    FiveSecFlag = fState;
}

bool get_FiveSecFlag(void)
{
    return FiveSecFlag;
}

//NOTE: Tenth of a second timer callback
void dsTimerCB(int sig, siginfo_t* si, void* uc)
{
    //start timer maintenance
    dsCount++;

    if ((MAX_MS_COUNT <= dsCount) || (1 >= dsCount)) {
        dsCount = 1;
    }
    //end timer maintenance

    //start .1 sec tasks

    set_dsFlag(true);
    //end .1 sec tasks

    set_TenthSecFlag(true);

    //start .5 sec tasks
    if (0 == (dsCount % 5)) {
#ifdef HB_FUNC_TEST
        if (HBFuncTestFlag) //during HB functional testing alternate LEDs
        {
            led_alternate();
        }
#endif //HB_FUNC_TEST

    } //end .5 sec tasks

    //start 1 sec tasks
    if (0 == (dsCount % 5)) {
#ifdef HB_FUNC_TEST
        if (false == HBFuncTestFlag) {
            if (false == HBFuncTestErrorFlag) {
                led_red_off();
                led_green_toggle();
            } else {
                led_green_off();
                led_red_toggle();
            }
        }
#endif //HB_FUNC_TEST

        set_OneSecFlag(true);
    } //end 1 sec tasks

    //start 5 sec tasks
    if (0 == (dsCount % 50)) {
        set_FiveSecFlag(true);
    } //end 5 sec tasks
}

#ifdef HB_FUNC_TEST
void set_HBFuncTestFlag(bool fState)
{
    HBFuncTestFlag = fState;
}

bool get_HBFuncTestFlag(void)
{
    return HBFuncTestFlag;
}

void set_HBFuncTestErrorFlag(bool fState)
{
    HBFuncTestErrorFlag = fState;
}

bool get_HBFuncTestErrorFlag(void)
{
    return HBFuncTestErrorFlag;
}
#endif //HB_FUNC_TEST

//NOTE: Delay Functions

void delay_us(uint16_t uSec)
{
    usleep(uSec);
}

void delay_ms(uint16_t mSec)
{
    usleep(mSec * 1000);
}