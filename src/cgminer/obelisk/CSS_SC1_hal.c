/**
 * \file CSS_SC1_hal.c
 *
 * \brief CSS ASIC SC1 firmware device driver
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
 * Implements the mid/low level device drivers for the CSS SC1 ASIC access and control via SPI interface.
 * Refer to CSS Blake2B data sheet for more details of the device.
 * This is using the master asynchronous type SPI interface (i.e. using callbacks as a SPI bus master).
 *
 * Based on information from David Vorick (May 24 email):
 * "The 64 bit chips have a 2^64 search space, so the limiter is actually just header updates from the mining pool.
 *  You can set each of the chips to have a different search space, and then send the same header to all of them,
 *  which means instead of feeding a different header to each chip every time there's an update, you can actually
 *  feed the same header to every chip at once. So you are talking about doing 1 header on the whole SPI every 100ms or so.
 *  The 64bit chips iirc can also tune how many zeroes they check for, so we could set that to 40 or 48, meaning that
 *  a chip would only report a solution every few minutes. The whole board would probably be at less than one solution
 *  per second if we tuned it properly."
 *
 * \section Module History
 * - 20180427 Rev 0.1 Started; using AS7 (build 7.0.1417)
 *
 */

/***    INCLUDE FILES       ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h> // PRIxx type printf; 64-bit type support
#include <unistd.h>

#include "Usermain.h"
#include "Console.h" // development console/uart support
#include "SPI_Support.h"
#include "MiscSupport.h" // endian and other stuff
#include "MCP23S17_hal.h"
#include "CSS_SC1Defines.h"
#include "CSS_SC1_hal.h"
#include "EMCTest.h"

/***    LOCAL DEFINITIONS     ***/
#define SC1_TRANSFER_CONTROL_BYTES (3) // SPI transfer has 3 bytes of control
#define SC1_TRANSFER_DATA_BYTES (8) // SPI transfer has 8 bytes of data (64 bit)
#define SC1_TRANSFER_BYTE_COUNT (SC1_TRANSFER_CONTROL_BYTES + SC1_TRANSFER_DATA_BYTES) // num of bytes per SPI transaction

#define SC1_READ_BUFF_SIZE 16 // max bytes size we expect to have to transfer to the device (plus a little more)
#define SC1_WRITE_BUFF_SIZE SC1_READ_BUFF_SIZE

#define OCR37_CORE63_CLKENB (SC1_OCR_CORE15 | SC1_OCR_INIT_VAL) // core 63 enable for OCR at 0x37
//#define OCR_ALL_CORE_FAST_CLKENB   (SC1_OCR_CORE_ENB | SC1_OCR_SLOW_VAL)  // all cores at fast rate
#define OCR_ALL_CORE_FAST_CLKENB (SC1_OCR_CORE_ENB | SC1_OCR_DIV4_VAL) //NOTE: Observed to overheat if set to DIV2 or DIV1

// Set bias up one notch
//#define OCR_ALL_CORE_FAST_CLKENB   (SC1_OCR_CORE_ENB | SC1_OCR_DIV4_VAL | (SC1_OCR_VCO_BIAS_BITS_1 << SC1_OCR_VCO_BIAS_FAST_POS))

#define NONCE_BITS_MASK 0x7FFF // port expander valid bits
#define DONE_BITS_MASK 0x7FFF // port expander valid bits

// Sample jobs provided in the SC1 datasheet
#define MAX_SAMPLE_JOBS 3
// Job sample #1; (modifed from datasheet; original datasheet values as comments)
#define SAMPLE1_LB 0x000000000B609366 // 0x000000000B609366
#define SAMPLE1_UB 0x000000000B609386
#define SAMPLE1_M0 0x1E63000000000000
#define SAMPLE1_M1 0xF78B797E5D753F6F
#define SAMPLE1_M2 0xE0E21B09975ADACE
#define SAMPLE1_M3 0x923C26B1B437E95D
#define SAMPLE1_M5 0x0000000056307FC8
#define SAMPLE1_M6 0x1023C71D7FB4A5EB
#define SAMPLE1_M7 0x459779BFC18FFED4
#define SAMPLE1_M8 0x10EEA15DE88CAF87
#define SAMPLE1_M9 0xCC2F2CEDD68DB7C6
#define SAMPLE1_FDR0 0x000000000B609376

// Job sample #2
#define SAMPLE2_LB 0x0000000000173300
#define SAMPLE2_UB 0x0000000000173400
#define SAMPLE2_M0 0x6F34000000000000
#define SAMPLE2_M1 0xC24A197EF68BB88B
#define SAMPLE2_M2 0x5D04905F16B44B06
#define SAMPLE2_M3 0x5FD01A37AB974684
#define SAMPLE2_M5 0x00000000557E177C
#define SAMPLE2_M6 0xA7791B442A204E89
#define SAMPLE2_M7 0x8C7FD7617FB56B67
#define SAMPLE2_M8 0xA80CD66DCA3D6CFB
#define SAMPLE2_M9 0x6B8C74CBBB4D92C8
#define SAMPLE2_FDR0 0x0000000000173398

// Job sample #3
#define SAMPLE3_LB 0x0000000002C41900
#define SAMPLE3_UB 0x0000000002C41A00
#define SAMPLE3_M0 0x1A87010000000000
#define SAMPLE3_M1 0x29204EF569A84C18
#define SAMPLE3_M2 0x6338BAE980D39C75
#define SAMPLE3_M3 0x3915CD992103A086
#define SAMPLE3_M5 0x00000000557E2629
#define SAMPLE3_M6 0xF33F77F1E2FC8AFB
#define SAMPLE3_M7 0x8A0A35E84301372F
#define SAMPLE3_M8 0xC8DC262408A96147
#define SAMPLE3_M9 0x643F1871F5DD5EC1
#define SAMPLE3_FDR0 0x0000000002C4194D

/***    LOCAL STRUCTURES/ENUMS      ***/

static const uint64_t ullVRegTable[16] = {
    SC1_V00_VAL,
    SC1_V01_VAL,
    SC1_V02_VAL,
    SC1_V03_VAL,
    SC1_V04_VAL,
    SC1_V05_VAL,
    SC1_V06_VAL,
    SC1_V07_VAL,
    SC1_V08_VAL,
    SC1_V09_VAL,
    SC1_V10_VAL,
    SC1_V11_VAL,
    SC1_V12_VAL,
    SC1_V13_VAL,
    SC1_V14_VAL,
    SC1_V15_VAL
};

/***    LOCAL FUNCTION PROTOTYPES   ***/

static int iSC1CmdPreLoadJob(S_SC1_JOB_T* psSC1Job);
static int iSC1CmdStartJob(void);
static int iSC1CmdLoadRange(S_SC1_JOB_T* psSC1Job);
static int iSC1CmdDataValid(void);
static void SC1CmdReadComplete(void);

static int iSC1TestJobVerify(uint64_t* puiSolution);
static void SC1GetTestJob(uint8_t uiSampleNum);

static int iSC1EMCDataValid(void);
static int iSC1EMCTestJobVerify(uint64_t* puiSolution);
static int iSC1EMCStartJob(void);

/***    LOCAL DATA DECLARATIONS     ***/
static S_SC1_JOB_T sSC1JobBuf;
static S_SC1_TRANSFER_T sSC1XferBuf; // Local buffer for SPI transfer to/from ASIC

static uint8_t ucaSC1OutBuf[SC1_WRITE_BUFF_SIZE]; // buffer for sending out
static uint8_t ucaSC1InBuf[SC1_READ_BUFF_SIZE]; // buffer for reading in

/***    GLOBAL DATA DECLARATIONS   ***/

HashJobInfo hashJobInfo[MAX_NUMBER_OF_HASH_BOARDS];

/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/

//  CSS_SC1_REVA_WORKAROUND needs to be defined to control how the asics are read
#ifndef CSS_SC1_REVA_WORKAROUND
#error "CSS_SC1_REVA_WORKAROUND not defined"
#endif

/** *************************************************************
 *  \brief SC1 type ASIC String Startup Method
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
 *  \return int status of operation (see err_codes.h)
 */
