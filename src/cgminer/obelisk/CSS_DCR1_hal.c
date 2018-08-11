/**
 * \file CSS_DCR1_hal.c
 *
 * \brief CSS ASIC DCR1 firmware device driver
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
 * Implements the mid/low level device drivers for the CSS DCR1 ASIC access and control via SPI interface.
 * Refer to CSS Blake256 data sheet for more details of the device.
 * This is using the master asynchronous type SPI interface (i.e. using callbacks as a SPI bus master).
 *
 * \section Module History
 * - 20180706 Rev 0.1 Started; using AS7 (build 7.0.1417)
 *
 */

/***    INCLUDE FILES       ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
// #include <inttypes.h>   // PRIxx type printf; 64-bit type support

//#include "Usermain.h"
#include "Usermain.h"

#ifndef HARDWARE_PLATFORM
#error "HARDWARE_PLATFORM not defined"
#endif

#if (HW_ATSAMG55_XP_BOARD != HARDWARE_PLATFORM)
// Initialization/declarations for SAMA5D27
#include <unistd.h>

#include "Console.h" // development console/uart support
#include "SPI_Support.h"
#include "MiscSupport.h" // endian and other stuff
#include "MCP23S17_hal.h"
#include "CSS_DCR1Defines.h"
#include "CSS_DCR1_hal.h"
#include "EMCTest.h"

#else
// Initialization/declarations for ATSAMG55
#include "atmel_start.h"
#include "atmel_start_pins.h"
//#include "ApplicationFiles\Usermain.h"
#include "ApplicationFiles\Support\Console\Console.h" // development console/uart support
#include "ApplicationFiles\BSP\SPI_Support\Spi_Support.h"
#include "ApplicationFiles\Support\Misc_Support\MiscSupport.h" // endian and other stuff
#include "ApplicationFiles\BSP\MCP23S17\MCP23S17_hal.h"
#include "ApplicationFiles\BSP\DCR1ASIC\CSS_DCR1Defines.h"
#include "ApplicationFiles\BSP\DCR1ASIC\CSS_DCR1_hal.h"

#endif // #if (HW_ATSAMG55_XP_BOARD != HARDWARE_PLATFORM)

/***    LOCAL DEFINITIONS     ***/
#define DCR1_TRANSFER_CONTROL_BYTES (3) // SPI transfer has 3 bytes of control
#define DCR1_TRANSFER_DATA_BYTES (4) // SPI transfer has 4 bytes of data (32 bit)
#define DCR1_TRANSFER_BYTE_COUNT (DCR1_TRANSFER_CONTROL_BYTES + DCR1_TRANSFER_DATA_BYTES) // num of bytes per SPI transaction

#define DCR1_READ_BUFF_SIZE 8 // max bytes size we expect to have to transfer to the device (plus a little more)
#define DCR1_WRITE_BUFF_SIZE DCR1_READ_BUFF_SIZE

#define OCRA_CORE15_CLKENB (DCR1_OCR_CORE15 | DCR1_OCRA_INIT_VAL) // core 15 enable of the osc group
#define OCRB_CORE15_CLKENB (DCR1_OCRB_STARTUP_VAL)
#define NONCE_BITS_MASK (0x7FFF) // port expander valid bits
#define DONE_BITS_MASK (0x7FFF) // port expander valid bits

// Sample jobs provided in the DCR1 datasheet
#define MAX_SAMPLE_JOBS 2
// Job sample #1; (modifed from datasheet; original datasheet values as comments)
#define DCR_SAMPLE1_MATCH (0xE2B2CC34)
#define DCR_SAMPLE1_V00 (0x51F87D90)
#define DCR_SAMPLE1_V01 (0x660D07D4)
#define DCR_SAMPLE1_V02 (0xA697C2B7)
#define DCR_SAMPLE1_V03 (0xC52BB4EF)
#define DCR_SAMPLE1_V04 (0xBB2C0B5E)
#define DCR_SAMPLE1_V05 (0x41B8CB4C)
#define DCR_SAMPLE1_V06 (0x9254A116)
#define DCR_SAMPLE1_V07 (0xE2B2CC34)
#define DCR_SAMPLE1_V08 (0x243F6A88)
#define DCR_SAMPLE1_V09 (0x85A308D3)
#define DCR_SAMPLE1_V10 (0x13198A2E)
#define DCR_SAMPLE1_V11 (0x03707344)
#define DCR_SAMPLE1_V12 (0xA4093D82)
#define DCR_SAMPLE1_V13 (0x299F3470)
#define DCR_SAMPLE1_V14 (0x082EFA98)
#define DCR_SAMPLE1_V15 (0xEC4E6C89)

#define DCR_SAMPLE1_LB (0x64EE1000) // (0x64EE1D70)
#define DCR_SAMPLE1_UB (0x64EE1DB0)
#define DCR_SAMPLE1_M0 (0x6A5C0200)
#define DCR_SAMPLE1_M1 (0x39200000)
#define DCR_SAMPLE1_M2 (0x74017B59)
#define DCR_SAMPLE1_M4 (0x00000005)
#define DCR_SAMPLE1_M5 (0x00000000)
#define DCR_SAMPLE1_M6 (0x00000000)
#define DCR_SAMPLE1_M7 (0x00000000)
#define DCR_SAMPLE1_M8 (0x00000000)
#define DCR_SAMPLE1_M9 (0x00000000)
#define DCR_SAMPLE1_M10 (0x00000000)
#define DCR_SAMPLE1_M11 (0x00000000)
#define DCR_SAMPLE1_M12 (0x04000000)
#define DCR_SAMPLE1_FDR0 (0x64EE1D76)

// Job sample #2
#define DCR_SAMPLE2_MATCH (0xFA547D3B)
#define DCR_SAMPLE2_V00 (0xD496F8A3)
#define DCR_SAMPLE2_V01 (0x36CCF4C9)
#define DCR_SAMPLE2_V02 (0xC264E664)
#define DCR_SAMPLE2_V03 (0x7E3CBC93)
#define DCR_SAMPLE2_V04 (0xD35E50AC)
#define DCR_SAMPLE2_V05 (0x15615B42)
#define DCR_SAMPLE2_V06 (0x20422DAD)
#define DCR_SAMPLE2_V07 (0xFA547D3B)
#define DCR_SAMPLE2_V08 (0x243F6A88)
#define DCR_SAMPLE2_V09 (0x85A308D3)
#define DCR_SAMPLE2_V10 (0x13198A2E)
#define DCR_SAMPLE2_V11 (0x03707344)
#define DCR_SAMPLE2_V12 (0xA4093D82)
#define DCR_SAMPLE2_V13 (0x299F3470)
#define DCR_SAMPLE2_V14 (0x082EFA98)
#define DCR_SAMPLE2_V15 (0xEC4E6C89)

#define DCR_SAMPLE2_LB (0x6E83D6C0)
#define DCR_SAMPLE2_UB (0x6E83D6F0)
#define DCR_SAMPLE2_M0 (0x01000000)
#define DCR_SAMPLE2_M1 (0x71BB0100)
#define DCR_SAMPLE2_M2 (0x27D8B856)
#define DCR_SAMPLE2_M4 (0x382A0936)
#define DCR_SAMPLE2_M5 (0xED7F1777)
#define DCR_SAMPLE2_M6 (0x43000000)
#define DCR_SAMPLE2_M7 (0x00000000)
#define DCR_SAMPLE2_M8 (0x00000000)
#define DCR_SAMPLE2_M9 (0x00000000)
#define DCR_SAMPLE2_M10 (0x00000000)
#define DCR_SAMPLE2_M11 (0x00000000)
#define DCR_SAMPLE2_M12 (0x00000000)
#define DCR_SAMPLE2_FDR0 (0x6E83D6DA)

/***    LOCAL STRUCTURES/ENUMS      ***/

/***    LOCAL FUNCTION PROTOTYPES   ***/

static int iDCR1CmdPreLoadJob(S_DCR1_JOB_T* psDCR1Job);
static int iDCR1CmdStartJob(void);
static int iDCR1CmdLoadRange(S_DCR1_JOB_T* psDCR1Job);
static int iDCR1CmdDataValid(void);
static void DCR1CmdReadComplete(void);

static int iDCR1TestJobVerify(uint32_t* puiSolution);

static void DCR1GetTestJob(uint8_t uiSampleNum);

static int iDCR1EMCDataValid(void);
static int iDCR1EMCTestJobVerify(uint64_t* puiSolution);
static int iDCR1EMCStartJob(void);

/***    LOCAL DATA DECLARATIONS     ***/
static S_DCR1_JOB_T sDCR1JobBuf;
static S_DCR1_TRANSFER_T sDCR1XferBuf; // Local buffer for SPI transfer to/from ASIC

static uint8_t ucaDCR1OutBuf[DCR1_WRITE_BUFF_SIZE]; // buffer for sending out
static uint8_t ucaDCR1InBuf[DCR1_READ_BUFF_SIZE]; // buffer for reading in

static const uint64_t ullVRegTable[16] = {
    DCR1_V00_VAL,
    DCR1_V01_VAL,
    DCR1_V02_VAL,
    DCR1_V03_VAL,
    DCR1_V04_VAL,
    DCR1_V05_VAL,
    DCR1_V06_VAL,
    DCR1_V07_VAL,
    DCR1_V08_VAL,
    DCR1_V09_VAL,
    DCR1_V10_VAL,
    DCR1_V11_VAL,
    DCR1_V12_VAL,
    DCR1_V13_VAL,
    DCR1_V14_VAL,
    DCR1_V15_VAL
};

/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/

//  CSS_DCR1_REVA_WORKAROUND needs to be defined to control how the asics are read
#ifndef CSS_DCR1_REVA_WORKAROUND
#error "CSS_DCR1_REVA_WORKAROUND not defined"
#endif

/** *************************************************************
 *  \brief DCR1 type ASIC String Startup Method
 *
 *  Called at startup to do first stage initialization of the ASIC(s).  Might possibly be called as
 *  part of restart of a string.
 *  Assumes the associated SPI port is initialized. Due to variability of each ASIC this
 *  does not use the multicast method but instead tries each ASIC sequentially. For each hash board:
 *  1) Initialize STARTCLK to off (high); just in case it is active from a prior operation (as when restarting
 *     a string).
 *  2) Sets an initial string voltage.  Assumes the voltage divides roughly evenly between the ASICs in the
 *     string.  For example for 9V string, each ASIC should have about 0.6V.
 *     - Uses the voltage monitor to verify that the proper voltage is generated and the measured voltages
 *       on the bottom and top ASICs are within expected ranges.
 *     - Purpose; verify ASIC string voltage generation and that the string is operational (current is flowing).
 *     - Report results to console
 *     - Assuming success goes on to startup the ASICs; else stops and reports failure to init
 *  2) Attempts to get communications working to the ASICs by writing to a test register (OCR) followed by read-back to
 *     verify for each ASIC.
 *     - Results are logged in boolean flag word (16-bit value per string/hash board) and counter.
 *     - Startup e-pot setting logged to an array; this can be used later to sense order of startup and which asics are the later ones
 *       to start; would like to see if this has use in string balancing later.
 *     - If could not get all ASICs to pass, step up the string voltage and retest all again until passing or cannot
 *       increase the string voltage further.
 *     - Report final results at completion of this phase of startup.
 *  3) After all ASICs tested
 *     - If all passed (test flags are 0x7FFF), then go on to a more extensive write/read test and then initialize the ASICs for operation
 *     - If all failed (test flags are 0), likely a problem with SPI communications or ASIC string power.
 *       Report the problem as failure to initialize.
 *     - If some but not all passed; likley not a general SPI communications or string power problem; perhaps the
 *       failed ASICs are still at too low a voltage (string is unbalanced) or a component related to SPI comms on that ASIC
 *       have a problem.  Need to develop a plan for this:
 *
 *  At the end point, the chips/cores for the board should be set up but clocking to cores has not
 *    yet been enabled.  The cores are ready to have clocking enabled and jobs loaded.
 *
 *  \param int uiBoard sets which board to configure; 0 to MAX_NUMBER_OF_HASH_BOARDS-1
 *  \return int status of operation (see err_codes.h or usermain.h)
 */
int iDCR1StringStartup(uint8_t uiBoard)
{
#define OCR_CONFIG_MAX_RETRIES 1
// Set EPOT_STARTUP_VALUE to a high value (low string voltage) to see how the string starts up. This should allow you to see
// the e-pot value as each asic starts passing the test.
#define EPOT_STARTUP_VALUE 30 // will auto-step up voltage using this as starting value of epot (100 = about 8V)
#define EPOT_LOW_LIMIT 17 // exclusive limit (typical range 12 to 127)
#if (12 > EPOT_LOW_LIMIT) // below about this we cannot get V15IO to work properly
#error "limit of EPOT is 12"
#endif

#define ASIC_TEST_FLAGS_PASS 0x7FFF

    bool bDone;
    uint8_t ixJ;
    uint8_t uiRetryCnt;
    uint8_t uiUUT, uiEpotCount;
    uint16_t uiTestData;
    uint16_t uiAsicTestFlags, uiAsicAwakeCnt;
    int iResult = ERR_NONE;

    uiUUT = (uint8_t)uiBoard;
    iResult = iIsBoardValid(uiUUT, true); // board must be there to startup and should have passed initial testing
    if (ERR_NONE == iResult) {
        iSetBoardStatus(uiUUT, ERR_NOT_INITIALIZED);

        iResult = iSetHashClockEnable(uiUUT, false); // ensure clock enable is off
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d hash clock disable Error\r\n", uiUUT + 1);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            // Set initial string voltage
            // #TODO; for now just set it to a fixed voltage close to what we want until we get the voltage monitor and
            // possibly a control loop running
            uiEpotCount = EPOT_STARTUP_VALUE;
            iResult = iSetPSControl(uiUUT, uiEpotCount);
            if (ERR_NONE != iResult) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d power supply control error\r\n", uiUUT + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            } else {
                iResult = iSetPSEnable(uiUUT, true); // turn on the string
                if (ERR_NONE != iResult) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d power supply enable error\r\n", uiUUT + 1);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                }
            } // if (ERR_NONE != iResult)
        } // if (ERR_NONE != iResult)

        if (ERR_NONE == iResult) { // if no errors from above...
            ReportSysVoltages(uiUUT);

            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "HB%d Establish string communications", uiUUT + 1);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

            // Write and verify we can program the oscillator control register on each ASIC in the string.
            // The OCR is critical to operation since it controls the clock rate (and power use) of the chip in the string.
            uiAsicAwakeCnt = 0;
            uiAsicTestFlags = 0; // track which pass the test
            bDone = false;
            while (false == bDone) {

                for (ixJ = 0; ixJ <= LAST_DCR1_CHIP; ixJ++) {
                    uiRetryCnt = 0;
                    do { // while (OCR_CONFIG_MAX_RETRIES > ++uiRetryCnt);
                        if (1 <= uiRetryCnt) { // put out '-' if a retry
                            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "."); // indicate a retry
                            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        }
                        iResult = iDCR1StartupOCRInitValue(uiUUT, ixJ);
                        if (ERR_NONE == iResult) {
                            break; // break do..while
                        } else if (ERR_FAILURE == iResult) {
                            // don't bother with retry, value was not even close to correct
                            break; // break do..while
                        }
                    } while (OCR_CONFIG_MAX_RETRIES > ++uiRetryCnt);

                    if (ERR_NONE == iResult) {
                        uiTestData = 0x1 << ixJ;
                        if (uiTestData != (uiAsicTestFlags & uiTestData)) {
                            uiAsicAwakeCnt++; // increase count if flag was not already set
                            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_INDENT4 "HB%d U:%02d = %-3d (0x%04x)",
                                (uiUUT + 1), (ixJ + 1), uiEpotCount, (uiAsicTestFlags | uiTestData));
                            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        }
                        uiAsicTestFlags |= uiTestData; // set the flag bit for the ASIC that passed test
                    } else {
                        uiTestData = 0x1 << ixJ;
                        if (uiTestData == (uiAsicTestFlags & uiTestData)) {
                            uiAsicAwakeCnt--; // decrease count if flag was already set
                            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "x"); // indicate we lost one
                            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        }
                        uiAsicTestFlags &= ~uiTestData; // clear the flag bit for the ASIC that failed test
                    }
                } // for (ixJ = 0; ixJ <= LAST_DCR1_CHIP; ixJ++)

                // Check how many asics have begun to work.
                if (MAX_DCR1_CHIPS_PER_STRING <= uiAsicAwakeCnt) { // if all passed we are done.
                    bDone = true;
                } else { // if not then increase string voltage and retry; until limit of test is reached
                    if (EPOT_LOW_LIMIT >= (uiEpotCount - 1)) {
                        bDone = true;
                    } else {
                        --uiEpotCount;
                        iResult = iSetPSControl(uiUUT, uiEpotCount); // step the voltage up and retry
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "+"); // indicate we stepped up voltage
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    }
                } // if (MAX_DCR1_CHIPS_PER_STRING <= uiAsicAwakeCnt)

            } // while (false == bDone)

            if (ASIC_TEST_FLAGS_PASS != uiAsicTestFlags) {
                (void)iSetPSEnable(uiUUT, false); // turn off the string on failure to launch all OCRs
                iSetBoardStatus(uiUUT, ERR_FAILURE);
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d OCR initialize FAIL (%d of %d)\r\n",
                    (uiUUT + 1), (MAX_DCR1_CHIPS_PER_STRING - uiAsicAwakeCnt), MAX_DCR1_CHIPS_PER_STRING);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                for (ixJ = 0; ixJ <= LAST_DCR1_CHIP; ixJ++) {
                    uiTestData = 0x1 << ixJ;
                    if (uiTestData != (uiAsicTestFlags & uiTestData)) {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "Chip %d failed initialization\r\n", ixJ + 1);
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    }
                } // for (ixJ = 0; ixJ <= LAST_DCR1_CHIP; ixJ++)
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n");
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            } else {
                // step up the voltage a little more so we don't leave the string just on the edge of operation
                if (EPOT_LOW_LIMIT <= (uiEpotCount - DCR1_EPOT_BOOST_COUNT)) {
                    uiEpotCount -= DCR1_EPOT_BOOST_COUNT;
                } else {
                    uiEpotCount = EPOT_LOW_LIMIT;
                }
                iResult = iSetPSControl(uiUUT, uiEpotCount);

                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n");
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                ReportSysVoltages(uiUUT); // Report string conditions at completion: voltages and control epot settings

                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_INFO "HB%d OCR initialize PASS\r\n", (uiUUT + 1));
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iSetBoardStatus(uiUUT, ERR_NONE);

                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INFO "HB%d Register access verify ", (uiUUT + 1));
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

#ifdef CHECK_M_REGISTERS_ON_INIT
                uiAsicAwakeCnt = 0;
                // an extra confidence check to confirm that SPI comms to the ASIC is working and that the ASIC has enough
                // voltage to be awake.  We don't bother with retries for this test.
                uiAsicTestFlags = 0;
                for (ixJ = 0; ixJ <= LAST_DCR1_CHIP; ixJ++) {
                    iResult = iDCR1MRegCheck(uiUUT, ixJ); // function to write and readback verify register(s)
                    if (ERR_NONE == iResult) {
                        uiTestData = 0x1 << ixJ;
                        if (uiTestData != (uiAsicTestFlags & uiTestData)) {
                            uiAsicAwakeCnt++; // increase count if flag was not already set
                        }
                        uiAsicTestFlags |= uiTestData; // set the flag bit for the ASIC that passed test
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "%X", ixJ + 1);
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    } else {
                        uiTestData = 0x1 << ixJ;
                        if (uiTestData == (uiAsicTestFlags & uiTestData)) {
                            uiAsicAwakeCnt--; // decrease count if flag was already set
                        }
                        uiAsicTestFlags &= ~uiTestData; // clear the flag bit for the ASIC that failed test
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "x");
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    }
                } // for (ixJ = 0; ixJ <= LAST_DCR1_CHIP; ixJ++)

#endif
                // Report results to console
                if (MAX_DCR1_CHIPS_PER_STRING <= uiAsicAwakeCnt) { // if we passed the above initialization...
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_INFO "HB%d string communications PASSED\r\n", uiUUT + 1);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                } else {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d: %d of %d chips FAILED write/read test (0x%x)\r\n",
                        uiUUT + 1, (MAX_DCR1_CHIPS_PER_STRING - uiAsicAwakeCnt), MAX_DCR1_CHIPS_PER_STRING, uiAsicTestFlags);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    for (ixJ = 0; ixJ <= LAST_DCR1_CHIP; ixJ++) {
                        uiTestData = 0x1 << ixJ;
                        if (uiTestData != (uiAsicTestFlags & uiTestData)) {
                            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "Chip %d failed readback\r\n", ixJ + 1);
                            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        }
                    } // for (ixJ = 0; ixJ <= LAST_DCR1_CHIP; ixJ++)
                }
            } // if (ASIC_TEST_FLAGS_PASS != uiAsicTestFlags)

        } // if (ERR_NONE == iResult)

        if (ERR_NONE == iGetBoardStatus(uiUUT)) {
            iResult = ERR_NONE;
        } else {
            iResult = ERR_FAILURE;
        }

    } // if (ERR_NONE == iResult)

    if (ERR_NONE != iResult) {
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d FAILED TO START\r\n", uiUUT + 1);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
    }

    return (iResult);
} // iDCR1StringStartup(void)

/** *************************************************************
 *  \brief Support function for the initial start up of ASICs.  Does write and
 *  read-back tests on the ASIC M-registers.  This tests the ASIC is listening and responds
 *  correctly. For example, if ASIC core voltage is too low then it will not read back
 *  data written to it properly.  If this fails, it should return as quickly as possible.
 *  For each register tested in the chip, write to all cores and read back the register to
 *  verify the data. Will retry the read but not retry the write.
 *
 *  \param uint8_t uiHashBoard sets which board to configure
 *  \param uint8_t uiChip determines which ASIC to configure; 0 to LAST_DCR1_CORE
 *  \return int status of operation (see err_codes.h)
 */
int iDCR1MRegCheck(uint8_t uiHashBoard, uint8_t uiChip)
{
#define TEST_PASSES 8
#define REG_TO_START_TEST ((uint8_t)E_DCR1_REG_M0)
#define REG_TO_END_TEST ((uint8_t)E_DCR1_REG_M2)
#define DATA_TO_START_TEST ((uint32_t)0xF00F6688)
#define CORES_TO_VERIFY MAX_NUMBER_OF_DCR1_CORES // 1 to MAX_NUMBER_OF_DCR1_CORES
#define MAX_RETRY_MREG_CHECK 5
    uint8_t uixI, uiDataValue, uixRegUnderTest;
    uint32_t uiTestDataPattern;
    int iRetry;
    int iResult = ERR_NONE;

    for (uixRegUnderTest = REG_TO_START_TEST; uixRegUnderTest <= REG_TO_END_TEST; uixRegUnderTest++) {
        if (E_DCR1_REG_M3_RSV != uixRegUnderTest) { // DCR1 M3 register is skipped

            uiTestDataPattern = DATA_TO_START_TEST;
            for (uiDataValue = 0; uiDataValue < TEST_PASSES; uiDataValue++) {
                sDCR1XferBuf.eMode = E_DCR1_MODE_CHIP_WRITE; // chip wide write should do all cores
                sDCR1XferBuf.uiBoard = uiHashBoard;
                sDCR1XferBuf.uiChip = uiChip;
                sDCR1XferBuf.uiCore = 0; // chip wide write
                sDCR1XferBuf.uiReg = uixRegUnderTest;
                sDCR1XferBuf.uiData = uiTestDataPattern;

                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);

                // and verify the values
                if (ERR_NONE == iResult) {
                    // read back the register from each asic
                    sDCR1XferBuf.eMode = E_DCR1_MODE_REG_READ; // local read from one chip and engine
                    for (uixI = 0; uixI < CORES_TO_VERIFY; uixI++) {

#if (0 != CSS_DCR1_REVA_WORKAROUND) // workaround for rev A silicon issue on reads
                        sDCR1XferBuf.uiReg = uixRegUnderTest << CSS_DCR1_REVA_WORKAROUND; // reg addr must be shifted for reads
#else
                        sDCR1XferBuf.uiReg = uixRegUnderTest;
#endif
                        sDCR1XferBuf.uiCore = uixI; // core to test

                        iRetry = 0;
                        while (MAX_RETRY_MREG_CHECK > iRetry) {
                            sDCR1XferBuf.uiData = 0; // hold MOSI fixed low during data part of transfer
                            iResult = iDCR1SpiTransfer(&sDCR1XferBuf); // Read to the chip; should block until completed

                            if (ERR_NONE != iResult) {
                                break; // break while if SPI transfer error
                            } else {

#if (0 != CSS_DCR1_REVA_WORKAROUND) // workaround for rev A silicon issue on reads
                                sDCR1XferBuf.uiData = sDCR1XferBuf.uiData >> CSS_DCR1_REVA_WORKAROUND;
#endif
                                if (uiTestDataPattern == sDCR1XferBuf.uiData) {
                                    if (0 < iRetry) {
                                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, " retry pass");
                                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                                    }
                                    break; // break while if passed
                                } else {
                                    iResult = ERR_FAILURE;
                                    if (0 == iRetry) {
                                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n-W- A%02d:C%02d:Rx%02x", uiChip + 1, uixI + 1, uixRegUnderTest);
                                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                                    } else {
                                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, ".%d", iRetry + 1);
                                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                                    }
                                    iRetry++;
                                }
                            } // if (ERR_NONE != iResult)

                        } // while (MAX_RETRY_MREG_CHECK > iRetry)

                    } // for (uixI = 0; uixI < CORES_TO_VERIFY; uixI++)

                } // if (ERR_NONE == iResult)

                if (ERR_NONE != iResult) {
                    break; // for (uiDataValue = 0
                }

                uiTestDataPattern = uiTestDataPattern >> 1;
            } // for (uiDataValue = 0; uiDataValue < TEST_PASSES; uiDataValue++)

            if (ERR_NONE != iResult) {
                break; // for (uixRegUnderTest = REG_TO_START_TEST
            }

        } // if (E_DCR1_REG_M3_RSV != uixRegUnderTest)
    } // for (uixRegUnderTest = REG_TO_START_TEST; uixRegUnderTest < REG_TO_END_TEST; uixRegUnderTest++)

    return (iResult);

} // iDCR1MRegCheck()

/** *************************************************************
 *  \brief Support function for the initial start up of ASICs.  Loads the
 *   eight oscillator control registers in the ASIC with a startup value.  Then
 *   verifies it can read the value back.  This is an integrity check to ensure we
 *   can control the oscillators and it gives them an initial defined value.  These
 *   were observed to wake up randomly and START_CLK should not be asserted until
 *   these are a set with a proper value.
 *
 *  \param uint8_t uiHashBoard sets which board to configure
 *  \param uint8_t uiChip determines which ASIC to configure; 0 to LAST_DCR1_CORE
 *  \return int status of operation (see err_codes.h)
 *  \retval ERR_NONE if passed
 *  \retval ERR_FAILURE if data read back is all 0's or all 1's
 *  \retval ERR_INVALID_DATA if data read back is mix of 1's and 0's (getting close)
 *
 * Note: The OCR is tested by CSS on each device and all asics should pass this test.
 */