int iSC1StringStartup(uint8_t uiBoard)
{
#define OCR_CONFIG_MAX_RETRIES 1
// Set EPOT_STARTUP_VALUE to a high value (low string voltage) to see how the string starts up. This should allow you to see
// the e-pot value as each asic starts passing the test.
#define EPOT_STARTUP_VALUE 30 // will auto-step up voltage using this as starting value of epot
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
    int iResult = ERR_NONE;
    uint16_t uiAsicTestFlags, uiAsicAwakeCnt;

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

                for (ixJ = 0; ixJ <= LAST_SC1_CHIP; ixJ++) {
                    uiRetryCnt = 0;
                    do { // while (OCR_CONFIG_MAX_RETRIES > ++uiRetryCnt);
                        if (1 <= uiRetryCnt) { // put out '-' if a retry
                            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "."); // indicate a retry
                            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        }
                        iResult = iStartupOCRInitValue(uiUUT, ixJ);
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
                } // for (ixJ = 0; ixJ <= LAST_SC1_CHIP; ixJ++)

                // Check how many asics we got working.
                if (MAX_SC1_CHIPS_PER_STRING <= uiAsicAwakeCnt) { // if all passed we are done.
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
                } // if (MAX_SC1_CHIPS_PER_STRING <= uiAsicAwakeCnt)

            } // while (false == bDone)

            if (ASIC_TEST_FLAGS_PASS != uiAsicTestFlags) {
                (void)iSetPSEnable(uiUUT, false); // turn off the string on failure to launch all OCRs
                iSetBoardStatus(uiUUT, ERR_FAILURE);
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d OCR initialize FAIL (%d of %d)\r\n",
                    (uiUUT + 1), (MAX_SC1_CHIPS_PER_STRING - uiAsicAwakeCnt), MAX_SC1_CHIPS_PER_STRING);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                for (ixJ = 0; ixJ <= LAST_SC1_CHIP; ixJ++) {
                    uiTestData = 0x1 << ixJ;
                    if (uiTestData != (uiAsicTestFlags & uiTestData)) {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "Chip %d failed initialization\r\n", ixJ + 1);
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    }
                } // for (ixJ = 0; ixJ <= LAST_SC1_CHIP; ixJ++)
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n");
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            } else {
                // step up the voltage a little more so we don't leave the string just on the edge of operation
                if (EPOT_LOW_LIMIT <= (uiEpotCount - SC1_EPOT_BOOST_COUNT)) {
                    uiEpotCount -= SC1_EPOT_BOOST_COUNT;
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
                // If voltage tests above pass; verify we can write and readback register(s) in each ASIC.  This is
                // an extra confidence check to confirm that SPI comms to the ASIC is working and that the ASIC has enough
                // voltage to be awake.  We don't bother with retries for this test.
                uiAsicTestFlags = 0;
                for (ixJ = 0; ixJ <= LAST_SC1_CHIP; ixJ++) {
                    iResult = iSC1MRegCheck(uiUUT, ixJ); // function to write and readback verify register(s)
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
                } // for (ixJ = 0; ixJ <= LAST_SC1_CHIP; ixJ++)
#endif

                if (MAX_SC1_CHIPS_PER_STRING <= uiAsicAwakeCnt) { // if we passed the above initialization...
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_INFO "HB%d string communications PASSED\r\n", uiUUT + 1);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                } else {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d: %d of %d chips FAILED write/read test (0x%x)\r\n",
                        uiUUT + 1, (MAX_SC1_CHIPS_PER_STRING - uiAsicAwakeCnt), MAX_SC1_CHIPS_PER_STRING, uiAsicTestFlags);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    for (ixJ = 0; ixJ <= LAST_SC1_CHIP; ixJ++) {
                        uiTestData = 0x1 << ixJ;
                        if (uiTestData != (uiAsicTestFlags & uiTestData)) {
                            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "Chip %d failed readback\r\n", ixJ + 1);
                            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        }
                    } // for (ixJ = 0; ixJ <= LAST_SC1_CHIP; ixJ++)
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
} // iSC1StringStartup(void)

/** *************************************************************
 *  \brief Support function for the initial start up of ASICs.  Does write and
 *  read-back tests on the ASIC M-registers.  This tests the ASIC is listening and responds
 *  correctly. For example, if ASIC core voltage is too low then it will not read back
 *  data written to it properly.  If this fails, it should return as quickly as possible.
 *  For each register tested in the chip, write to all cores and read back the register to
 *  verify the data. Will retry the read but not retry the write.
 *
 *  \param uint8_t uiHashBoard sets which board to configure
 *  \param uint8_t uiChip determines which ASIC to configure; 0 to LAST_SC1_CORE
 *  \return int status of operation (see err_codes.h)
 */
int iSC1MRegCheck(uint8_t uiHashBoard, uint8_t uiChip)
{
#define TEST_PASSES 4
#define REG_TO_START_TEST E_SC1_REG_M0
#define REG_TO_END_TEST E_SC1_REG_M2
#define DATA_TO_START_TEST (0x55AACC99F00F6688)
#define CORES_TO_VERIFY MAX_NUMBER_OF_SC1_CORES // 1 to MAX_NUMBER_OF_SC1_CORES
#define MAX_RETRY_MREG_CHECK 5
    uint8_t uixI, uiDataValue, uixRegUnderTest;
    uint64_t uiTestDataPattern;
    int iRetry;
    int iResult = ERR_NONE;

    for (uixRegUnderTest = REG_TO_START_TEST; uixRegUnderTest <= REG_TO_END_TEST; uixRegUnderTest++) {
        if (E_SC1_REG_M4_RSV != uixRegUnderTest) { // skip M4 register

            uiTestDataPattern = DATA_TO_START_TEST;
            for (uiDataValue = 0; uiDataValue < TEST_PASSES; uiDataValue++) {
                sSC1XferBuf.eMode = E_SC1_MODE_CHIP_WRITE; // chip wide write should do all cores
                sSC1XferBuf.uiBoard = uiHashBoard;
                sSC1XferBuf.uiChip = uiChip;
                sSC1XferBuf.uiCore = 0; // chip wide write
                sSC1XferBuf.eRegister = uixRegUnderTest;
                sSC1XferBuf.uiData = uiTestDataPattern;

                iResult = iSC1SpiTransfer(&sSC1XferBuf);

                // and verify the values
                if (ERR_NONE == iResult) {
                    // read back the register from each asic
                    sSC1XferBuf.eMode = E_SC1_MODE_REG_READ; // local read from one chip and engine
                    for (uixI = 0; uixI < CORES_TO_VERIFY; uixI++) {

#if (0 != CSS_SC1_REVA_WORKAROUND) // workaround for rev A silicon issue on reads
                        sSC1XferBuf.eRegister = uixRegUnderTest << CSS_SC1_REVA_WORKAROUND; // reg addr must be shifted for reads
#else
                        sSC1XferBuf.eRegister = uixRegUnderTest;
#endif
                        sSC1XferBuf.uiCore = uixI; // core to test

                        iRetry = 0;
                        while (MAX_RETRY_MREG_CHECK > iRetry) {
                            sSC1XferBuf.uiData = 0; // hold MOSI fixed low during data part of transfer
                            iResult = iSC1SpiTransfer(&sSC1XferBuf); // Read to the chip; should block until completed

                            if (ERR_NONE != iResult) {
                                break; // break while if SPI transfer error
                            } else {

#if (0 != CSS_SC1_REVA_WORKAROUND) // workaround for rev A silicon issue on reads
                                sSC1XferBuf.uiData = sSC1XferBuf.uiData >> CSS_SC1_REVA_WORKAROUND;
#endif
                                if (uiTestDataPattern == sSC1XferBuf.uiData) {
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

        } // if (E_SC1_REG_M4_RSV != uixRegUnderTest)
    } // for (uixRegUnderTest = REG_TO_START_TEST; uixRegUnderTest < REG_TO_END_TEST; uixRegUnderTest++)

    return (iResult);

} // iSC1MRegCheck()

/** *************************************************************
 *  \brief Support function for the initial start up of ASICs.  Loads the
 *   four oscillator control registers in the ASIC with a startup value.  Then
 *   verifies it can read the value back.  This is an integrity check to ensure we
 *   can control the oscillators and it gives them an initial defined value.  These
 *   were observed to wake up randomly and START_CLK should not be asserted until
 *   these are a set with a proper value.
 *
 *  \param uint8_t uiHashBoard sets which board to configure
 *  \param uint8_t uiChip determines which ASIC to configure; 0 to LAST_SC1_CORE
 *  \return int status of operation (see err_codes.h)
 *  \retval ERR_NONE if passed
 *  \retval ERR_FAILURE if data read back is all 0's or all 1's
 *  \retval ERR_INVALID_DATA if data read back is mix of 1's and 0's (getting close)
 *
 * Note: The OCR is tested by CSS on each device and all asics should pass this test.
 */
int iStartupOCRInitValue(uint8_t uiHashBoard, uint8_t uiChip)
{
#define NUM_OSC_IN_A_CHIP 4
    int ixI;
    int iResult = ERR_NONE;

    // Set initial value
    sSC1XferBuf.eMode = E_SC1_MODE_CHIP_WRITE; // chip wide write should do all oscillators
    sSC1XferBuf.uiBoard = uiHashBoard;
    sSC1XferBuf.uiChip = uiChip;
    sSC1XferBuf.uiCore = 0; // chip wide write
    sSC1XferBuf.eRegister = E_SC1_REG_OCR;
    sSC1XferBuf.uiData = SC1_OCR_STARTUP_VAL;

    iResult = iSC1SpiTransfer(&sSC1XferBuf);

    // Next verify the values
    if (ERR_NONE == iResult) {
        // read back the OCR from each of the oscillator groups; later we may only need to do one of these
        // since they are on the same chip but it will be useful to initially be aggressive with verification.
        sSC1XferBuf.eMode = E_SC1_MODE_REG_READ; // local read from one chip and engine

#if (0 != CSS_SC1_REVA_WORKAROUND) // workaround for rev A silicon issue on reads; seems to work most of the time
        sSC1XferBuf.eRegister = E_SC1_REG_OCR << CSS_SC1_REVA_WORKAROUND; // reg addr must be shifted for reads
#else
        sSC1XferBuf.eRegister = E_SC1_REG_OCR;
#endif

        // KENC: FOR QUICKER STARTUP, JUST TALK TO ONE OCR - THEY ARE 99.9% likely all OK
        for (ixI = 0; ixI < 1; ixI++) {
            // for (ixI = 0; ixI < NUM_OSC_IN_A_CHIP; ixI++) {
            switch (ixI) {
            default: // because we need a default on every switch
            case 0:
                sSC1XferBuf.uiCore = SC1_OCR_OSC_GROUP1;
                break;
            case 1:
                sSC1XferBuf.uiCore = SC1_OCR_OSC_GROUP2;
                break;
            case 2:
                sSC1XferBuf.uiCore = SC1_OCR_OSC_GROUP3;
                break;
            case 3:
                sSC1XferBuf.uiCore = SC1_OCR_OSC_GROUP4;
                break;
            }

            sSC1XferBuf.uiData = 0x0; // hold MOSI fixed low during data part of transfer
            iResult = iSC1SpiTransfer(&sSC1XferBuf); // Read to the chip; should block until completed

#if (0 != CSS_SC1_REVA_WORKAROUND)
            sSC1XferBuf.uiData = sSC1XferBuf.uiData >> CSS_SC1_REVA_WORKAROUND;
#endif

            if (ERR_NONE != iResult) {
                break; // for loop; stop if we get a fail on transfer
            } else {
                if (SC1_OCR_STARTUP_VAL != sSC1XferBuf.uiData) {
                    // if data is all 0's or all 1's send different error code
                    // Data is observed to be all 0's if voltage is very low so we shouldn't bother
                    // to retry in that case.
                    if ((0x0 == sSC1XferBuf.uiData) || (0xFFFFFFFFFFFFFFFF == sSC1XferBuf.uiData)) {
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

} // iStartupOCRInitValue()

/** *************************************************************
 *  \brief SC1 ASIC Device Initialize Method
 *  \param uint8_t uiBoard sets which board to configure; if MAX_NUMBER_OF_HASH_BOARDS or higher then do all
 *  \return int status of operation (see err_codes.h)
 *
 *  Called once at startup or on demand. Resets and programs the device(s) for a default configuration.
 *  Assumes the associated SPI port is initialized.  Assumes the ASICs have been started up and verified they
 *  work, at proper voltage, etc. Uses multicast to send commands to all cores and chips at the same time.
 *  See CSS Blake2B data sheet section 3.5; the ASIC does not have a hardware reset.  This inializes the various
 *  registers and loads in the initial values to define the BLAKE2B operation.
 *  1) Disable any MCU interrupts associated with DONE/NONCE; none on test SPI board
 *  2) Mask engines' nonce interrupt outputs with Fifo control register
 *  3) Set SC1 START_CLK high; global to all chips to disable clocking of oscillators
 *  4) Set up the oscillator control register (OCR); using slow clocking to start and disable clocks to all cores
 *  5) Reset Engine Control Register (ECR)
 *  6) Set limits register to 0x31 (as per datasheet for SC1)
 *  7) Set step register to 0x01 (defines search step)
 *  8) Set lower bound register to 0x0
 *  9) Set upper bound register to 0x0
 * 10) Set V registers (V0 Match, V8 Match, V00 - V15) to defaults from data sheet
 * 11) Set M registers to 0x0
 * 12) Set ECR reg engine reset and spi reset bits to 1; should reset all cores
 * 13) Set SC1 START_CLK low; will start clocking and reset cores
 * 14) Set ECR reg to remove resets
 * 15) Disable clocking to any cores we won't be using; set all chips the same though
 * 16) Verify DONE/NONCE status signal from GPIO expander before starting any jobs
 */
int iSC1DeviceInit(uint8_t uiBoard)
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
        uiUUT = (uint8_t)uixBoardCnt;
        iResult = iIsBoardValid(uiUUT, true);
        if (ERR_NONE == iResult) { // board must be there to startup and should have passed initial testing
            sSC1XferBuf.uiBoard = uiUUT;

            // *************
            // 1) Disable any MCU interrupts associated with DONE/NONCE; none on test SPI board
            // < insert code here >     (none presently enabled on board)

            // *************
            // 2) Mask engines' nonce interrupt outputs with Fifo control register
            if (ERR_NONE == iResult) {
                iConfigStep++;
                // Mask engine interrupts in FCR (Fifo control register)
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_FCR;
                sSC1XferBuf.uiData = E_SC1_FCR_MASK_ALL;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 3) Set SC1 START_CLK high; global to all chips to disable clocking of oscillators
            (void)iSetHashClockEnable(uiUUT, false); // Deassert START_CLK (HASHENB)

            // *************
            // 4) Set up the oscillator control register (OCR); using slow clocking to start
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_OCR;
                sSC1XferBuf.uiData = (SC1_OCR_SLOW_VAL | SC1_OCR_CORE_ENB); // At startup, we clock all cores; later we may turn off some
                // Not actually clocking cores yet until we enable hashing clock signal
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 5) Reset Engine Control Register (ECR) on all engines and chips
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_ECR;
                sSC1XferBuf.uiData = 0;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 6) Set limits register to 0x31 (as per datasheet for SC1)
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_LIMITS;
                sSC1XferBuf.uiData = SC1_LIMITS_VAL;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 7) Set step register to 0x01 (defines search step)
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_STEP;
                sSC1XferBuf.uiData = SC1_STEP_VAL;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 8) Set lower bound register to 0x0
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_LB;
                sSC1XferBuf.uiData = 0;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 9) Set upper bound register to 0x0
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_UB;
                sSC1XferBuf.uiData = 0;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)

            // *************
            // 10) Set V registers (V0 - V15, V0 Match, V8 Match) to defaults from data sheet
            for (uixI = E_SC1_REG_V00; uixI <= E_SC1_REG_V15; uixI++) {
                if (ERR_NONE == iResult) {
                    sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                    sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                    sSC1XferBuf.uiCore = 0;
                    sSC1XferBuf.eRegister = uixI;
                    sSC1XferBuf.uiData = ullVRegTable[uixI];
                    iResult = iSC1SpiTransfer(&sSC1XferBuf);
                } // if (ERR_NONE == iResult)
            } // for
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_V0MATCH;
                sSC1XferBuf.uiData = SC1_V0_MATCH_VAL;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)
            if (ERR_NONE == iResult) {
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_V8MATCH;
                sSC1XferBuf.uiData = SC1_V8_MATCH_VAL;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
                iConfigStep++;
            } // if (ERR_NONE == iResult)

            // *************
            // 11) Set M registers to 0x0; For SC1, addresses are 0x10 to 0x19
            sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
            sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
            sSC1XferBuf.uiCore = 0;

            for (uixI = E_SC1_REG_M0; uixI <= E_SC1_REG_M9; uixI++) {
                if (E_SC1_REG_M4_RSV != uixI) { // skip M4 on SC1 ASIC
                    sSC1XferBuf.eRegister = uixI;
                    sSC1XferBuf.uiData = 0;
                    iResult = iSC1SpiTransfer(&sSC1XferBuf);
                    if (ERR_NONE != iResult) {
                        break; // for
                    }
                } // if
            } // for

            // *************
            // 12) Set ECR reg engine reset and spi reset bits to 1; should reset all cores
            if (ERR_NONE == iResult) {
                iConfigStep++; // because we finished the prior step #11 above
                // Set ECR: Reset core (bit 3) and reset cores' SPI FSM (bit 0) high
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_ECR;
                sSC1XferBuf.uiData = E_SC1_ECR_RESET_SPI_FSM | E_SC1_ECR_RESET_CORE;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)

            // 13) Set SC1 START_CLK low; will start clocking and reset cores
            if (ERR_NONE == iResult) {
                iConfigStep++;
                iResult = iSetHashClockEnable(uiUUT, true); // Assert START_CLK to begin clocking oscillators and cores
            } // if (ERR_NONE == iResult)

            // 14) Set ECR reg to release the resets while cores are clocked to reset all cores
            if (ERR_NONE == iResult) {
                iConfigStep++;
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_ECR;
                sSC1XferBuf.uiData = 0;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
                // not sure how much to delay for reset; best guess this is ok
                delay_ms(20);
            } // if (ERR_NONE == iResult)

            iResult = iSetHashClockEnable(uiUUT, false); // Deassert START_CLK (HASHENB)

            // *************
            // 15) Disable clocking to any cores we won't be using; set all chips the same though
            if (ERR_NONE == iResult) {
                iConfigStep++;
                // For test purposes, we limit power/heat we are only enabling at most 1 core per ASIC
                // Disable all core clocks first
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0x00;
                sSC1XferBuf.uiCore = 0x00;
                sSC1XferBuf.eRegister = E_SC1_REG_OCR;
                sSC1XferBuf.uiData = SC1_OCR_SLOW_VAL; // mask off clocking to all cores
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)

            if (ERR_NONE == iResult) {
                // Now enable only those we want;
                // Enabling 64th core of asics of the string for now instead of all
                // #TODO change this to enable all the cores once we have the proper power supply/heat sinks
                sSC1XferBuf.eMode = E_SC1_MODE_REG_WRITE; // set oscillator on group of core we want
                sSC1XferBuf.uiCore = SC1_OCR_OSC_GROUP4; // upper group only
                sSC1XferBuf.eRegister = E_SC1_REG_OCR;
                for (uixI = 0; uixI <= MAX_SC1_CHIPS_PER_STRING; uixI++) {
                    sSC1XferBuf.uiChip = uixI;
                    sSC1XferBuf.uiData = OCR37_CORE63_CLKENB;
                    iResult = iSC1SpiTransfer(&sSC1XferBuf);
                    if (ERR_NONE != iResult) {
                        break; // for
                    }
                } // for
            } // if (ERR_NONE == iResult)

            if (ERR_NONE == iResult) {
                iResult = iSetHashClockEnable(uiUUT, true); // Assert START_CLK to begin clocking oscillators and cores
            }
            // *************

            // 16) Verify there are no DONE or NONCE assertions since we have not yet submitted any jobs
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
                sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines
                sSC1XferBuf.uiChip = 0; // chip and core is don't care (set to 0's)
                sSC1XferBuf.uiCore = 0;
                sSC1XferBuf.eRegister = E_SC1_REG_FCR;
                sSC1XferBuf.uiData = E_SC1_FCR_MASK_ALL;
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
            } // if (ERR_NONE == iResult)

            if (ERR_NONE != iResult) {
                iRetval = iResult; // return error if we detect any valid boards had a problem
                iSetBoardStatus(uiUUT, ERR_FAILURE);
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d SC1 Failed initial configuration (%d)\r\n", uiUUT + 1, iConfigStep);
            } else {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INFO "HB%d SC1 Initialized Successfully\r\n", uiUUT + 1);
            }
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

        } // if (ERR_NONE == iResult)

    } // for (uixBoardCnt = iStart; uixBoardCnt <= iEnd; uixBoardCnt++)

    return (iRetval);
} // int iSC1DeviceInit(uint8_t uiBoard)