int iDCR1StartupOCRInitValue(uint8_t uiHashBoard, uint8_t uiChip)
{
#define NUM_OSC_IN_A_CHIP 8
    int ixI;
    int iResult = ERR_NONE;

    // Set initial value
    sDCR1XferBuf.eMode = E_DCR1_MODE_CHIP_WRITE; // chip wide write should do all oscillators
    sDCR1XferBuf.uiBoard = uiHashBoard;
    sDCR1XferBuf.uiChip = uiChip;
    sDCR1XferBuf.uiCore = 0; // chip wide write
    sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_OCRB;
    sDCR1XferBuf.uiData = DCR1_OCRB_STARTUP_VAL;
    iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    if (ERR_NONE == iResult) {
        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_OCRA;
        sDCR1XferBuf.uiData = DCR1_OCRA_STARTUP_VAL;
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    } // if (ERR_NONE == iResult)

    // Next verify the values
    if (ERR_NONE == iResult) {
        // read back the OCRA from each of the oscillator groups; later we may only need to do one of these
        // since they are on the same chip but it will be useful to initially be aggressive with verification.
        sDCR1XferBuf.eMode = E_DCR1_MODE_REG_READ; // local read from one chip and engine

#if (0 != CSS_DCR1_REVA_WORKAROUND) // workaround for rev A silicon issue on reads; seems to work most of the time
        sDCR1XferBuf.uiReg = (uint8_t)(E_DCR1_REG_OCRA << CSS_DCR1_REVA_WORKAROUND); // reg addr must be shifted for reads
#else
        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_OCRA;
#endif

        // KENC: FOR QUICKER STARTUP, JUST TALK TO ONE OCR - THEY ARE 99.9% likely all OK
        for (ixI = 0; ixI < 1; ixI++) {
            //for (ixI = 0; ixI < NUM_OSC_IN_A_CHIP; ixI++) {
            switch (ixI) {
            default: // because we need a default on every switch
            case 0:
                sDCR1XferBuf.uiCore = DCR1_OCR_OSC_GROUP1;
                break;
            case 1:
                sDCR1XferBuf.uiCore = DCR1_OCR_OSC_GROUP2;
                break;
            case 2:
                sDCR1XferBuf.uiCore = DCR1_OCR_OSC_GROUP3;
                break;
            case 3:
                sDCR1XferBuf.uiCore = DCR1_OCR_OSC_GROUP4;
                break;
            case 4:
                sDCR1XferBuf.uiCore = DCR1_OCR_OSC_GROUP5;
                break;
            case 5:
                sDCR1XferBuf.uiCore = DCR1_OCR_OSC_GROUP6;
                break;
            case 6:
                sDCR1XferBuf.uiCore = DCR1_OCR_OSC_GROUP7;
                break;
            case 7:
                sDCR1XferBuf.uiCore = DCR1_OCR_OSC_GROUP8;
                break;
            } // switch

            sDCR1XferBuf.uiData = 0x0; // hold MOSI fixed low during data part of transfer
            iResult = iDCR1SpiTransfer(&sDCR1XferBuf); // Read to the chip; should block until completed

#if (0 != CSS_DCR1_REVA_WORKAROUND)
            sDCR1XferBuf.uiData = sDCR1XferBuf.uiData >> CSS_DCR1_REVA_WORKAROUND;
#endif

            if (ERR_NONE != iResult) {
                break; // for loop; stop if we get a fail on transfer
            } else {
                if (DCR1_OCRA_STARTUP_VAL != sDCR1XferBuf.uiData) {
                    // if data is all 0's or all 1's send different error code
                    // Data is observed to be all 0's if voltage is very low so we shouldn't bother
                    // to retry in that case.
                    if ((0x0 == sDCR1XferBuf.uiData) || (0xFFFFFFFFF == sDCR1XferBuf.uiData)) {
                        iResult = ERR_FAILURE;
                    } else {
                        iResult = ERR_INVALID_DATA;
                    }
                    break; // for loop; stop if we fail the verify
                }
            } // if (ERR_NONE != iResult)

        } // for (ixI = 0; ixI < NUM_OSC_IN_A_CHIP; ixI++)

    } // if (ERR_NONE == iResult)

    return (iResult);

} // iDCR1StartupOCRInitValue()

/** *************************************************************
 *  \brief DCR1 ASIC Device Initialize Method
 *  \param uint8_t uiBoard sets which board to configure; if MAX_NUMBER_OF_HASH_BOARDS or higher then do all
 *  \return int status of operation (see err_codes.h)
 *
 *  Called once at startup or on demand. Resets and programs the device(s) for a default configuration.
 *  Assumes the associated SPI port is initialized.  Assumes the ASICs have been started up and verified they
 *  work, at proper voltage, etc. Uses multicast to send commands to all cores and chips at the same time.
 *  See CSS Blake256 data sheet section 3.5; the ASIC does not have a hardware reset.  This inializes the various
 *  registers and loads in the initial values to define the BLAKE2B operation.
 *  1) Disable any MCU interrupts associated with DONE/NONCE
 *  2) Mask engines' nonce interrupt outputs with Fifo control register
 *  3) Set the chip START_CLK high; global to all chips to disable clocking of oscillators
 *  4) Set up the oscillator control register (OCR); using slow clocking to start and disable clocks to all cores
 *  5) Reset Engine Control Register (ECR)
 *  6) Set limits register to 0x38 (as per datasheet for DCR1)
 *  7) Set lower bound register to 0x0
 *  8) Set upper bound register to 0x0
 *  9) Set V registers (V0 Match, V00 - V15) to defaults from data sheet
 * 10) Set M registers to 0x0
 * 11) Set ECR reg engine reset and spi reset bits to 1; should reset all cores
 * 12) Set START_CLK low; will start clocking and reset cores
 * 13) Set ECR reg to remove resets
 * 14) Disable clocking to any cores we won't be using; set all chips the same though
 * 15) Verify DONE/NONCE status signal from GPIO expander before starting any jobs
 */
int iDCR1DeviceInit(uint8_t uiBoard)
{
#define DONE_AT_STRING_INIT 0x8000
#define NONCE_AT_STRING_INIT 0x8000
    uint8_t uixBoardCnt, uixI;
    uint8_t uiUUT;
    uint16_t uiTestData;
    int iStart, iEnd, iConfigStep;
    int iResult;
    int iRetval = ERR_NONE;

    if ((0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard)) { // do one or all?
        iStart = (int)uiBoard; // individual board
        iEnd = (int)uiBoard;
    } else {
        iStart = 0; // all boards
        iEnd = LAST_HASH_BOARD;
    } // if ( (0 <= uiHashBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiHashBoard) )

    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n\r\n WARNING! Limited operation during initial testing\r\n");
    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

    for (uixBoardCnt = iStart; uixBoardCnt <= iEnd; uixBoardCnt++) {
        iConfigStep = 1;
        uiUUT = uixBoardCnt;
        iResult = iIsBoardValid(uiUUT, true);
        if (ERR_NONE == iResult) { // board must be there to startup and should have passed initial testing
            sDCR1XferBuf.uiBoard = uiUUT;

            // *************
            // 1) Disable any MCU interrupts associated with DONE/NONCE
            // #TODO < insert code here >     (none presently enabled on board)

            // *************
            // 2) Mask engines' nonce interrupt outputs with Fifo control register
            if (ERR_NONE == iResult) {
                iConfigStep++;
                // Mask engine interrupts in FCR (Fifo control register)
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_FCR;
                sDCR1XferBuf.uiData = E_DCR1_FCR_MASK_ALL;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 3) Set START_CLK inactive (high); global to all chips to disable clocking of oscillators
            (void)iSetHashClockEnable(uiUUT, false); // Deassert START_CLK (HASHENB)

            // *************
            // 4) Set up the oscillator control register (OCR); using slow clocking to start
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_OCRB;
                sDCR1XferBuf.uiData = DCR1_OCRB_STARTUP_VAL;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
                if (ERR_NONE == iResult) {
                    sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_OCRA;
                    sDCR1XferBuf.uiData = (DCR1_OCRA_SLOW_VAL | DCR1_OCR_CORE_ENB); // At startup, we clock all cores; later we may turn off some
                    // Not actually clocking cores yet until we enable hashing clock signal
                    iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
                } // if (ERR_NONE == iResult)

            } // if (ERR_NONE == iResult)

            // *************
            // 5) Reset Engine Control Register (ECR) on all engines and chips
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_ECR;
                sDCR1XferBuf.uiData = 0;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 6) Set limits register as per datasheet
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_LIMITS;
                sDCR1XferBuf.uiData = DCR1_LIMITS_VAL;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // Unlike the SC1 asic, the DCR1 seems to have no step register
            //             if (ERR_NONE == iResult) {
            //                 iConfigStep++;
            //                 sSC1XferBuf.eMode     = E_SC1_MODE_MULTICAST; // global write to all chips and engines
            //                 sSC1XferBuf.uiChip    = 0; // chip and core is don't care (set to 0's)
            //                 sSC1XferBuf.uiCore    = 0;
            //                 sSC1XferBuf.uiReg = E_SC1_REG_STEP;
            //                 sSC1XferBuf.uiData    = SC1_STEP_VAL;
            //                 iResult = iSC1SpiTransfer(&sSC1XferBuf);
            //             }   // if (ERR_NONE == iResult)

            // *************
            // 7) Set lower bound register to 0x0
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_LB;
                sDCR1XferBuf.uiData = 0;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 8) Set upper bound register to 0x0
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_UB;
                sDCR1XferBuf.uiData = 0;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 9) Set V registers (V0 - V15, V0 Match) to defaults from data sheet
            for (uixI = E_DCR1_REG_V00; uixI <= E_DCR1_REG_V15; uixI++) {
                if (ERR_NONE == iResult) {
                    sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                    sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                    sDCR1XferBuf.uiCore = 0;
                    sDCR1XferBuf.uiReg = uixI;
                    sDCR1XferBuf.uiData = ullVRegTable[uixI];
                    iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
                } // if (ERR_NONE == iResult)
            } // for
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DRC1_REG_V0MATCH;
                sDCR1XferBuf.uiData = DCR1_V0_MATCH_VAL;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 10) Set M registers to 0x0
            sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
            sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
            sDCR1XferBuf.uiCore = 0;

            // In DCR1 M0 to M9 are contiguously addressed; M10-M12 have additional offset
            for (uixI = 0; uixI < E_DCR1_NUM_MREGS; uixI++) {
                if (3 != uixI) { // skip M3 on DCR1 ASIC
                    if (10 > uixI) {
                        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_M0 + uixI;
                    } else {
                        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_M0 + uixI + (uint8_t)(E_DCR1_REG_M10 - (E_DCR1_REG_M9 + 1)); // adjust register offset for M10 to M12
                    }
                    sDCR1XferBuf.uiData = 0;
                    iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
                    if (ERR_NONE != iResult) {
                        break; // for
                    }
                }
            } // for

            // *************
            // 11) Set ECR reg engine reset and spi reset bits to 1; should reset all cores
            if (ERR_NONE == iResult) {
                iConfigStep++;
                // Set ECR: Reset core (bit 3) and reset cores' SPI FSM (bit 0) high
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_ECR;
                sDCR1XferBuf.uiData = DCR1_ECR_RESET_SPI_FSM | DCR1_ECR_RESET_CORE;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            } // if (ERR_NONE == iResult)

            // 12) Set START_CLK low; will start clocking and reset cores
            if (ERR_NONE == iResult) {
                iConfigStep++;
                iResult = iSetHashClockEnable(uiUUT, true); // Assert START_CLK to begin clocking oscillators and cores
            } // if (ERR_NONE == iResult)

            // 13) Set ECR reg to release the resets while cores are clocked to reset all cores
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_ECR;
                sDCR1XferBuf.uiData = 0;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            } // if (ERR_NONE == iResult)

            iResult = iSetHashClockEnable(uiUUT, false); // Deassert START_CLK (HASHENB)

            // *************
            // 14) Disable clocking to any cores we won't be using; set all chips the same though
            if (ERR_NONE == iResult) {
                iConfigStep++;
                // For test purposes, we limit power/heat we are only enabling at most 1 core per ASIC
                // Disable all core clocks first
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_OCRA;
                sDCR1XferBuf.uiData = DCR1_OCRA_SLOW_VAL; // mask off clocking to all cores
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            } // if (ERR_NONE == iResult)

            if (ERR_NONE == iResult) {
                // Now enable only those we want;
                // Enabling 128th core of asics of the string for now instead of all
                // #TODO change this to enable all the cores once we have the proper power supply/heat sinks
                sDCR1XferBuf.eMode = E_DCR1_MODE_REG_WRITE; // set oscillator on group of core we want
                sDCR1XferBuf.uiCore = DCR1_OCR_OSC_GROUP8; // upper group only
                for (uixI = 0; uixI <= MAX_DCR1_CHIPS_PER_STRING; uixI++) {
                    sDCR1XferBuf.uiChip = uixI;
                    sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_OCRB;
                    sDCR1XferBuf.uiData = OCRB_CORE15_CLKENB;
                    iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
                    if (ERR_NONE != iResult) {
                        break; // for
                    }
                    sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_OCRA;
                    sDCR1XferBuf.uiData = OCRA_CORE15_CLKENB; // 16th core of group 8 is 128th core
                    iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
                    if (ERR_NONE != iResult) {
                        break; // for
                    }
                } // for
            } // if (ERR_NONE == iResult)

            if (ERR_NONE == iResult) {
                iResult = iSetHashClockEnable(uiUUT, true); // Assert START_CLK to begin clocking oscillators and cores
            }
            // *************

            // 15) Verify there are no DONE or NONCE assertions since we have not yet submitted any jobs
            if (ERR_NONE == iResult) {
                // Test read of DONE signals.  All the DONE/ATTN signals should be low.
                iResult = iReadBoardDoneInts(uiUUT, &uiTestData);
                if (ERR_NONE != iResult) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "ERROR reading DONE status %d\r\n", iResult);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                } else {
                    if (DONE_AT_STRING_INIT != uiTestData) {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "DONE value: FAIL 0x%x (expected 0x%x)\r\n", uiTestData, DONE_AT_STRING_INIT);
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    }
                }
            } // if (ERR_NONE == iResult)

            if (ERR_NONE == iResult) {
                // Test read of NONCE signals.  All the DONE/ATTN signals should be low.
                iResult = iReadBoardNonceInts(uiUUT, &uiTestData);
                if (ERR_NONE != iResult) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "ERROR reading NONCE status %d\r\n", iResult);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                } else {
                    if (NONCE_AT_STRING_INIT != uiTestData) {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "NONCE value: FAIL 0x%x (expected 0x%x)\r\n", uiTestData, NONCE_AT_STRING_INIT);
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    }
                }
            } // if (ERR_NONE == iResult)

            // Mask the FIFO interrupt signals (set the mask bits high); we'll unmask what we want later on.
            // We did this above; doing it again may not be necessary but being conservative.
            if (ERR_NONE == iResult) {
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines
                sDCR1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sDCR1XferBuf.uiCore = 0;
                sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_FCR;
                sDCR1XferBuf.uiData = E_DCR1_FCR_MASK_ALL;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            } // if (ERR_NONE == iResult)

            if (ERR_NONE != iResult) {
                iRetval = iResult; // return error if we detect any valid boards had a problem
                iSetBoardStatus(uiUUT, ERR_FAILURE);
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d DCR1 Failed initial configuration (%d)\r\n", uiUUT + 1, iConfigStep);
            } else {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INFO "HB%d DCR1 Initialized Successfully\r\n", uiUUT + 1);
            }
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

        } // if (ERR_NONE == iResult)

    } // for (uixBoardCnt = iStart; uixBoardCnt <= iEnd; uixBoardCnt++)

    return (iRetval);

} // iDCR1DeviceInit