/** *************************************************************
 * \brief Load job to ASIC; doesn't care if the chip(s)/core(s) are idle or not.
 *  This is just pre-loading the next job and does not start the computation.
 *  As described in the datasheet, this allows a job to be pre-loaded while
 *  computing a job so the next computation can be started quickly.
 *  \param psSC1Job is ptr to struct with job parameters
 *  \return int status of operation (see err_codes.h)
 *  Assumes sSC1XferBuf structure has the target device in board, chip and core fields
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 * #TODO Make this more generic and public so it can load jobs on the real board.
 */
static int iSC1CmdPreLoadJob(S_SC1_JOB_T* psSC1Job)
{
    int ixI;
    int iResult;
    E_SC1_CORE_REG_T eReg;

    // Load in the bounds
    // #TODO: in the real system loading the bounds may be done separately and only done once and then
    // the M-regs are changed only for new jobs. The method is yet to be determined.
    iResult = iSC1CmdLoadRange(psSC1Job);

    if (ERR_NONE == iResult) {
        sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and cores on the board
        // Load the job parameters to the core(s) M registers
        // For test, DONE/NONCE should be inactive (low) here before we start.
        // For production board, DONE/NONCE might be asserted (high) if some
        // other cores are active.  Therefore, we are not checking them.
        // Multiple cores can get the same M0-M9 parameter with different upper/lower bounds.
        eReg = E_SC1_REG_M0; // starting value; M-regs in SC1 are contiguously addressed
        for (ixI = 0; ixI < E_SC1_NUM_MREGS; ixI++, eReg++) {
            if (E_SC1_REG_M4_RSV != eReg) { // skip M4 on SC1 ASIC
                sSC1XferBuf.eRegister = eReg;
                sSC1XferBuf.uiData = psSC1Job->uiaMReg[ixI];
                iResult = iSC1SpiTransfer(&sSC1XferBuf);
                if (ERR_NONE != iResult) {
                    break; // for
                }
            }
        } // for
    } // if (ERR_NONE == iResult)
    return (iResult);
} // iSC1CmdPreLoadJob()

/** *************************************************************
 * \brief Placeholder function to load the lower and upper bound regs of the job.
 *  \param psSC1Job is ptr to struct with job parameters
 *  \return int status of operation (see err_codes.h)
 *  Assumes sSC1XferBuf structure has the target device in board, chip and core fields
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 *  Not sure if this will spread a larger search space across the multiple cores/ASICs or
 *  if cgminer does that with function calls.
 * #TODO Make this more generic and public so it can submit jobs on the real board.
 */
static int iSC1CmdLoadRange(S_SC1_JOB_T* psSC1Job)
{
    int iResult;
    // Determine what boards, chips and cores to transfer to.
    // For initial testing we are loading all chips/cores of one board with the same bounds.
    sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST; // global write to all chips and engines

    // For SC1 the cores in an ASIC are likely get separate LB and UB with full range spread across
    // multiple cores/chips.
    // Using same test bounds on all ASICs for test purposes here.
    sSC1XferBuf.eRegister = E_SC1_REG_LB;
    sSC1XferBuf.uiData = psSC1Job->uiLBReg;
    iResult = iSC1SpiTransfer(&sSC1XferBuf);

    if (ERR_NONE == iResult) {
        sSC1XferBuf.eRegister = E_SC1_REG_UB;
        sSC1XferBuf.uiData = psSC1Job->uiUBReg;
        iResult = iSC1SpiTransfer(&sSC1XferBuf);
    }
    return (iResult);

} // iSC1CmdLoadRange()

/** *************************************************************
 *  \brief Start job to ASIC; the ASIC/cores being started should be idle and the job should
 *  be pre-loaded and ready to start.  This will reset the resources and handshake the Data_Valid.
 *  \param none
 *  \return int status of operation (see err_codes.h)
 *  Assumes sSC1XferBuf structure has the target device in board, chip and core fields
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 * #TODO Make this more generic and public so it can start jobs on the real board.
 */