/** *************************************************************
 * \brief Load job to ASIC; doesn't care if the chip(s)/core(s) are idle or not.
 *  This is just pre-loading the next job and does not start the computation.
 *  As described in the datasheet, this allows a job to be pre-loaded while
 *  computing a job so the next computation can be started quickly.
 *  \param psDCR1Job is ptr to struct with job parameters
 *  \return int status of operation (see err_codes.h)
 *  Assumes sDCRXferBuf structure has the target device in board, chip and core fields
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 * #TODO Make this more generic and public so it can submit jobs on the final design.
 */
static int iDCR1CmdPreLoadJob(S_DCR1_JOB_T* psDCR1Job)
{
    int ixI;
    int iResult;
    E_DCR1_CORE_REG_T eReg;

    // Load in the bounds
    // #TODO: in the real system loading the bounds may be done separately and only done once and then
    // the M-regs are changed only for new jobs. The method is yet to be determined.
    iResult = iDCR1CmdLoadRange(psDCR1Job);

    if (ERR_NONE == iResult) {
        sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and cores on the board
        // Load the job parameters to the core(s) M registers
        // For test, DONE/NONCE should be inactive (low) here before we start.
        // For production board, DONE/NONCE might be asserted (high) if some
        // other cores are active.  Therefore, we are not checking them.
        // Multiple cores can get the same M-reg parameter with different upper/lower bounds.
        // In DCR1 M0 to M9 are contiguously addressed; M10-M12 have additional offset
        for (ixI = 0; ixI < E_DCR1_NUM_MREGS; ixI++) {
            eReg = (uint8_t)E_DCR1_REG_M0 + ixI;
            if (E_DCR1_REG_M3_RSV != eReg) { // skip M3 on DCR1 ASIC
                if (10 <= ixI) {
                    eReg += (uint8_t)(E_DCR1_REG_M10 - (E_DCR1_REG_M9 + 1)); // adjust register offset for M10 to M12
                }
                sDCR1XferBuf.uiReg = eReg;
                sDCR1XferBuf.uiData = psDCR1Job->uiaMReg[ixI];
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
                if (ERR_NONE != iResult) {
                    break; // for
                }
            }
        } // for
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and cores on the board
        for (ixI = 0; ixI < E_DCR1_NUM_VREGS; ixI++) {
            sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_V00 + ixI;
            sDCR1XferBuf.uiData = psDCR1Job->uiaVReg[ixI];
            iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            if (ERR_NONE != iResult) {
                break; // for
            }
        } // for
    } // if (ERR_NONE == iResult)
    if (ERR_NONE == iResult) {
        sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and cores on the board
        sDCR1XferBuf.uiReg = (uint8_t)E_DRC1_REG_V0MATCH;
        sDCR1XferBuf.uiData = psDCR1Job->uiMatchReg;
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    } // if (ERR_NONE == iResult)

    return (iResult);

} // iDCR1CmdPreLoadJob()

/** *************************************************************
 * \brief Placeholder function to load the lower and upper bound regs of the job.
 *  Not sure if this will spread the larger search space across the multiple cores/ASICs.
 *  \param psDCR1Job is ptr to struct with job parameters
 *  \return int status of operation (see err_codes.h)
 *  Assumes sDCR1XferBuf structure has the target device in board, chip and core fields
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 * #TODO Make this more generic and public so it can submit jobs on the real board.
 */
static int iDCR1CmdLoadRange(S_DCR1_JOB_T* psDCR1Job)
{
    int iResult;
    // Determine what boards, chips and cores to transfer to.
    // For initial testing we are loading all chips/cores with the same bounds.
    sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST; // global write to all chips and engines

    // For DCR1 the cores in an ASIC are likely get separate LB and UB with full range spread across
    // multiple cores/chips.
    // Using same test bounds on all ASICs for test purposes here.
    sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_LB;
    sDCR1XferBuf.uiData = psDCR1Job->uiLBReg;
    iResult = iDCR1SpiTransfer(&sDCR1XferBuf);

    if (ERR_NONE == iResult) {
        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_UB;
        sDCR1XferBuf.uiData = psDCR1Job->uiUBReg;
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    }
    return (iResult);

} // iDCR1CmdLoadRange()

/** *************************************************************
 *  \brief Start job to ASIC; the ASIC/cores being started should be idle and the job should
 *  be pre-loaded and ready to start.  This will reset the resources and handshake the Data_Valid.
 *  \param none
 *  \return int status of operation (see err_codes.h)
 *  Assumes sDCR1XferBuf structure has the target device in board, chip and core fields
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 * #TODO Make this more generic and public so it can start jobs on the real board.
 */
static int iDCR1CmdStartJob(void)
{
    int iResult;
    uint16_t uiTestData, uiTestVerify;

    // Reset the core(s) FIFO memory so it can get new solutions and clear any prior NONCE(s)
    sDCR1XferBuf.eMode = E_DCR1_MODE_REG_WRITE;
    sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_ECR;
    sDCR1XferBuf.uiData = DCR1_ECR_RESET_SPI_FSM | DCR1_ECR_RESET_CORE;
    iResult = iDCR1SpiTransfer(&sDCR1XferBuf); // Set ECR: MRST (bit 3) and reset error SPI (bit 0) high
    if (ERR_NONE == iResult) {
        sDCR1XferBuf.uiData = 0;
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf); // Set ECR: MRST (bit 3) and reset error SPI (bit 0) low
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Unmask the bits that define the nonce fifo masks for the desired target
        sDCR1XferBuf.eMode = E_DCR1_MODE_REG_WRITE;
        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_FCR;
        sDCR1XferBuf.uiData = 0; // clear all mask bits to unmask
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Test read of NONCE signals; ; expecting target bit to be clear
        iResult = iReadBoardNonceInts(sDCR1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "reading NONCE status (%d)\r\n", iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            uiTestVerify = 0x1 << sDCR1XferBuf.uiChip;
            if (0 != (uiTestData & uiTestVerify)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "unexpected NONCE assertion (0x%x)\r\n", uiTestVerify);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                //                 iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Test read of DONE signals; ; expecting target bit to be clear
        iResult = iReadBoardDoneInts(sDCR1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "reading DONE status (%d)\r\n", iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            uiTestVerify = 0x1 << sDCR1XferBuf.uiChip;
            if (0 != (uiTestData & uiTestVerify)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "unexpected DONE assertion (0x%x)\r\n", uiTestVerify);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                //                 iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        iResult = iDCR1CmdDataValid(); // submit job(s) by pulsing Data_Valid
    } // if (ERR_NONE == iResult)
    return (iResult);

} // iDCR1CmdStartJob()

/** *************************************************************
 * \brief Test hash board operation function that completes the sample job. Final steps should
 *  verify the NONCE/DONE signals and read back the answer(s) and validate the result.
 *  \param ptr to uint32 to return the solution read back from the device
 *  \return int status of operation (see err_codes.h)
 *  Assumes sDCRXferBuf structure has the target device in board, chip and core fields
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 * #TODO Make this more generic and public so it can start job on the real board.
 * Assumes the board, chip and core are already set in the transfer buffer structure.
 */
static int iDCR1TestJobVerify(uint32_t* puiSolution)
{
    int iResult;
    uint8_t uiCoreSave;
    uint16_t uiTestData;
    uint16_t uiTestVerify;

    // Read any found solution(s) < Note: can't do this with rev A silicon >
    // 1) Determine which board, asic and core generated the NONCE/DONE
    //  1a) Each board has its own DONE/NONCE signals; poll or interrupt on the board slot signal
    //      <SAMG55 test board can only see 1st slot>
    // 2) For an asserted NONCE
    //  2a) Read the NONCE gpio expander and determine which ASIC is signaling
    //  2b) for the ASIC, read the interrupt status reg (ISR); decode the core ID and Fifo bit. See data sheet
    //      note about reading this multiple times to ensure it is valid.
    //  2c) Read the associated Fifo Data Reg (FDR[7:0]) of the core to retrieve the found solution
    //  2e) Mask the Fifo Data Reg position in the Fifo Control Reg (FCR) to remove the NONCE signal
    //  2f) Repeat NONCE signal steps above if NONCE is still asserted
    // 3) For an asserted DONE
    //  3a) Read the DONE gpio expander and determine which ASIC is signaling
    //  3b) Read any NONCEs found (if desired) using steps above
    //  3c) Once all desired NONCEs are read out, pulse Read_Complete (ECR bit 1) signal by asserting
    //      it, verify DONE is deasserted and then deassert Read_Complete

    // Test read of DONE signals; expecting target bit to be set
    iResult = iReadBoardDoneInts(sDCR1XferBuf.uiBoard, &uiTestData);
    if (ERR_NONE != iResult) {
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "reading DONE status (%d)\r\n", iResult);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
    } else {
        uiTestVerify = 0x1 << sDCR1XferBuf.uiChip;
        if (uiTestVerify != (uiTestData & uiTestVerify)) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "missing DONE assertion (0x%x)\r\n", uiTestVerify);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            iResult = ERR_FAILURE;
        } else if (uiTestVerify != (uiTestData & DONE_BITS_MASK)) {
            // For this test, also check that no other DONE bits are set since we are checking asics one-by-one
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "unexpected DONE assertion (0x%x)\r\n", uiTestData);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            iResult = ERR_FAILURE;
        }
    } // if (ERR_NONE != iResult)

    if (ERR_NONE == iResult) {
        // Test read of NONCE signals; expecting target bit to be set
        iResult = iReadBoardNonceInts(sDCR1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "reading NONCE status (%d)\r\n", iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            uiTestVerify = 0x1 << sDCR1XferBuf.uiChip;
            if (uiTestVerify != (uiTestData & uiTestVerify)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d Chip %d missing NONCE assertion\r\n", sDCR1XferBuf.uiBoard + 1, sDCR1XferBuf.uiChip + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_FAILURE;
            } else if (uiTestVerify != (uiTestData & NONCE_BITS_MASK)) {
                // For this test, also check that no other NONCE bits are set since we are checking asics one-by-one
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "unexpected NONCE assertion (0x%x)\r\n", uiTestData);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Read the NONCE data and mask the associated fifo flag(s) in FCR (Fifo control register) to remove NONCE
        sDCR1XferBuf.eMode = E_DCR1_MODE_REG_READ;
        uiCoreSave = sDCR1XferBuf.uiCore;
        sDCR1XferBuf.uiCore = (uint8_t)E_DCR1_CHIP_REGS;
        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_IVR;
        sDCR1XferBuf.uiData = 0;
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
        sDCR1XferBuf.uiCore = uiCoreSave;
        if (ERR_NONE == iResult) {
            // Verify CORE ID equals expected (last core) and interrupt vector is 0th fifo location only
            if ((uint16_t)LAST_DCR1_CORE != (((uint16_t)(sDCR1XferBuf.uiData) & E_DCR1_IVR_CORE_ID_MASK) >> DCR1_IVR_CORE_ID_POS)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d Chip %d unexpected Core ID\r\n", sDCR1XferBuf.uiBoard + 1, sDCR1XferBuf.uiChip + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_FAILURE;
            } else {
                // should only have found a single solution
                if ((uint16_t)E_DCR1_FCR_MASK_D0 != (uint16_t)(sDCR1XferBuf.uiData & E_DCR1_IVR_INT_MASK)) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d Chip %d unexpected Fifo data\r\n", sDCR1XferBuf.uiBoard + 1, sDCR1XferBuf.uiChip + 1);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    iResult = ERR_FAILURE;
                }
            }
        } // if (ERR_NONE == iResult)

        if (ERR_NONE == iResult) { // read the data to get the found solution
            sDCR1XferBuf.eMode = E_DCR1_MODE_REG_READ;
            sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_FDR0;
            sDCR1XferBuf.uiData = 0;
            iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            if (ERR_NONE == iResult) {
                *puiSolution = sDCR1XferBuf.uiData;
            } // if (ERR_NONE == iResult)
        } // if (ERR_NONE == iResult)

    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) { // mask the fifo data flags to remove the NONCE signal
        sDCR1XferBuf.eMode = E_DCR1_MODE_REG_WRITE;
        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_FCR;
        sDCR1XferBuf.uiData = E_DCR1_FCR_MASK_ALL;
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    } // if (ERR_NONE == iResult)

    // For test purposes we are checking that masking above cleared the NONCE signal.  In this case it is testing one asic so the other
    // asics should not be active.  We would not do this check in normal operation except perhaps to test the specific asic we just
    // completed.
    if (ERR_NONE == iResult) {
        // Test read of NONCE signals to verify they are cleared
        iResult = iReadBoardNonceInts(sDCR1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "reading NONCE status (%d)\r\n", iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else if (0 != (uiTestData & NONCE_BITS_MASK)) {
            // For this test, also check that no other DONE bits are set since we are checking asics one-by-one
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "unexpected NONCE assertion (0x%x)\r\n", uiTestData);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            iResult = ERR_FAILURE;
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    DCR1CmdReadComplete(); // Acknowledge reading the solution by pulsing Read_Complete (removes Done but not NONCE)

    // For test purposes we are checking that steps above cleared the DONE signal.  In this case it is testing one asic so the other
    // asics should not be active.  We would not do this check in normal operation except perhaps to test the specific asic we just
    // completed.
    if (ERR_NONE == iResult) {
        // Test read of DONE signals to verify they are cleared
        iResult = iReadBoardDoneInts(sDCR1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "reading DONE status (%d)\r\n", iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else if (0 != (uiTestData & DONE_BITS_MASK)) {
            // For this test, also check that no other DONE bits are set since we are checking asics one-by-one
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "unexpected DONE assertion (0x%x)\r\n", uiTestData);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            iResult = ERR_FAILURE;
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    return (iResult);

} // iDCR1TestJobVerify()

/** *************************************************************
 * \brief Pulse Data-Valid (internal signal) to submit a job to the core(s).  This implements
 * the handshaking described in the data sheet (sec 3.3 in DS rev 1.0, esp fig 2 timing diagram
 * and figure 5 flowchart).
 * This transfers the programmed registers (M-regs, LB and UB regs, etc) into the core(s)
 * to start their computations.
 * The process is:
 * 1) Verify the core is not busy by checking the engine status register of the core.
 * 2) Assert VALID_DATA (ECR bit 2)
 * 3) Wait for the core BUSY assertion (ESR bit 1) until it is asserted <or DONE (ESR bit 0)
 *    is asserted if the job was very short>.  <in rev A silicon we cannot poll BUSY/DONE so
 *    we skip this step>
 * 4) Deassert VALID_DATA
 * After this we would go on to Wait for DONE or NONCE to indicate a potential solution is found.
 *
 *  \param none
 *  \return int status of operation (see err_codes.h)
 *  Assumes sDCR1XferBuf structure has the target device in board, chip and core fields
 */
static int iDCR1CmdDataValid(void)
{
    int iTimeOut;
    int iResult = ERR_NONE;

#if (1 == CSS_DCR1_REVA_WORKAROUND)
    // Rev A silicon cannot read back so we just write the data-valid signal and hope for the best
    if (ERR_NONE == iResult) {
        sDCR1XferBuf.eMode = E_DCR1_MODE_REG_WRITE;
        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_ECR;
        sDCR1XferBuf.uiData = DCR1_ECR_VALID_DATA; // assert DATA_VALID
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    } // if (ERR_NONE == iResult)
    if (ERR_NONE == iResult) {
        delay_us(10); // I have no idea how much to wait; CSS datasheet doesn't say
        sDCR1XferBuf.uiData = 0x0; // deassert DATA_VALID
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    } // if (ERR_NONE == iResult)

#else
    //     // Check for busy status...expected to be not busy before we submit the data valid
    //      This extra check shouldn't be needed.
    //     sDCR1XferBuf.eMode     = E_DCR1_MODE_REG_READ;
    //     sDCR1XferBuf.uiReg     = (uint8_t)E_DCR1_REG_ESR;
    //     sDCR1XferBuf.uiData    = 0;
    //     iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    //     if ( (ERR_NONE == iResult) && (0 != (sDCR1XferBuf.uiData & E_DCR1_ESR_BUSY)) ) {
    //         iResult = ERR_BUSY;     // engine is busy so we cannot give it a job
    //     }

    if (ERR_NONE == iResult) {
        sDCR1XferBuf.eMode = E_DCR1_MODE_REG_WRITE;
        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_ECR;
        sDCR1XferBuf.uiData = DCR1_ECR_VALID_DATA; // assert DATA_VALID
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Check for busy or done status...expect one or other after giving the valid data signal
        sDCR1XferBuf.eMode = E_DCR1_MODE_REG_READ;
        sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_ESR;
        iTimeOut = 1000; // in millsecs assuming 1ms delay below
        do {
            sDCR1XferBuf.uiData = 0;
            iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            if ((ERR_NONE != iResult) || (0 != (sDCR1XferBuf.uiData & (E_DCR1_ESR_DONE | E_DCR1_ESR_BUSY)))) {
                break;
            } // if ( (ERR_NONE != iResult)...
            delay_ms(1);
        } while (0 < --iTimeOut);
        if (0 >= iTimeOut) {
            //            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\nTimeout on busy\r\n");
            //            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            iResult = ERR_TIMEOUT;
        }
    } // if (ERR_NONE == iResult)

    //     // optional extra reporting of chip level busy register
    //     if (ERR_NONE == iResult) {
    //         int uiCoreSave;
    //         sDCR1XferBuf.eMode     = E_DCR1_MODE_REG_READ;
    //         uiCoreSave = sDCR1XferBuf.uiCore;
    //         sDCR1XferBuf.uiCore    = E_DCR1_CHIP_REGS;
    //         sDCR1XferBuf.uiReg     = (uint8_t)E_DCR1_REG_EBR3;
    //         sDCR1XferBuf.uiData    = 0;
    //         iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    //         sDCR1XferBuf.uiCore = uiCoreSave;
    //         (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\nEBR = 0x%x\r\n",sDCR1XferBuf.uiData);
    //         CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
    //     }    // if (ERR_NONE == iResult)

    // Deassert DATA_VALID even if we had an error above
    sDCR1XferBuf.eMode = E_DCR1_MODE_REG_WRITE;
    sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_ECR;
    sDCR1XferBuf.uiData = 0x0; // deassert DATA_VALID
    (void)iDCR1SpiTransfer(&sDCR1XferBuf);

#endif // #if (1 == CSS_DCR1_REVA_WORKAROUND)

    return (iResult);

} // iDCR1CmdDataValid()

/** *************************************************************
 * \brief Pulse Read-Complete internal signal to complete a job to the core(s).
 * This implements the handshaking described in the data sheet (sec 3.3 in DS rev 2.0, esp fig 5 flowchart).
 * This is used after the DONE signal and after reading the nonce data from the fifo.
 *
 * \param none
 * \return none
 *  Assumes sDCR1XferBuf structure has the target device in board, chip and core fields
 */
static void DCR1CmdReadComplete(void)
{
    sDCR1XferBuf.eMode = E_DCR1_MODE_REG_WRITE;
    sDCR1XferBuf.uiReg = (uint8_t)E_DCR1_REG_ECR;
    sDCR1XferBuf.uiData = DCR1_ECR_READ_COMPLETE; // assert READ_COMPLETE
    (void)iDCR1SpiTransfer(&sDCR1XferBuf);
    delay_us(50); // I have no idea how much to wait; CSS datasheet doesn't say
    sDCR1XferBuf.uiData = 0x0; // deassert READ_COMPLETE
    (void)iDCR1SpiTransfer(&sDCR1XferBuf);

} // DCR1CmdReadComplete()

/** *************************************************************
 *  \brief DCR1 ASIC Device SPI Transfer Method
 *  Converts the structure pointed to by psDCR1Transfer to a byte buffer and then calls the SPI transfer.
 *  Assumes the SPI signal routing has been configured to route communications to the ASICs.
 *  Does a (blocking) check in SPI status to prevent changing the output buffer of an on-going transfer.
 *  Will return error if timeout occurs. For reads, this will wait for the data. For writes, this will
 *  start the transfer but not wait. However, the structure can be preconfigured with the next transfer since
 *  it has already been used to construct the SPI buffer.
 *  If caller doesn't want to block while waiting on the SPI bus then it should call iIsHBSpiBusy(false)
 *  prior to calling this to ensure SPI bus is available and ready to go. If multiple tasks use this
 *  resource then arbitration and/or mutex methods should be used.
 *
 *  \param psDCR1Transfer is ptr to struct with address and 64-bit data value to write/read into
 *  \return int error codes from err_codes.h
 *  \retval ERR_NONE Completed successfully.
 *  \retval ERR_BUSY SPI comms busy or timeout; try again later
 *  \retval ERR_INVALID_DATA invalid parameter?
 */
int iDCR1SpiTransfer(S_DCR1_TRANSFER_T* psDCR1Transfer)
{
    uint8_t ixI;
    int iRetVal = ERR_NONE;

    // Do some basic checks on the input; not testing everything
    if (NULL == psDCR1Transfer) {
        iRetVal = ERR_INVALID_DATA;
    } else {
        switch (psDCR1Transfer->eMode) {
        case E_DCR1_MODE_REG_WRITE:
        case E_DCR1_MODE_REG_READ:
        case E_DCR1_MODE_CHIP_WRITE:
        case E_DCR1_MODE_MULTICAST:
            break;
        default:
            iRetVal = ERR_INVALID_DATA;
        } // end switch
    }

    if (ERR_NONE == iRetVal) {
        // Verify the SPI port is idle and ready to go; if not we wait to allow an active transfer to complete.
        // We don't want to corrupt the transfer buffer or change the SPI mux during an active transfer.
        if (0 != iIsHBSpiBusy(true)) {
            iRetVal = ERR_BUSY; // got a timeout?
        }
    }

    if (ERR_NONE == iRetVal) {
        // Set up the mode-address in bytes [2:0]; big-endian order
        ucaDCR1OutBuf[0] = (uint8_t)((psDCR1Transfer->eMode << 6) & 0xC0); // 2-bit mode
        ucaDCR1OutBuf[0] |= (uint8_t)((psDCR1Transfer->uiChip >> 1) & 0x3F); // 6-msb of 7-bit chip addr
        ucaDCR1OutBuf[1] = (uint8_t)((psDCR1Transfer->uiChip << 7) & 0x80); // lsb of 7-bit chip addr
        ucaDCR1OutBuf[1] |= (uint8_t)((psDCR1Transfer->uiCore >> 1) & 0x7F); // 7-msb of 8-bit core addr
        ucaDCR1OutBuf[2] = (uint8_t)(psDCR1Transfer->uiReg & DCR1_ADR_REG_Bits); // 7-bit reg offset
        ucaDCR1OutBuf[2] |= (uint8_t)((psDCR1Transfer->uiCore << 7) & 0x80); // lsb of 8-bit core addr
        // Data is 32 bits written msb first; so we need to switch endianism
        if (0 != psDCR1Transfer->uiData) {
            Uint32ToArray(psDCR1Transfer->uiData, &ucaDCR1OutBuf[DCR1_TRANSFER_CONTROL_BYTES], true); // convert to array with endian swap to network order
        } else { // slight optimization for 0 on write or for reads
            for (ixI = 0; ixI < DCR1_TRANSFER_DATA_BYTES; ixI++) { // data is zero so just clear it out
                ucaDCR1OutBuf[DCR1_TRANSFER_CONTROL_BYTES + ixI] = (uint8_t)0;
            }
        } // if (0 != psDCR1Transfer->uiData)

        // Set board SPI mux and SS for the hashBoard we are going to transfer with.
        HBSetSpiMux(E_SPI_ASIC); // set mux for SPI on the hash board
        HBSetSpiSelects(psDCR1Transfer->uiBoard, false);

        // On SAMG55 the callback will deassert the slave select signal(s) at end of transfer.
        // On SAMA5D27 SOM; it must be done in the code because callback is not used.
        if (E_DCR1_MODE_REG_READ == psDCR1Transfer->eMode) {
            // read transfer; this will wait for the data so it returns with the result
            (void)bSPI5StartDataXfer(E_SPI_XFER8, ucaDCR1OutBuf, ucaDCR1InBuf, DCR1_TRANSFER_BYTE_COUNT);
#if (HW_SAMA5D27_SOM == HARDWARE_PLATFORM)
            HBSetSpiSelects(psDCR1Transfer->uiBoard, true); // SAMA5D27
            psDCR1Transfer->uiData = uiArrayToUint32(&ucaDCR1InBuf[3], true);
#endif // #if (HW_SAMA5D27_SOM == HARDWARE_PLATFORM)

#if (HW_ATSAMG55_XP_BOARD == HARDWARE_PLATFORM)
            if (0 != iIsHBSpiBusy(true)) { // wait for xfer to complete
                iRetVal = ERR_BUSY; // got a timeout?
                // Note: slave selects will probably be messed up if the transfer timed out. Error handler should
                // deal with this.
                SPITimeOutError();
            } else {
                // assemble input to 32 bit data; switch to small endian
                psDCR1Transfer->uiData = uiArrayToUint32(&ucaDCR1InBuf[3], true);
                // Note: Rev A asics may corrupt the data; on some registers this seems to be shifted left by 1 bit
                // It should be unshifted by the caller after the endian swap above.
                //                 DCR1SetSpiSelects(psDCR1Transfer->uiBoard,true);  // should not be needed; done by the callback
            } // if (0 != iIsHBSpiBusy(true))
#endif // #if (HW_ATSAMG55_XP_BOARD == HARDWARE_PLATFORM)
        } else { // write transfer; start the transfer but don't wait
            (void)bSPI5StartDataXfer(E_SPI_XFER_WRITE, ucaDCR1OutBuf, ucaDCR1InBuf, DCR1_TRANSFER_BYTE_COUNT);

#if (HW_SAMA5D27_SOM == HARDWARE_PLATFORM)
            HBSetSpiSelects(psDCR1Transfer->uiBoard, true); // SAMA5D27
#endif // #if (HW_SAMA5D27_SOM == HARDWARE_PLATFORM)

        } // if (E_DCR1_MODE_REG_READ == psDCR1Transfer->eMode)

    } // if (ERR_NONE == iRetVal)

    return (iRetVal);

} // iDCR1SpiTransfer

// ** **********************************
// Test Functions
// ** **********************************
/** *************************************************************
 * \brief Test function to submit example jobs. This can be used to test basic
 *  hashing board functionality. This only tests one core of an asic to keep the
 *  power use low.  It would be suitable as a bench test of a hash board without requiring
 *  a full power supply, fans, heat-sinks, etc.
 *  For each asic, enables the Fifo control masks, submits a sample job, waits for the DONE,
 *  checks the NONCE values, cleans up and does the next asic.  Then advances to the next example
 *  job and repeats until all the sample jobs are completed.
 * \param none
 * \return int type; is less than 0 for an error, 0 for no error, 1 for completed
 * Assumes each example only has a single solution.
 */
int iDCR1TestJobs(void)
{
#define FIRST_STATE 0
#define NEW_JOB 1
#define SET_UUT 2
#define LOAD_JOB 3
#define START_JOB 4
#define WAIT_FOR_DONE 5
#define READ_NONCE 6
#define NEXT_CORE 7
#define NEXT_CHIP 8
#define NEXT_BOARD 9
#define NEXT_SAMPLE_JOB 10
#define LAST_STATE 11

    static uint8_t uiBoardUUT = 0;
    static uint8_t uiAsicUUT = 0;
    static uint8_t uiCoreUUT = 0;
    static uint8_t uiSampleJob = 0;
    static int iState = FIRST_STATE;

    uint32_t uiSolution;
    uint16_t uiTestData;
    static uint16_t uiTimeOut;
    int iResult = ERR_NONE;
    int iReturn = ERR_NONE;

    switch (iState) {
    default:
        iState = FIRST_STATE;
    case FIRST_STATE: // Start of the machine;
        uiSampleJob = 0;
        iState = NEW_JOB;
        break;

    case NEW_JOB: // update the job
        DCR1GetTestJob(uiSampleJob); // Set up the job
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_INFO "Test Job %d\r\n", uiSampleJob + 1);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        uiBoardUUT = 0;
        uiAsicUUT = 0;
        uiCoreUUT = 0;
        iResult = iIsBoardValid(uiBoardUUT, true); // Verify the board is a valid target
        if (ERR_NONE != iResult) { // board must be there to startup and should have passed initial testing
            iState = NEXT_BOARD;
        } else {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "Board %d", uiBoardUUT + 1);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            iState = SET_UUT;
        }
        break;

    case SET_UUT:
        sDCR1XferBuf.uiBoard = uiBoardUUT;
        sDCR1XferBuf.uiChip = uiAsicUUT;
        uiCoreUUT = LAST_DCR1_CORE; // holding at last core for now
        sDCR1XferBuf.uiCore = uiCoreUUT;
        iState = LOAD_JOB;
        break;

    case NEXT_SAMPLE_JOB:
        if (MAX_SAMPLE_JOBS <= ++uiSampleJob) {
            iState = LAST_STATE; // all done
        } else {
            iState = NEW_JOB;
        }
        break;

    case NEXT_CORE:
        if (MAX_NUMBER_OF_DCR1_CORES <= ++uiCoreUUT) {
            uiCoreUUT = 0;
            // #TODO have to change the clocking bits in the OCR to disable prior core and enable the next one
            iState = NEXT_CHIP;
        } else {
            iState = SET_UUT;
        }
        break;

    case NEXT_CHIP:
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, ".");
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        if (MAX_DCR1_CHIPS_PER_STRING <= ++uiAsicUUT) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "DONE");
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            uiAsicUUT = 0;
            iState = NEXT_BOARD;
        } else {
            iState = SET_UUT;
        }
        break;

    case NEXT_BOARD:
        if (MAX_NUMBER_OF_HASH_BOARDS <= ++uiBoardUUT) {
            uiBoardUUT = 0;
            iState = NEXT_SAMPLE_JOB;
        } else {
            iResult = iIsBoardValid(uiBoardUUT, true); // Verify the board is a valid target
            if (ERR_NONE == iResult) { // board must be there to startup and should have passed initial testing
                uiAsicUUT = 0;
                uiCoreUUT = 0;
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "Board %d", uiBoardUUT + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iState = SET_UUT;
            }
        }
        break;

    case LOAD_JOB: // pre-load job into the target
        iResult = iDCR1CmdPreLoadJob(&sDCR1JobBuf); // preload the job parameters to the ASIC(s)
        if (ERR_NONE != iResult) {
            iReturn = iResult;
        } else {
            iState = START_JOB;
        }
        break;

    case START_JOB: // Configure the target
        iResult = iDCR1CmdStartJob(); // submit the job
        if (ERR_NONE != iResult) {
            iReturn = iResult;
        } else {
            uiTimeOut = 5000; // wait for done timeout in millisecs (assuming 1 ms delay below)
            iState = WAIT_FOR_DONE;
        }
        break;

    case WAIT_FOR_DONE: // waiting for nonce/done
        do {
            iResult = iReadBoardDoneInts(sDCR1XferBuf.uiBoard, &uiTestData);
            if (ERR_NONE != iResult) {
                // problem reading the done status
                iResult = ERR_FAILURE;
            } else {
                if (0 != (uiTestData & DONE_BITS_MASK)) {
                    break; // while ; got a done from somebody
                }
            }
            if (0 == --uiTimeOut) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, " Timeout on Done\r\n");
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_TIMEOUT; // error; timeout on wait; breaks while
            } else {
                delay_ms(1);
            }
        } while (ERR_NONE == iResult);
        if (ERR_NONE == iResult) {
            iState = READ_NONCE;
        } else {
            iReturn = iResult;
        }
        break;

    case READ_NONCE: // read and verify the nonce
        // since these are sample jobs they should have a single nonce returned; read and verify and clear the nonce
        iResult = iDCR1TestJobVerify(&uiSolution);
        if (ERR_NONE != iResult) {
            iReturn = iResult;
        } else {
            if (sDCR1JobBuf.uiSolution != uiSolution) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d Chip%d unexpected solution\r\n", sDCR1XferBuf.uiBoard + 1, sDCR1XferBuf.uiChip + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            }
            iState = NEXT_CORE;
        }
        break;

    case LAST_STATE:
        iState = FIRST_STATE;
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n");
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        iReturn = 1; // all done
        break;
    }
    if (ERR_NONE > iReturn) { // restart machine if we got an error code
        iState = FIRST_STATE;
    }
    return (iReturn);
} // iDCR1TestJobs()