static int iSC1CmdStartJob(void)
{
    int iResult;
    uint16_t uiTestData, uiTestVerify;

    // Reset the core(s) FIFO memory so it can get new solutions and clear any prior NONCE(s)
    sSC1XferBuf.eMode = E_SC1_MODE_REG_WRITE;
    sSC1XferBuf.eRegister = E_SC1_REG_ECR;
    sSC1XferBuf.uiData = E_SC1_ECR_RESET_SPI_FSM | E_SC1_ECR_RESET_CORE;
    iResult = iSC1SpiTransfer(&sSC1XferBuf); // Set ECR: MRST (bit 3) and reset error SPI (bit 0) high
    if (ERR_NONE == iResult) {
        sSC1XferBuf.uiData = 0;
        iResult = iSC1SpiTransfer(&sSC1XferBuf); // Set ECR: MRST (bit 3) and reset error SPI (bit 0) low
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Unmask the bits that define the nonce fifo masks for the desired target
        sSC1XferBuf.eMode = E_SC1_MODE_REG_WRITE;
        sSC1XferBuf.eRegister = E_SC1_REG_FCR;
        sSC1XferBuf.uiData = 0; // clear all mask bits to unmask
        iResult = iSC1SpiTransfer(&sSC1XferBuf);
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Test read of NONCE signals; ; expecting target bit to be clear
        iResult = iReadBoardNonceInts(sSC1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "reading NONCE status (%d)\r\n", iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            uiTestVerify = 0x1 << sSC1XferBuf.uiChip;
            if (0 != (uiTestData & uiTestVerify)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "unexpected NONCE assertion (0x%x)\r\n", uiTestData);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Test read of DONE signals; ; expecting target bit to be clear
        iResult = iReadBoardDoneInts(sSC1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "reading DONE status (%d)\r\n", iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            uiTestVerify = 0x1 << sSC1XferBuf.uiChip;
            if (0 != (uiTestData & uiTestVerify)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "unexpected DONE assertion (0x%x)\r\n", uiTestData);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        iResult = iSC1CmdDataValid(); // submit job(s) by pulsing Data_Valid
    } // if (ERR_NONE == iResult)
    return (iResult);

} // iSC1CmdStartJob()

/** *************************************************************
 * \brief Test hash board operation function that completes the sample job. Final steps should
 *  verify the NONCE/DONE signals and read back the answer(s) and validate the result.
 *  \param ptr to uint64 to return the solution read back from the device
 *  \return int status of operation (see err_codes.h)
 *  Assumes sSC1XferBuf structure has the target device in board, chip and core fields
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 * #TODO Make this more generic and public so it can start job on the real board.
 * Assumes the board, chip and core are already set in the transfer buffer structure.
 */
static int iSC1TestJobVerify(uint64_t* puiSolution)
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
    iResult = iReadBoardDoneInts(sSC1XferBuf.uiBoard, &uiTestData);
    if (ERR_NONE != iResult) {
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "reading DONE status (%d)\r\n", iResult);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
    } else {
        uiTestVerify = 0x1 << sSC1XferBuf.uiChip;
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
        iResult = iReadBoardNonceInts(sSC1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "reading NONCE status (%d)\r\n", iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            uiTestVerify = 0x1 << sSC1XferBuf.uiChip;
            if (uiTestVerify != (uiTestData & uiTestVerify)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d Chip %d missing NONCE assertion\r\n", sSC1XferBuf.uiBoard + 1, sSC1XferBuf.uiChip + 1);
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
        // #TODO add in the code to read the nonce; should verify we got only one nonce for the sample job
        sSC1XferBuf.eMode = E_SC1_MODE_REG_READ;
        uiCoreSave = sSC1XferBuf.uiCore;
        sSC1XferBuf.uiCore = E_SC1_CHIP_REGS;
        sSC1XferBuf.eRegister = E_SC1_REG_IVR;
        sSC1XferBuf.uiData = 0;
        iResult = iSC1SpiTransfer(&sSC1XferBuf);
        sSC1XferBuf.uiCore = uiCoreSave;
        if (ERR_NONE == iResult) {
            // Verify CORE ID is last core and interrupt vector is 0th fifo location only
            if ((uint16_t)LAST_SC1_CORE != (((uint16_t)(sSC1XferBuf.uiData) & E_SC1_IVR_CORE_ID_MASK) >> SC1_IVR_CORE_ID_POS)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d Chip %d unexpected Core ID\r\n", sSC1XferBuf.uiBoard + 1, sSC1XferBuf.uiChip + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_FAILURE;
            } else {
                // should only have found a single solution
                if ((uint16_t)E_SC1_FCR_MASK_D0 != (uint16_t)(sSC1XferBuf.uiData & E_SC1_IVR_INT_MASK)) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d Chip %d unexpected Fifo data\r\n", sSC1XferBuf.uiBoard + 1, sSC1XferBuf.uiChip + 1);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    iResult = ERR_FAILURE;
                }
            }
        } // if (ERR_NONE == iResult)

        if (ERR_NONE == iResult) { // read the data to get the found solution
            sSC1XferBuf.eMode = E_SC1_MODE_REG_READ;
            sSC1XferBuf.eRegister = E_SC1_REG_FDR0;
            sSC1XferBuf.uiData = 0;
            iResult = iSC1SpiTransfer(&sSC1XferBuf);
            if (ERR_NONE == iResult) {
                *puiSolution = sSC1XferBuf.uiData;
            } // if (ERR_NONE == iResult)
        } // if (ERR_NONE == iResult)

    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) { // mask the fifo data flags to remove the NONCE signal
        sSC1XferBuf.eMode = E_SC1_MODE_REG_WRITE;
        sSC1XferBuf.eRegister = E_SC1_REG_FCR;
        sSC1XferBuf.uiData = E_SC1_FCR_MASK_ALL;
        iResult = iSC1SpiTransfer(&sSC1XferBuf);
    } // if (ERR_NONE == iResult)

    // For test purposes we are checking that masking above cleared the NONCE signal.  In this case it is testing one asic so the other
    // asics should not be active.  We would not do this check in normal operation except perhaps to test the specific asic we just
    // completed.
    if (ERR_NONE == iResult) {
        // Test read of NONCE signals to verify they are cleared
        iResult = iReadBoardNonceInts(sSC1XferBuf.uiBoard, &uiTestData);
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

    SC1CmdReadComplete(); // Acknowledge reading the solution by pulsing Read_Complete (removes Done but not NONCE)

    // For test purposes we are checking that steps above cleared the DONE signal.  In this case it is testing one asic so the other
    // asics should not be active.  We would not do this check in normal operation except perhaps to test the specific asic we just
    // completed.
    if (ERR_NONE == iResult) {
        // Test read of DONE signals to verify they are cleared
        iResult = iReadBoardDoneInts(sSC1XferBuf.uiBoard, &uiTestData);
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

} // iSC1TestJobVerify()

/** *************************************************************
 * \brief Pulse Data-Valid (internal signal) to submit a job to the core(s).  This implements
 * the handshaking described in the data sheet (sec 3.3 in DS rev 2.0, esp fig 5 flowchart)
 * This transfers the programmed M0-M9, LB, UB and SP regs into the core(s)
 * to start their computations.
 * It is assumed that the core is not busy and is ready to start the job. The process is:
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
 *  Assumes sSC1XferBuf structure has the target device in board, chip and core fields
 */
static int iSC1CmdDataValid(void)
{
    int iTimeOut;
    int iResult = ERR_NONE;

#if (1 == CSS_DCR1_REVA_WORKAROUND)
    // Rev A silicon cannot read back so we just write the data-valid signal and hope for the best
    if (ERR_NONE == iResult) {
        sSC1XferBuf.eMode = E_SC1_MODE_REG_WRITE;
        sSC1XferBuf.eRegister = E_SC1_REG_ECR;
        sSC1XferBuf.uiData = E_SC1_ECR_VALID_DATA; // assert DATA_VALID
        iResult = iSC1SpiTransfer(&sSC1XferBuf);
    } // if (ERR_NONE == iResult)
    // Deassert DATA_VALID even if we had an error above
    delay_us(10); // I have no idea how much to wait; CSS datasheet doesn't say
    sSC1XferBuf.uiData = 0x0; // deassert DATA_VALID
    (void)iSC1SpiTransfer(&sSC1XferBuf);

#else
    //     // Check for busy status...expected to be not busy before we submit the data valid
    //     This extra check shouldn't be needed so it is commented out.
    //     sSC1XferBuf.eMode     = E_SC1_MODE_REG_READ;
    //     sSC1XferBuf.eRegister = E_SC1_REG_ESR;
    //     sSC1XferBuf.uiData    = 0;
    //     iResult = iSC1SpiTransfer(&sSC1XferBuf);
    //     if ( (ERR_NONE == iResult) && (0 != (sSC1XferBuf.uiData & E_SC1_ESR_BUSY)) ) {
    //         iResult = ERR_BUSY;     // engine is busy do we cannot give it a job
    //     }

    if (ERR_NONE == iResult) {
        sSC1XferBuf.eMode = E_SC1_MODE_REG_WRITE;
        sSC1XferBuf.eRegister = E_SC1_REG_ECR;
        sSC1XferBuf.uiData = E_SC1_ECR_VALID_DATA; // assert DATA_VALID
        iResult = iSC1SpiTransfer(&sSC1XferBuf);
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Check for busy or done status...expect one or other after giving the valid data signal
        sSC1XferBuf.eMode = E_SC1_MODE_REG_READ;
        sSC1XferBuf.eRegister = E_SC1_REG_ESR;
        iTimeOut = 1000; // in millsecs assuming 1ms delay below
        do {
            sSC1XferBuf.uiData = 0;
            iResult = iSC1SpiTransfer(&sSC1XferBuf);
            if ((ERR_NONE != iResult) || (0 != (sSC1XferBuf.uiData & (E_SC1_ESR_DONE | E_SC1_ESR_BUSY)))) {
                break;
            } // if ( (ERR_NONE != iResult)...
            delay_ms(1);
        } while (0 < --iTimeOut);
        if (0 >= iTimeOut) {
            iResult = ERR_TIMEOUT;
        }
    } // if (ERR_NONE == iResult)

    // Deassert DATA_VALID even if we had an error above
    sSC1XferBuf.eMode = E_SC1_MODE_REG_WRITE;
    sSC1XferBuf.eRegister = E_SC1_REG_ECR;
    sSC1XferBuf.uiData = 0x0; // deassert DATA_VALID
    (void)iSC1SpiTransfer(&sSC1XferBuf);

#endif // #if (1 == CSS_SC1_REVA_WORKAROUND)

    return (iResult);

} // iSC1CmdDataValid()

/** *************************************************************
 * \brief Pulse Read-Complete internal signal to complete a job to the core(s).
 * This implements the handshaking described in the data sheet (sec 3.3 in DS rev 2.0, esp fig 5 flowchart).
 * This is used after the DONE signal and after reading the nonce data from the fifo.
 *
 * \param none
 * \return none
 *  Assumes sSC1XferBuf structure has the target device in board, chip and core fields
 */
static void SC1CmdReadComplete(void)
{
    sSC1XferBuf.eMode = E_SC1_MODE_REG_WRITE;
    sSC1XferBuf.eRegister = E_SC1_REG_ECR;
    sSC1XferBuf.uiData = E_SC1_ECR_READ_COMPLETE; // assert READ_COMPLETE
    (void)iSC1SpiTransfer(&sSC1XferBuf);
    delay_us(50); // I have no idea how much to wait; CSS datasheet doesn't say
    sSC1XferBuf.uiData = 0x0; // deassert READ_COMPLETE
    (void)iSC1SpiTransfer(&sSC1XferBuf);

} // SC1CmdReadComplete()

/** *************************************************************
 *  \brief SC1 ASIC Device SPI Transfer Method
 *  Converts the structure pointed to by psSC1Transfer to a byte buffer and then calls the SPI transfer.
 *  Assumes the SPI signal routing has been configured to route communications to the ASICs.
 *  Does a (blocking) check in SPI status to prevent changing the output buffer of an on-going transfer.
 *  Will return error if timeout occurs. For reads, this will wait for the data. For writes, this will
 *  start the transfer but not wait. However, the structure can be preconfigured with the next transfer since
 *  it has already been used to construct the SPI buffer.
 *  If caller doesn't want to block while waiting on the SPI bus then it should call iIsHBSpiBusy(false)
 *  prior to calling this to ensure SPI bus is available and ready to go. If multiple tasks use this
 *  resource then arbitration and/or mutex methods should be used.
 *
 *  \param psSC1Transfer is ptr to struct with address and 64-bit data value to write/read into
 *  \return int error codes from err_codes.h
 *  \retval ERR_NONE Completed successfully.
 *  \retval ERR_BUSY SPI comms busy or timeout; try again later
 *  \retval ERR_INVALID_DATA invalid parameter?
 */
int iSC1SpiTransfer(S_SC1_TRANSFER_T* psSC1Transfer)
{
    uint8_t ixI;
    int iRetVal = ERR_NONE;

    // Do some basic checks on the input; not testing everything
    if (NULL == psSC1Transfer) {
        iRetVal = ERR_INVALID_DATA;
    } else {
        switch (psSC1Transfer->eMode) {
        case E_SC1_MODE_REG_WRITE:
        case E_SC1_MODE_REG_READ:
        case E_SC1_MODE_CHIP_WRITE:
        case E_SC1_MODE_MULTICAST:
            break;
        default:
            iRetVal = ERR_INVALID_DATA;
        } // end switch
    }

    // if (ERR_NONE == iRetVal) {
    //     // Verify the SPI port is idle and ready to go; if not we wait to allow an active transfer to complete.
    //     // We don't want to corrupt the transfer buffer or change the SPI mux during an active transfer.
    //     if (0 != iIsHBSpiBusy(true)) {
    //         iRetVal = ERR_BUSY; // got a timeout?
    //     }
    // }

    if (ERR_NONE == iRetVal) {
        // Set up the mode-address in bytes [2:0]; big-endian order
        ucaSC1OutBuf[0] = (uint8_t)((psSC1Transfer->eMode << 6) & 0xC0); // 2-bit mode
        ucaSC1OutBuf[0] |= (uint8_t)((psSC1Transfer->uiChip >> 1) & 0x3F); // 6-msb of 7-bit chip addr
        ucaSC1OutBuf[1] = (uint8_t)((psSC1Transfer->uiChip << 7) & 0x80); // lsb of 7-bit chip addr
        ucaSC1OutBuf[1] |= (uint8_t)((psSC1Transfer->uiCore >> 1) & 0x7F); // 7-msb of 8-bit core addr
        ucaSC1OutBuf[2] = (uint8_t)(psSC1Transfer->eRegister & SC1_ADR_REG_Bits); // 7-bit reg offset
        ucaSC1OutBuf[2] |= (uint8_t)((psSC1Transfer->uiCore << 7) & 0x80); // lsb of 8-bit core addr
        // Data is 64 bits written msb first; so we need to switch endianism
        if (0 != psSC1Transfer->uiData) {
            Uint64ToArray(psSC1Transfer->uiData, &ucaSC1OutBuf[SC1_TRANSFER_CONTROL_BYTES], true); // convert to array with endian swap to network order
        } else { // slight optimization for 0 on write or for reads
            for (ixI = 0; ixI < SC1_TRANSFER_DATA_BYTES; ixI++) { // data is zero so just clear it out
                ucaSC1OutBuf[SC1_TRANSFER_CONTROL_BYTES + ixI] = (uint8_t)0;
            }
        } // if (0 != psSC1Transfer->uiData)

        // Set board SPI mux and SS for the hashBoard we are going to transfer with.
        HBSetSpiMux(E_SPI_ASIC);
        HBSetSpiSelects(psSC1Transfer->uiBoard);

        // On SAMG55 the callback will deassert the slave select signal(s) at end of transfer.
        // On SAMA5D27 SOM; it must be done in the code because callback is not used.
        if (E_SC1_MODE_REG_READ == psSC1Transfer->eMode) {
            // read transfer; this will wait for the data so it returns with the result
            (void)bSPI5StartDataXfer(E_SPI_XFER8, ucaSC1OutBuf, ucaSC1InBuf, SC1_TRANSFER_BYTE_COUNT);
            psSC1Transfer->uiData = uiArrayToUint64(&ucaSC1InBuf[3], true);
        } else { // write transfer; start the transfer but don't wait
            (void)bSPI5StartDataXfer(E_SPI_XFER_WRITE, ucaSC1OutBuf, ucaSC1InBuf, SC1_TRANSFER_BYTE_COUNT);
            // for HW_ATSAMG55_XP_BOARD, if we want to monitor then we poll the callback flag here
        } // if (E_SC1_MODE_REG_READ == psSC1Transfer->eMode)

    } // if (ERR_NONE == iRetVal)

    return (iRetVal);
} // iSC1SpiTransfer

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
int iSC1TestJobs(void)
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

    uint64_t uiSolution;
    uint16_t uiTestData;
    uint16_t uiTimeOut;
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
        SC1GetTestJob(uiSampleJob); // Set up the job parameters in the structure
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_INFO "Job Sample %d:\r\n", uiSampleJob + 1);
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
        sSC1XferBuf.uiBoard = uiBoardUUT;
        sSC1XferBuf.uiChip = uiAsicUUT;
        uiCoreUUT = LAST_SC1_CORE; // holding at last core for now
        sSC1XferBuf.uiCore = uiCoreUUT;
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
        if (MAX_NUMBER_OF_SC1_CORES <= ++uiCoreUUT) {
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
        if (MAX_SC1_CHIPS_PER_STRING <= ++uiAsicUUT) {
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

    case LOAD_JOB: // pre-load into the target
        iResult = iSC1CmdPreLoadJob(&sSC1JobBuf); // preload the job parameters to the ASIC(s)
        if (ERR_NONE != iResult) {
            iReturn = iResult;
        } else {
            iState = START_JOB;
        }
        break;

    case START_JOB: // Configure the target
        iResult = iSC1CmdStartJob(); // submit the job
        if (ERR_NONE != iResult) {
            iReturn = iResult;
        } else {
            iState = WAIT_FOR_DONE;
        }
        break;

    case WAIT_FOR_DONE: // waiting for nonce/done
        uiTimeOut = 2000; // in millisecs (assuming 1 ms delay below)
        do {
            iResult = iReadBoardDoneInts(sSC1XferBuf.uiBoard, &uiTestData);
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
                iResult = ERR_TIMEOUT; // timeout on wait
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
        iResult = iSC1TestJobVerify(&uiSolution);
        if (ERR_NONE != iResult) {
            iReturn = iResult;
        } else {
            if (sSC1JobBuf.uiSolution != uiSolution) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d Chip %d unexpected solution\r\n", sSC1XferBuf.uiBoard + 1, sSC1XferBuf.uiChip + 1);
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
} // iSC1TestJobs()

/** *************************************************************
 * \brief Test function to submit example job.  This defines what to load.
 * \param uiSampleNum is number of the job to load
 * \return none
 */
static void SC1GetTestJob(uint8_t uiSampleNum)
{

    switch (uiSampleNum) {
    default:
    case 0:
        sSC1JobBuf.uiLBReg = SAMPLE1_LB;
        sSC1JobBuf.uiUBReg = SAMPLE1_UB;
        sSC1JobBuf.uiaMReg[0] = SAMPLE1_M0;
        sSC1JobBuf.uiaMReg[1] = SAMPLE1_M1;
        sSC1JobBuf.uiaMReg[2] = SAMPLE1_M2;
        sSC1JobBuf.uiaMReg[3] = SAMPLE1_M3;
        sSC1JobBuf.uiaMReg[4] = 0; // M4 not used
        sSC1JobBuf.uiaMReg[5] = SAMPLE1_M5;
        sSC1JobBuf.uiaMReg[6] = SAMPLE1_M6;
        sSC1JobBuf.uiaMReg[7] = SAMPLE1_M7;
        sSC1JobBuf.uiaMReg[8] = SAMPLE1_M8;
        sSC1JobBuf.uiaMReg[9] = SAMPLE1_M9;
        sSC1JobBuf.uiSolution = SAMPLE1_FDR0;
        break;
    case 1:
        sSC1JobBuf.uiLBReg = SAMPLE2_LB;
        sSC1JobBuf.uiUBReg = SAMPLE2_UB;
        sSC1JobBuf.uiaMReg[0] = SAMPLE2_M0;
        sSC1JobBuf.uiaMReg[1] = SAMPLE2_M1;
        sSC1JobBuf.uiaMReg[2] = SAMPLE2_M2;
        sSC1JobBuf.uiaMReg[3] = SAMPLE2_M3;
        sSC1JobBuf.uiaMReg[4] = 0; // M4 not used
        sSC1JobBuf.uiaMReg[5] = SAMPLE2_M5;
        sSC1JobBuf.uiaMReg[6] = SAMPLE2_M6;
        sSC1JobBuf.uiaMReg[7] = SAMPLE2_M7;
        sSC1JobBuf.uiaMReg[8] = SAMPLE2_M8;
        sSC1JobBuf.uiaMReg[9] = SAMPLE2_M9;
        sSC1JobBuf.uiSolution = SAMPLE2_FDR0;
        break;
    case 2:
        sSC1JobBuf.uiLBReg = SAMPLE3_LB;
        sSC1JobBuf.uiUBReg = SAMPLE3_UB;
        sSC1JobBuf.uiaMReg[0] = SAMPLE3_M0;
        sSC1JobBuf.uiaMReg[1] = SAMPLE3_M1;
        sSC1JobBuf.uiaMReg[2] = SAMPLE3_M2;
        sSC1JobBuf.uiaMReg[3] = SAMPLE3_M3;
        sSC1JobBuf.uiaMReg[4] = 0; // M4 not used
        sSC1JobBuf.uiaMReg[5] = SAMPLE3_M5;
        sSC1JobBuf.uiaMReg[6] = SAMPLE3_M6;
        sSC1JobBuf.uiaMReg[7] = SAMPLE3_M7;
        sSC1JobBuf.uiaMReg[8] = SAMPLE3_M8;
        sSC1JobBuf.uiaMReg[9] = SAMPLE3_M9;
        sSC1JobBuf.uiSolution = SAMPLE3_FDR0;
        break;

    } // switch
} // SC1GetTestJob()

#ifdef ENABLE_EMC_TEST_FUNCTIONS
// ** **********************************
// Test Functions
// ** **********************************

/** *************************************************************
 *  \brief Start job to ASIC for EMC testing;
 *  This will reset the resources and handshake the Data_Valid.
 *  \param none
 *  \return int status of operation (see err_codes.h)
 *  Assumes sSC1XferBuf structure has the target board
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 */
static int iSC1EMCStartJob(void)
{
    int iResult;
    uint16_t uiTestData, uiTestVerify;

    // Reset the core(s) FIFO memory so it can get new solutions and clear any prior NONCE(s)
    sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST;
    sSC1XferBuf.uiChip = 0; // all chips
    sSC1XferBuf.uiCore = 0; // all cores
    sSC1XferBuf.eRegister = E_SC1_REG_ECR;
    sSC1XferBuf.uiData = E_SC1_ECR_RESET_SPI_FSM | E_SC1_ECR_RESET_CORE;
    iResult = iSC1SpiTransfer(&sSC1XferBuf); // Set ECR: MRST (bit 3) and reset error SPI (bit 0) high
    if (ERR_NONE == iResult) {
        sSC1XferBuf.uiData = 0;
        iResult = iSC1SpiTransfer(&sSC1XferBuf); // Set ECR: MRST (bit 3) and reset error SPI (bit 0) low
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Unmask the bits that define the nonce fifo masks for the desired target
        sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST;
        sSC1XferBuf.eRegister = E_SC1_REG_FCR;
        sSC1XferBuf.uiData = 0; // clear all mask bits to unmask
        iResult = iSC1SpiTransfer(&sSC1XferBuf);
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        // Test read of NONCE signals; ; expecting target bit to be clear
        iResult = iReadBoardNonceInts(sSC1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d Reading NONCE status (%d)\r\n", sSC1XferBuf.uiBoard + 1, iResult);
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
        iResult = iReadBoardDoneInts(sSC1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d reading DONE status (%d)\r\n", sSC1XferBuf.uiBoard + 1, iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            if (0 != (uiTestData & DONE_BITS_MASK)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d unexpected DONE assertion (0x%x)\r\n", sSC1XferBuf.uiBoard + 1, uiTestVerify);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                // just a warning; don't stop the EMC testing
                // iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) {
        iResult = iSC1EMCDataValid(); // submit job(s) by pulsing Data_Valid
    } // if (ERR_NONE == iResult)
    return (iResult);

} // iSC1EMCStartJob()

/** *************************************************************
 * \brief Pulse Data-Valid (internal signal) to submit a job to the core(s).
 *  This implements the handshaking described in the data sheet (sec 3.3 in
 *  DS rev 2.0, esp fig 5 flowchart).
 * PRESENTLY A TEST FUNCTION ONLY FOR EMC TEST.
 *
 *  \param none
 *  \return int status of operation (see err_codes.h)
 *  Assumes sSC1XferBuf structure has the target device in board, chip and core fields
 */
static int iSC1EMCDataValid(void)
{
    int iResult;

    sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST;
    sSC1XferBuf.eRegister = E_SC1_REG_ECR;
    sSC1XferBuf.uiData = E_SC1_ECR_VALID_DATA; // assert DATA_VALID
    (void)iSC1SpiTransfer(&sSC1XferBuf);

    delay_us(50); // I have no idea how much to wait; CSS datasheet doesn't say
    sSC1XferBuf.uiData = 0x0; // deassert DATA_VALID
    iResult = iSC1SpiTransfer(&sSC1XferBuf);

    return (iResult);

} // iSC1EMCDataValid()

/** *************************************************************
 * \brief Pulse Read-Complete internal signal to complete a job to the core(s).
 * This implements the handshaking described in the data sheet (sec 3.3 in DS rev 2.0, esp fig 5 flowchart).
 * This is used after the DONE signal and after reading the nonce data from the fifo.
 * Here this should clear DONEs of all of the cores
 * PRESENTLY A TEST FUNCTION ONLY FOR EMC TEST.
 * \param none
 * \return none
 *  Assumes sSC1XferBuf structure has the target device in board, chip and core fields
 */
static void SC1EMCReadComplete(void)
{
    sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST;
    sSC1XferBuf.eRegister = E_SC1_REG_ECR;
    sSC1XferBuf.uiData = E_SC1_ECR_READ_COMPLETE; // assert READ_COMPLETE
    (void)iSC1SpiTransfer(&sSC1XferBuf);

    delay_us(50); // I have no idea how much to wait; CSS datasheet doesn't say
    sSC1XferBuf.uiData = 0x0; // deassert READ_COMPLETE
    (void)iSC1SpiTransfer(&sSC1XferBuf);

} // SC1EMCReadComplete()

/** *************************************************************
 * \brief Test hash board operation function that completes the sample job.
 *  Final steps should verify the NONCE/DONE signals and read back the answer from
 *  one asic/core and validate the result.  This also does the read complete handshake
 *  that clears the done.
 *  \param ptr to uint64 to return the solution read back from the device
 *  \return int status of operation (see err_codes.h)
 *  Assumes sSC1XferBuf structure has the target board field
 *
 * PRESENTLY A TEST FUNCTION ONLY.
 */
static int iSC1EMCTestJobVerify(uint64_t* puiSolution)
{
    int iResult = ERR_NONE;
    uint8_t uiCoreSave;
    uint16_t uiTestData;
    uint16_t uiTestVerify;
    int ixI;

    if (ERR_NONE == iResult) {
        // Test read of NONCE signals; expecting target bit to be set
        iResult = iReadBoardNonceInts(sSC1XferBuf.uiBoard, &uiTestData);
        if (ERR_NONE != iResult) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d reading NONCE status (%d)\r\n", sSC1XferBuf.uiBoard + 1, iResult);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            uiTestVerify = 0x1; // chip 1 of string should have a nonce
            if (uiTestVerify != (uiTestData & uiTestVerify)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d Chip %d missing NONCE assertion\r\n", sSC1XferBuf.uiBoard + 1, sSC1XferBuf.uiChip + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE != iResult)
    } // if (ERR_NONE == iResult)

    if (ERR_NONE == iResult) { // if it detected a nonce above...
        // Read the NONCE data and mask the associated fifo flag(s) in FCR (Fifo control register) to remove NONCE
        sSC1XferBuf.eMode = E_SC1_MODE_REG_READ;
        uiCoreSave = sSC1XferBuf.uiCore;
        sSC1XferBuf.uiCore = E_SC1_CHIP_REGS;
        sSC1XferBuf.eRegister = E_SC1_REG_IVR;
        sSC1XferBuf.uiData = 0;
        iResult = iSC1SpiTransfer(&sSC1XferBuf);
        sSC1XferBuf.uiCore = LAST_SC1_CORE;
        if (ERR_NONE == iResult) {
            // Verify CORE ID is last core and interrupt vector is 0th fifo location only
            if ((uint16_t)LAST_SC1_CORE != (((uint16_t)(sSC1XferBuf.uiData) & E_SC1_IVR_CORE_ID_MASK) >> SC1_IVR_CORE_ID_POS)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "\r\n" MSG_TKN_ERROR "HB%d Chip %d unexpected Core ID (0x%x)\r\n", sSC1XferBuf.uiBoard + 1, sSC1XferBuf.uiChip + 1, (uint16_t)(sSC1XferBuf.uiData));
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = ERR_FAILURE;
            }
        } // if (ERR_NONE == iResult)

        if (ERR_NONE == iResult) { // read the data to get the found solution
            sSC1XferBuf.eMode = E_SC1_MODE_REG_READ;
            sSC1XferBuf.eRegister = E_SC1_REG_FDR0;
            sSC1XferBuf.uiData = 0;
            iResult = iSC1SpiTransfer(&sSC1XferBuf);
            if (ERR_NONE == iResult) {
                *puiSolution = sSC1XferBuf.uiData; // solution to return
            } // if (ERR_NONE == iResult)
        } // if (ERR_NONE == iResult)

    } // if (ERR_NONE == iResult)

    sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST;
    sSC1XferBuf.eRegister = E_SC1_REG_FCR;
    sSC1XferBuf.uiData = E_SC1_FCR_MASK_ALL;
    iResult = iSC1SpiTransfer(&sSC1XferBuf);

    SC1EMCReadComplete(); // Acknowledge reading the solution by pulsing Read_Complete (removes Done but not NONCE)

    return (iResult);

} // iSC1EMCTestJobVerify()
#endif // #ifdef ENABLE_EMC_TEST_FUNCTIONS
#if 0 // SC1OCRBias() TEST FUNCTION DISABLED
/** *************************************************************
 * \brief Test function to change the core clock frequency bias.
 * PRESENTLY A TEST FUNCTION ONLY.
 * \param iValue from defined values (SC1_OCR_CLK_DIV_1,SC1_OCR_CLK_DIV_2,SC1_OCR_CLK_DIV_4,SC1_OCR_CLK_DIV_8)
 *               Can be -1 to cycle through the values
 * \return none
 */
void SC1OCRBias(void)
{
    static uint8_t uiVcoBiasCtr = 0;    // ctr for adjusting vco bias
    uint64_t uiVcoBiasBits = 0;

    uiVcoBiasCtr++;
    if (10 < uiVcoBiasCtr) {   // maximum
        uiVcoBiasCtr = 0;
    }
    switch (uiVcoBiasCtr) {
        case 0: // slowest has 5 bits bits set
            uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_5 << SC1_OCR_VCO_BIAS_SLOW_POS;
            break;
        case 1: // 4 bits slow set
            uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_4 << SC1_OCR_VCO_BIAS_SLOW_POS;
            break;
        case 2: // 3 bits slow set
            uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_3 << SC1_OCR_VCO_BIAS_SLOW_POS;
            break;
        case 3: // 2 bits slow set
            uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_2 << SC1_OCR_VCO_BIAS_SLOW_POS;
            break;
        case 4: // 1 bits slow set
            uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_1 << SC1_OCR_VCO_BIAS_SLOW_POS;
            break;
        default:
        case 5: // 0 bits bits set; unbiased
            uiVcoBiasBits = 0;
            break;
        case 6: // 1 bit fast set
            uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_1 << SC1_OCR_VCO_BIAS_FAST_POS;
            break;
        case 7: // 2 bits fast set
            uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_2 << SC1_OCR_VCO_BIAS_FAST_POS;
            break;
        case 8: // 3 bits fast set
            uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_3 << SC1_OCR_VCO_BIAS_FAST_POS;
            break;
        case 9: // 4 bits fast set
            uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_4 << SC1_OCR_VCO_BIAS_FAST_POS;
            break;
        case 10: // 5 bits fast set
            uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_5 << SC1_OCR_VCO_BIAS_FAST_POS;
            break;
    }   // switch
    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "-I- vco bias %d\r\n",uiVcoBiasCtr);
    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

    // Program the oscillator control register; test clock enable bit
    sSC1XferBuf.eMode     = E_SC1_MODE_REG_WRITE;
    sSC1XferBuf.uiCore    = SC1_OCR_OSC_GROUP4;    // upper group only
    sSC1XferBuf.eRegister = E_SC1_REG_OCR;
    sSC1XferBuf.uiData    = (OCR37_CORE63_CLKENB | SC1_OCR_ENB_TEST_CLK_OUT | uiVcoBiasBits);
//     sSC1XferBuf.uiChip = 0;      // ASIC 1
    sSC1XferBuf.uiChip = 1;      // ASIC 2

    (void)iSC1SpiTransfer(&sSC1XferBuf);

}  // SC1OCRBias()
#endif // #if 0  // SC1OCRBias() TEST FUNCTION DISABLED

int iSetSC1OCRDividerAndBias(uint8_t uiBoard, uint8_t uiDivider, int8_t iVcoBias)
{
    uint64_t uiDividerBits = 0;
    switch (uiDivider) {
    case 1:
        uiDividerBits = SC1_OCR_DIV1_VAL;
        break;
    case 2:
        uiDividerBits = SC1_OCR_DIV2_VAL;
        break;
    case 4:
        uiDividerBits = SC1_OCR_DIV4_VAL;
        break;
    default:
    case 8:
        uiDividerBits = SC1_OCR_DIV8_VAL;
        break;
    }

    uint64_t uiVcoBiasBits = 0;

    switch (iVcoBias) {
    case -5: // slowest has 5 bits bits set
        uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_5 << SC1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -4: // 4 bits slow set
        uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_4 << SC1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -3: // 3 bits slow set
        uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_3 << SC1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -2: // 2 bits slow set
        uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_2 << SC1_OCR_VCO_BIAS_SLOW_POS;
        break;
    case -1: // 1 bits slow set
        uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_1 << SC1_OCR_VCO_BIAS_SLOW_POS;
        break;
    default:
    case 0: // 0 bits bits set; unbiased
        uiVcoBiasBits = 0;
        break;
    case 1: // 1 bit fast set
        uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_1 << SC1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 2: // 2 bits fast set
        uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_2 << SC1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 3: // 3 bits fast set
        uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_3 << SC1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 4: // 4 bits fast set
        uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_4 << SC1_OCR_VCO_BIAS_FAST_POS;
        break;
    case 5: // 5 bits fast set
        uiVcoBiasBits = (uint64_t)SC1_OCR_VCO_BIAS_BITS_5 << SC1_OCR_VCO_BIAS_FAST_POS;
        break;
    } // switch
    // (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "-I- vco bias %d\r\n", iVcoBias);
    // CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

    // Program the oscillator control register; test clock enable bit
    sSC1XferBuf.eMode = E_SC1_MODE_MULTICAST;
    sSC1XferBuf.uiChip = 0; // all chips
    sSC1XferBuf.uiCore = 0; // all cores
    sSC1XferBuf.eRegister = E_SC1_REG_OCR;
    sSC1XferBuf.uiBoard = uiBoard;
    sSC1XferBuf.uiData = (SC1_OCR_CORE_ENB | uiVcoBiasBits | uiDividerBits);
    (void)iSC1SpiTransfer(&sSC1XferBuf);
} // iSetSC1OCRDividerAndBias()