/** *************************************************************
 * \brief Test function to submit example job.  This defines what to load.
 * \param uiSampleNum is number of the job to load
 * \return none
 */
static void DCR1GetTestJob(uint8_t uiSampleNum)
{

    switch (uiSampleNum) {
    default:
    case 0:
        sDCR1JobBuf.uiLBReg = DCR_SAMPLE1_LB;
        sDCR1JobBuf.uiUBReg = DCR_SAMPLE1_UB;
        sDCR1JobBuf.uiaMReg[0] = DCR_SAMPLE1_M0;
        sDCR1JobBuf.uiaMReg[1] = DCR_SAMPLE1_M1;
        sDCR1JobBuf.uiaMReg[2] = DCR_SAMPLE1_M2;
        sDCR1JobBuf.uiaMReg[3] = 0; // M3 not used
        sDCR1JobBuf.uiaMReg[4] = DCR_SAMPLE1_M4;
        sDCR1JobBuf.uiaMReg[5] = DCR_SAMPLE1_M5;
        sDCR1JobBuf.uiaMReg[6] = DCR_SAMPLE1_M6;
        sDCR1JobBuf.uiaMReg[7] = DCR_SAMPLE1_M7;
        sDCR1JobBuf.uiaMReg[8] = DCR_SAMPLE1_M8;
        sDCR1JobBuf.uiaMReg[9] = DCR_SAMPLE1_M9;
        sDCR1JobBuf.uiaMReg[10] = DCR_SAMPLE1_M10;
        sDCR1JobBuf.uiaMReg[11] = DCR_SAMPLE1_M11;
        sDCR1JobBuf.uiaMReg[12] = DCR_SAMPLE1_M12;
        sDCR1JobBuf.uiMatchReg = DCR_SAMPLE1_MATCH;
        sDCR1JobBuf.uiaVReg[0] = DCR_SAMPLE1_V00;
        sDCR1JobBuf.uiaVReg[1] = DCR_SAMPLE1_V01;
        sDCR1JobBuf.uiaVReg[2] = DCR_SAMPLE1_V02;
        sDCR1JobBuf.uiaVReg[3] = DCR_SAMPLE1_V03;
        sDCR1JobBuf.uiaVReg[4] = DCR_SAMPLE1_V04;
        sDCR1JobBuf.uiaVReg[5] = DCR_SAMPLE1_V05;
        sDCR1JobBuf.uiaVReg[6] = DCR_SAMPLE1_V06;
        sDCR1JobBuf.uiaVReg[7] = DCR_SAMPLE1_V07;
        sDCR1JobBuf.uiaVReg[8] = DCR_SAMPLE1_V08;
        sDCR1JobBuf.uiaVReg[9] = DCR_SAMPLE1_V09;
        sDCR1JobBuf.uiaVReg[10] = DCR_SAMPLE1_V10;
        sDCR1JobBuf.uiaVReg[11] = DCR_SAMPLE1_V11;
        sDCR1JobBuf.uiaVReg[12] = DCR_SAMPLE1_V12;
        sDCR1JobBuf.uiaVReg[13] = DCR_SAMPLE1_V13;
        sDCR1JobBuf.uiaVReg[14] = DCR_SAMPLE1_V14;
        sDCR1JobBuf.uiaVReg[15] = DCR_SAMPLE1_V15;
        sDCR1JobBuf.uiSolution = DCR_SAMPLE1_FDR0;
        break;
    case 1:
        sDCR1JobBuf.uiLBReg = DCR_SAMPLE2_LB;
        sDCR1JobBuf.uiUBReg = DCR_SAMPLE2_UB;
        sDCR1JobBuf.uiaMReg[0] = DCR_SAMPLE2_M0;
        sDCR1JobBuf.uiaMReg[1] = DCR_SAMPLE2_M1;
        sDCR1JobBuf.uiaMReg[2] = DCR_SAMPLE2_M2;
        sDCR1JobBuf.uiaMReg[3] = 0; // M3 not used
        sDCR1JobBuf.uiaMReg[4] = DCR_SAMPLE2_M4;
        sDCR1JobBuf.uiaMReg[5] = DCR_SAMPLE2_M5;
        sDCR1JobBuf.uiaMReg[6] = DCR_SAMPLE2_M6;
        sDCR1JobBuf.uiaMReg[7] = DCR_SAMPLE2_M7;
        sDCR1JobBuf.uiaMReg[8] = DCR_SAMPLE2_M8;
        sDCR1JobBuf.uiaMReg[9] = DCR_SAMPLE2_M9;
        sDCR1JobBuf.uiaMReg[10] = DCR_SAMPLE2_M10;
        sDCR1JobBuf.uiaMReg[11] = DCR_SAMPLE2_M11;
        sDCR1JobBuf.uiaMReg[12] = DCR_SAMPLE2_M12;
        sDCR1JobBuf.uiMatchReg = DCR_SAMPLE2_MATCH;
        sDCR1JobBuf.uiaVReg[0] = DCR_SAMPLE2_V00;
        sDCR1JobBuf.uiaVReg[1] = DCR_SAMPLE2_V01;
        sDCR1JobBuf.uiaVReg[2] = DCR_SAMPLE2_V02;
        sDCR1JobBuf.uiaVReg[3] = DCR_SAMPLE2_V03;
        sDCR1JobBuf.uiaVReg[4] = DCR_SAMPLE2_V04;
        sDCR1JobBuf.uiaVReg[5] = DCR_SAMPLE2_V05;
        sDCR1JobBuf.uiaVReg[6] = DCR_SAMPLE2_V06;
        sDCR1JobBuf.uiaVReg[7] = DCR_SAMPLE2_V07;
        sDCR1JobBuf.uiaVReg[8] = DCR_SAMPLE2_V08;
        sDCR1JobBuf.uiaVReg[9] = DCR_SAMPLE2_V09;
        sDCR1JobBuf.uiaVReg[10] = DCR_SAMPLE2_V10;
        sDCR1JobBuf.uiaVReg[11] = DCR_SAMPLE2_V11;
        sDCR1JobBuf.uiaVReg[12] = DCR_SAMPLE2_V12;
        sDCR1JobBuf.uiaVReg[13] = DCR_SAMPLE2_V13;
        sDCR1JobBuf.uiaVReg[14] = DCR_SAMPLE2_V14;
        sDCR1JobBuf.uiaVReg[15] = DCR_SAMPLE2_V15;
        sDCR1JobBuf.uiSolution = DCR_SAMPLE2_FDR0;
        break;

    } // switch

} // DCR1GetTestJob()

#ifdef ENABLE_EMC_TEST_FUNCTIONS
// ** **********************************
// Test Functions
// ** **********************************

/** *************************************************************
 * \brief Test function to submit example jobs for EMC testing.
 *  - Enables all cores and desired clock rate.
 *  - Submit the same job to all cores.  For DCR1 uses half the 64-bit bounds.
 *  - Waits for all asics to signal done (this is not checking all cores for done)
 *  - During waiting and after waiting check temperatures to verify the asic is not too hot.
 *  - If too hot, shut down the clocking and wait until it is cool enough to restart.
 *  - Repeat the job.
 * \param iControlState is EMC_TEST_STATE_START to begin/restart the machine;
 *       EMC_TEST_STATE_END to force an end of process; EMC_TEST_STATE_RUN to run normally
 * \return int type; is less than 0 for an error, 0 for no error, EMC_TEST_STATE_END (1) for completed
 * Assumes each example only has a single solution.
 */
/*
int iDCR1EMCTestJob(int iControlState)
{
#define EMC_NONCE_UB 0xFFFFFFFF // Only 32-bits as opposed to SC1's 64-bits

#define EMC_JOB_TIMEOUT_MS 12000 * 5 // about 60 sec limit for the emc job
    // #define FIRST_STATE 0
    // #define NEW_JOB 1
    // #define LOAD_JOB 2
    // #define START_JOB 3
    // #define WAIT_FOR_DONE 4
    // #define READ_NONCE 5
    // #define LAST_STATE 6

#define ERR_NOBOARDS_FOUND -100
    static uint8_t uiBoardUUT = 0;
    static uint8_t uiSampleJob = 0;
    static int iState = FIRST_STATE;
    static uint16_t uiDoneStatusOld[MAX_NUMBER_OF_HASH_BOARDS];
    static uint32_t uiEmcJobTimeOut;

    int ixI;
    uint64_t uiSolution;
    uint16_t uiTestData, uiDoneStatus;
    int iResult = ERR_NONE;
    int iReturn = ERR_NONE;

    if (EMC_TEST_STATE_START == iControlState) {
        iState = FIRST_STATE;
    } else if (EMC_TEST_STATE_END == iControlState) {
        iState = LAST_STATE;
    }

    switch (iState) {
    default:
        iState = FIRST_STATE;
    case FIRST_STATE: // Start of the machine;
        uiSampleJob = 0;
        iState = NEW_JOB;
        break;

    case NEW_JOB: // update the job
        DCR1GetTestJob(uiSampleJob); // Load the job structure with parameters
        // overwrite the bounds for emc test
        sDCR1JobBuf.uiLBReg = 0x0000000000000000; // (was SAMPLE1_LB = 0x000000000B609366)
        sDCR1JobBuf.uiUBReg = EMC_NONCE_UB; // is 2^32 -1  (was SAMPLE1_UB = 0x000000000B609386 ) // TWEAK
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_INFO "Job Sample %d:\r\n", uiSampleJob + 1);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        iState = LOAD_JOB;
        break; // case NEW_JOB

    case LOAD_JOB: // pre-load into the target board(s)
        iResult = ERR_NOBOARDS_FOUND; // safety catch for case with no valid boards; gets overridden if there is a board
        for (ixI = 0; ixI < MAX_NUMBER_OF_HASH_BOARDS; ixI++) {
            uiBoardUUT = (uint8_t)ixI;
            if (ERR_NONE == iIsBoardValid(uiBoardUUT, true)) {
                iResult = iSetDCR1OCRDividerAndBias(uiBoardUUT, controlLoopState[uiBoardUUT].currDivider, controlLoopState[uiBoardUUT].currBias);

                // // Change the oscillator clocking to normal (divide by 1) mode and enable clocking on all cores
                // sDCR1XferBuf.eMode
                //     = E_DCR1_MODE_MULTICAST;
                // sDCR1XferBuf.uiChip = 0; // all chips
                // sDCR1XferBuf.uiCore = 0; // all cores
                // sDCR1XferBuf.uiReg = E_DCR1_REG_OCRB; // TODO: Need to transfer both OCRA and OCRB
                // sDCR1XferBuf.uiData = OCRB_CORE15_CLKENB; // TODO: This value is probably wrong!!!!!!!!!!!!!!!!!!!!
                // iResult = iDCR1SpiTransfer(&sDCR1XferBuf);

                if (ERR_NONE != iResult) {
                    break; // for
                } else {
                    iResult = iDCR1CmdPreLoadJob(&sDCR1JobBuf);
                    if (ERR_NONE != iResult) {
                        break; // for
                    }
                }
            } // if (ERR_NONE == iIsBoardValid(uiBoardUUT,true))
        } // for

        if (ERR_NONE != iResult) { // catch ERR_NOBOARDS_FOUND or other errors
            iReturn = iResult;
        } else {
            iState = START_JOB; // start job if no error
        }
        break; // case LOAD_JOB

    case START_JOB: // Configure the target
        iResult = ERR_NOBOARDS_FOUND; // safety catch for case with no valid boards; gets overridden if there is a board
        for (ixI = 0; ixI < MAX_NUMBER_OF_HASH_BOARDS; ixI++) {
            uiBoardUUT = (uint8_t)ixI;
            if (ERR_NONE == iIsBoardValid(uiBoardUUT, true)) {
                sDCR1XferBuf.uiBoard = uiBoardUUT;

                // HASHRATE
                clock_gettime(CLOCK_MONOTONIC, &hashJobInfo[ixI].start);
                hashJobInfo[ixI].end.tv_sec = 0;
                hashJobInfo[ixI].end.tv_nsec = 0;

                iResult = iDCR1EMCStartJob(); // submit the job
                if (ERR_NONE != iResult) {
                    break; // for
                }
            } // if (ERR_NONE == iIsBoardValid(uiBoardUUT,true))
        } // for

        if (ERR_NONE != iResult) { // catch ERR_NOBOARDS_FOUND or other errors
            iReturn = iResult;
        } else {
            uiEmcJobTimeOut = EMC_JOB_TIMEOUT_MS;
            for (ixI = 0; ixI < MAX_NUMBER_OF_HASH_BOARDS; ixI++) {
                uiDoneStatusOld[ixI] = 0;
            }
            iState = WAIT_FOR_DONE; // next state if no error
        }
        break; // case START_JOB

    case WAIT_FOR_DONE: // waiting for nonce/done
        iState = READ_NONCE; // optimistic assumption; overriden if not done

        // For loop checks each hash board for done status and reports changes in status
        for (ixI = 0; ixI < MAX_NUMBER_OF_HASH_BOARDS; ixI++) {
            uiBoardUUT = (uint8_t)ixI;
            if (ERR_NONE == iIsBoardValid(uiBoardUUT, true)) { // board must be there and no errors
                iResult = iReadBoardDoneInts(uiBoardUUT, &uiTestData);
                if (ERR_NONE != iResult) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d ERROR reading DONE status\r\n", uiBoardUUT + 1);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    break; // for
                } else {
                    uiDoneStatus = (uiTestData & DONE_BITS_MASK);
                    if (DONE_BITS_MASK != uiDoneStatus) {
                        iState = WAIT_FOR_DONE;
                    } else {
                        // Only do this the first time - while we're waiting for the other boards to be done,
                        // this code is still called, so we don't want to drop out effective hashrate.
                        if (hashJobInfo[ixI].end.tv_sec == 0 && hashJobInfo[ixI].end.tv_nsec == 0) {
                            // HASHRATE
                            clock_gettime(CLOCK_MONOTONIC, &hashJobInfo[ixI].end);
                            struct timespec* start = &hashJobInfo[ixI].start;
                            struct timespec* end = &hashJobInfo[ixI].end;
                            HashJobInfo* jobInfo = &hashJobInfo[ixI];

                            // Count the hashes for every engine on every ASIC on this board
                            jobInfo->nonceCount = (uint64_t)EMC_NONCE_UB * 15LL * 64LL;
                            jobInfo->totalTime = ((uint64_t)(end->tv_sec - start->tv_sec) * 1000000000LL) + (uint64_t)(end->tv_nsec - start->tv_nsec);
                            jobInfo->totalTime /= 1000000LL; // Convert to milliseconds
                            // (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "** totalTime=%llu ms", jobInfo->totalTime);
                            // CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

                            if (jobInfo->totalTime > 0) {
                                jobInfo->hashrateGHS = (double)((double)jobInfo->nonceCount / ((double)jobInfo->totalTime / 1000.0)) / 1000000000.0;
                            } else {
                                jobInfo->hashrateGHS = 0;
                            }
                            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE,
                                "=====\nHB%u: start=%ld+%ld  end=%ld+%ld  nonceCount=%llu  totalTime=%llums  hashrateGHS=%f\n=====\n",
                                uiBoardUUT + 1,
                                start->tv_sec,
                                start->tv_nsec,
                                end->tv_sec,
                                end->tv_nsec,
                                jobInfo->nonceCount,
                                jobInfo->totalTime,
                                jobInfo->hashrateGHS);
                            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        }
                    }

                    if (uiDoneStatusOld[ixI] != uiDoneStatus) { // report changes as they happen
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INFO "HB%d Done status change (0x%04x)\r\n", uiBoardUUT + 1, uiDoneStatus);
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        uiDoneStatusOld[ixI] = uiDoneStatus;
                    }
                }
            } // if (ERR_NONE == iIsBoardValid(uiBoardUUT,true))
        } // for

        if (ERR_NONE != iResult) {
            iReturn = iResult;
        } else {
            if (0 == --uiEmcJobTimeOut) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "Timeout on EMC job\r\n");
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_TIMEOUT;
                iReturn = iResult;
            } else {
                if (WAIT_FOR_DONE == iState) {
                    delay_ms(1); // need this so we don't timeout too quickly
                }
            } // if (0 == --uiEmcJobTimeOut)
        }
        break; // case WAIT_FOR_DONE

    case READ_NONCE:
        // Since these are sample jobs they should have a single nonce returned; read and verify and clear the nonce.
        // For EMC testing purposes this only needs to check one chip and one core since they all get the same problem.
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "Verify EMC job\r\n");
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

        // For loop verifies each hash board solution and does the read complete handshake
        for (ixI = 0; ixI < MAX_NUMBER_OF_HASH_BOARDS; ixI++) {
            uiBoardUUT = (uint8_t)ixI;
            if (ERR_NONE == iIsBoardValid(uiBoardUUT, true)) { // board must be there and no errors
                sDCR1XferBuf.uiBoard = uiBoardUUT;
                iResult = iDCR1EMCTestJobVerify(&uiSolution);
                //                     if (ERR_NONE == iResult) {
                //                         // verify we got the expected solution
                //                         if (sDCR1JobBuf.uiSolution != uiSolution) {
                //                             (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n"MSG_TKN_ERROR"HB%d Chip %d unexpected solution\r\n",sDCR1XferBuf.uiBoard+1, sDCR1XferBuf.uiChip+1);
                //                             CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                //                             // Don't declare a failure on timeout; we just keep going
                //                         }
                //                     }
            } // if (ERR_NONE == iIsBoardValid(uiBoardUUT,true))
        } // for

        uiSampleJob = 0; // Repeat same job
        iState = LAST_STATE;
        break; // case READ_NONCE

    case LAST_STATE:
        for (ixI = 0; ixI < MAX_NUMBER_OF_HASH_BOARDS; ixI++) {
            uiBoardUUT = (uint8_t)ixI;
            if (ERR_NONE == iIsBoardValid(uiBoardUUT, true)) {
                sDCR1XferBuf.uiBoard = uiBoardUUT;
                // set oscillator back to divide by 8 and only last core of each group enabled
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST;
                sDCR1XferBuf.uiChip = 0; // all chips
                sDCR1XferBuf.uiCore = 0; // all cores
                sDCR1XferBuf.uiReg = E_DCR1_REG_OCRB; // TODO: Need to transfer both OCRA and OCRB
                sDCR1XferBuf.uiData = OCRA_CORE15_CLKENB;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            }
        } // for
        iState = FIRST_STATE;
        iReturn = EMC_TEST_STATE_END; // all done
        break; // case LAST_STATE
    } // switch (iState)

    if (ERR_NONE > iReturn) { // clean up if we got an error code
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "iDCR1EMCTestJob(): Error %d in case %d\r\n", iReturn, iState);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

        for (ixI = 0; ixI < MAX_NUMBER_OF_HASH_BOARDS; ixI++) {
            uiBoardUUT = (uint8_t)ixI;
            if (ERR_NONE == iIsBoardValid(uiBoardUUT, true)) {
                sDCR1XferBuf.uiBoard = uiBoardUUT;
                // set oscillator back to divide by 8 and only last core of each group enabled
                sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST;
                sDCR1XferBuf.uiChip = 0; // all chips
                sDCR1XferBuf.uiCore = 0; // all cores
                sDCR1XferBuf.uiReg = E_DCR1_REG_OCRB; // TODO: Need to transfer both OCRA and OCRB
                sDCR1XferBuf.uiData = OCRA_CORE15_CLKENB;
                iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            }
        } // for
        // restart state machine if we got an error code
        iState = FIRST_STATE;
    } // if (ERR_NONE > iReturn)

    return (iReturn);
} // iDCR1EMCTestJob()
*/

/** *************************************************************
 *  \brief Start job to ASIC for EMC testing;
 *  This will reset the resources and handshake the Data_Valid.
 *  \param none
 *  \return int status of operation (see err_codes.h)
 *  Assumes sDCR1XferBuf structure has the target board
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 */
static int iDCR1EMCStartJob(void)
{
    int iResult;
    uint16_t uiTestData, uiTestVerify;

    // Reset the core(s) FIFO memory so it can get new solutions and clear any prior NONCE(s)
    sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST;
    sDCR1XferBuf.uiChip = 0; // all chips
    sDCR1XferBuf.uiCore = 0; // all cores
    sDCR1XferBuf.uiReg = E_DCR1_REG_ECR;
    sDCR1XferBuf.uiData = DCR1_ECR_RESET_SPI_FSM | DCR1_ECR_RESET_CORE;
    iResult = iDCR1SpiTransfer(&sDCR1XferBuf); // Set ECR: MRST (bit 3) and reset error SPI (bit 0) high
    if (ERR_NONE == iResult) {
        sDCR1XferBuf.uiData = 0;
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf); // Set ECR: MRST (bit 3) and reset error SPI (bit 0) low
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Unmask the bits that define the nonce fifo masks for the desired target
        sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST;
        sDCR1XferBuf.uiReg = E_DCR1_REG_FCR;
        sDCR1XferBuf.uiData = 0; // clear all mask bits to unmask
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Test read of NONCE signals; ; expecting target bit to be clear
        iResult = iReadBoardNonceInts(sDCR1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d Reading NONCE status (%d)\r\n", sDCR1XferBuf.uiBoard + 1, iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            if (0 != (uiTestData & NONCE_BITS_MASK)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d Unexpected NONCE assertion (0x%x)\r\n", uiTestVerify);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                // just a warning; don't stop the EMC testing
                // iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Test read of DONE signals; ; expecting target bit to be clear
        iResult = iReadBoardDoneInts(sDCR1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d reading DONE status (%d)\r\n", sDCR1XferBuf.uiBoard + 1, iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            if (0 != (uiTestData & DONE_BITS_MASK)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d unexpected DONE assertion (0x%x)\r\n", sDCR1XferBuf.uiBoard + 1, uiTestVerify);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                // just a warning; don't stop the EMC testing
                // iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        iResult = iDCR1EMCDataValid(); // submit job(s) by pulsing Data_Valid
    } // if (ERR_NONE == iResult)
    return (iResult);

} // iDCR1EMCStartJob()

/** *************************************************************
 * \brief Pulse Data-Valid (internal signal) to submit a job to the core(s).
 *  This implements the handshaking described in the data sheet (sec 3.3 in
 *  DS rev 2.0, esp fig 5 flowchart).
 * PRESENTLY A TEST FUNCTION ONLY FOR EMC TEST.
 *
 *  \param none
 *  \return int status of operation (see err_codes.h)
 *  Assumes sDCR1XferBuf structure has the target device in board, chip and core fields
 */
static int iDCR1EMCDataValid(void)
{
    int iResult;

    sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST;
    sDCR1XferBuf.uiReg = E_DCR1_REG_ECR;
    sDCR1XferBuf.uiData = DCR1_ECR_VALID_DATA; // assert DATA_VALID
    (void)iDCR1SpiTransfer(&sDCR1XferBuf);

    delay_us(50); // I have no idea how much to wait; CSS datasheet doesn't say
    sDCR1XferBuf.uiData = 0x0; // deassert DATA_VALID
    iResult = iDCR1SpiTransfer(&sDCR1XferBuf);

    return (iResult);

} // iDCR1EMCDataValid()

/** *************************************************************
 * \brief Pulse Read-Complete internal signal to complete a job to the core(s).
 * This implements the handshaking described in the data sheet (sec 3.3 in DS rev 2.0, esp fig 5 flowchart).
 * This is used after the DONE signal and after reading the nonce data from the fifo.
 * Here this should clear DONEs of all of the cores
 * PRESENTLY A TEST FUNCTION ONLY FOR EMC TEST.
 * \param none
 * \return none
 *  Assumes sDCR1XferBuf structure has the target device in board, chip and core fields
 */
static void DCR1EMCReadComplete(void)
{
    sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST;
    sDCR1XferBuf.uiReg = E_DCR1_REG_ECR;
    sDCR1XferBuf.uiData = DCR1_ECR_READ_COMPLETE; // assert READ_COMPLETE
    (void)iDCR1SpiTransfer(&sDCR1XferBuf);

    delay_us(50); // I have no idea how much to wait; CSS datasheet doesn't say
    sDCR1XferBuf.uiData = 0x0; // deassert READ_COMPLETE
    (void)iDCR1SpiTransfer(&sDCR1XferBuf);

} // DCR1EMCReadComplete()

/** *************************************************************
 * \brief Test hash board operation function that completes the sample job.
 *  Final steps should verify the NONCE/DONE signals and read back the answer from
 *  one asic/core and validate the result.  This also does the read complete handshake
 *  that clears the done.
 *  \param ptr to uint64 to return the solution read back from the device
 *  \return int status of operation (see err_codes.h)
 *  Assumes sDCR1XferBuf structure has the target board field
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 */
static int iDCR1EMCTestJobVerify(uint64_t* puiSolution)
{
    int iResult = ERR_NONE;
    uint8_t uiCoreSave;
    uint16_t uiTestData;
    uint16_t uiTestVerify;
    int ixI;

    if (ERR_NONE == iResult) {
        // Test read of NONCE signals; expecting target bit to be set
        iResult = iReadBoardNonceInts(sDCR1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d reading NONCE status (%d)\r\n", sDCR1XferBuf.uiBoard + 1, iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            uiTestVerify = 0x1; // chip 1 of string should have a nonce
            if (uiTestVerify != (uiTestData & uiTestVerify)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d Chip %d missing NONCE assertion\r\n", sDCR1XferBuf.uiBoard + 1, sDCR1XferBuf.uiChip + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) { // if it detected a nonce above...
        // Read the NONCE data and mask the associated fifo flag(s) in FCR (Fifo control register) to remove NONCE
        sDCR1XferBuf.eMode = E_DCR1_MODE_REG_READ;
        uiCoreSave = sDCR1XferBuf.uiCore;
        sDCR1XferBuf.uiCore = E_DCR1_CHIP_REGS;
        sDCR1XferBuf.uiReg = E_DCR1_REG_IVR;
        sDCR1XferBuf.uiData = 0;
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
        sDCR1XferBuf.uiCore = LAST_DCR1_CORE;
        if (ERR_NONE == iResult) {
            // Verify CORE ID is last core and interrupt vector is 0th fifo location only
            if ((uint16_t)LAST_DCR1_CORE != (((uint16_t)(sDCR1XferBuf.uiData) & E_DCR1_IVR_CORE_ID_MASK) >> DCR1_IVR_CORE_ID_POS)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d Chip %d unexpected Core ID (0x%x)\r\n", sDCR1XferBuf.uiBoard + 1, sDCR1XferBuf.uiChip + 1, (uint16_t)(sDCR1XferBuf.uiData));
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE == iResult)

        if (ERR_NONE == iResult) { // read the data to get the found solution
            sDCR1XferBuf.eMode = E_DCR1_MODE_REG_READ;
            sDCR1XferBuf.uiReg = E_DCR1_REG_FDR0;
            sDCR1XferBuf.uiData = 0;
            iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
            if (ERR_NONE == iResult) {
                *puiSolution = sDCR1XferBuf.uiData; // solution to return
            } // if (ERR_NONE == iResult)
        } // if (ERR_NONE == iResult)

    } // if (ERR_NONE == iResult)

    sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST;
    sDCR1XferBuf.uiReg = E_DCR1_REG_FCR;
    sDCR1XferBuf.uiData = E_DCR1_FCR_MASK_ALL;
    iResult = iDCR1SpiTransfer(&sDCR1XferBuf);

    DCR1EMCReadComplete(); // Acknowledge reading the solution by pulsing Read_Complete (removes Done but not NONCE)

    return (iResult);

} // iDCR1EMCTestJobVerify()
#endif // #ifdef ENABLE_EMC_TEST_FUNCTIONS

#if 0
/** *************************************************************
 *  \brief Support function for basic register write and readback.
 *
 *  \param uint8_t uiHashBoard sets which board to configure
 *  \param uint8_t uiChip determines which ASIC to configure; 0 to LAST_DCR1_CORE-1
 *  \return int status of operation (see err_codes.h)
 *  \retval ERR_NONE if passed
 *  \retval ERR_FAILURE if data read back is all 0's or all 1's
 *  \retval ERR_INVALID_DATA if data read back is mix of 1's and 0's (getting close)
 *
 */
int iDCR1TestReg(uint8_t uiHashBoard, uint8_t uiChip)
{
#define TEST_REG_DATA_VALUE 0x005500CC
    int iResult = ERR_NONE;
    static uint8_t uiTestRegister = (uint8_t)E_DCR1_REG_M0;

    // Write initial value to register
    sDCR1XferBuf.eMode      = E_DCR1_MODE_CHIP_WRITE;
    sDCR1XferBuf.uiBoard    = uiHashBoard;
    sDCR1XferBuf.uiChip     = uiChip;
    sDCR1XferBuf.uiCore     = 0;      // chip wide write
    sDCR1XferBuf.uiReg      = uiTestRegister;
    sDCR1XferBuf.uiData     = (uint32_t)TEST_REG_DATA_VALUE | (uint32_t)uiTestRegister << 8;
    iResult = iDCR1SpiTransfer(&sDCR1XferBuf);

    // Next verify the values
    if (ERR_NONE == iResult) {
        // read back the test register
        sDCR1XferBuf.eMode  = E_DCR1_MODE_REG_READ; // local read from one chip and engine

#if (0 != CSS_DCR1_REVA_WORKAROUND) // workaround for rev A silicon issue on reads; seems to work most of the time
        sDCR1XferBuf.uiReg     = uiTestRegister << CSS_DCR1_REVA_WORKAROUND;  // reg addr must be shifted for reads
#else
        sDCR1XferBuf.uiReg     = uiTestRegister;
#endif
        sDCR1XferBuf.uiCore = 0;

        sDCR1XferBuf.uiData = 0x0;      // hold MOSI fixed low during data part of transfer
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);   // Read to the chip; should block until completed

        if (ERR_NONE == iResult) {
#if (0 != CSS_DCR1_REVA_WORKAROUND)
            sDCR1XferBuf.uiData = sDCR1XferBuf.uiData >> CSS_DCR1_REVA_WORKAROUND;
#endif
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\nTest reg value 0x%x = 0x%08x\r\n", uiTestRegister, sDCR1XferBuf.uiData);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            if (TEST_REG_DATA_VALUE != sDCR1XferBuf.uiData) {
                // if data is all 0's or all 1's send different error code
                // Data is observed to be all 0's if voltage is very low so we shouldn't bother
                // to retry in that case.
                if ((0x0 == sDCR1XferBuf.uiData) || (0xFFFFFFFFF == sDCR1XferBuf.uiData)) {
                    iResult = ERR_FAILURE;
                } else {
                    iResult = ERR_INVALID_DATA;
                }
            }   // if (TEST_REG_DATA_VALUE != sDCR1XferBuf.uiData)
        }   // if (ERR_NONE != iResult)

    }   // if (ERR_NONE == iResult)
    uiTestRegister++;
    if (E_DCR1_REG_M9 < uiTestRegister) {
        uiTestRegister = E_DCR1_REG_M0;
    } else if (E_DCR1_REG_M3_RSV == uiTestRegister) {
        uiTestRegister++;
    }

    return(iResult);

}  // iDCR1TestReg()
#endif // #if 0

// Set the
int iSetDCR1OCRDividerAndBias(uint8_t uiBoard, uint8_t uiDivider, int8_t iVcoBias)
{
    uint64_t uiDividerBits = 0;
    switch (uiDivider) {
    case 1:
        uiDividerBits = DCR1_OCR_CLK_DIV_1;
        break;
    case 2:
        uiDividerBits = DCR1_OCR_CLK_DIV_2;
        break;
    case 4:
        uiDividerBits = DCR1_OCR_CLK_DIV_4;
        break;
    default:
    case 8:
        uiDividerBits = DCR1_OCR_CLK_DIV_8;
        break;
    }

    uint64_t uiVcoBiasBits = 0;

    switch (iVcoBias) {
    case -5: // slowest has 5 bits bits set
        uiVcoBiasBits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_5 << DCR1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -4: // 4 bits slow set
        uiVcoBiasBits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_4 << DCR1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -3: // 3 bits slow set
        uiVcoBiasBits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_3 << DCR1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -2: // 2 bits slow set
        uiVcoBiasBits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_2 << DCR1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -1: // 1 bits slow set
        uiVcoBiasBits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_1 << DCR1_OCR_VCO_BIAS_SLOW_POS;
        break;
    default:
    case 0: // 0 bits bits set; unbiased
        uiVcoBiasBits = 0;
        break;
    case 1: // 1 bit fast set
        uiVcoBiasBits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_1 << DCR1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 2: // 2 bits fast set
        uiVcoBiasBits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_2 << DCR1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 3: // 3 bits fast set
        uiVcoBiasBits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_3 << DCR1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 4: // 4 bits fast set
        uiVcoBiasBits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_4 << DCR1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 5: // 5 bits fast set
        uiVcoBiasBits = (uint64_t)DCR1_OCR_VCO_BIAS_BITS_5 << DCR1_OCR_VCO_BIAS_FAST_POS;
        break;
    } // switch
    // (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "-I- vco bias %d\r\n", iVcoBias);
    // CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

    // Program the oscillator control register; test clock enable bit
    sDCR1XferBuf.eMode = E_DCR1_MODE_MULTICAST;
    sDCR1XferBuf.uiChip = 0; // all chips
    sDCR1XferBuf.uiCore = 0; // all cores
    sDCR1XferBuf.uiBoard = uiBoard;

    uint64_t regValue = (DCR1_OCR_CORE_ENB | uiVcoBiasBits | uiDividerBits);

    // DCR1 has 32-bit registers, so this needs to be written to two registers
    sDCR1XferBuf.uiReg = E_DCR1_REG_OCRA;
    sDCR1XferBuf.uiData = regValue & 0xFFFFFFFF;
    int iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    if (ERR_NONE == iResult) {
        sDCR1XferBuf.uiReg = E_DCR1_REG_OCRB;
        sDCR1XferBuf.uiData = regValue >> 32;
        iResult = iDCR1SpiTransfer(&sDCR1XferBuf);
    }

    return iResult;
} // iSetDCR1OCRDividerAndBias()
