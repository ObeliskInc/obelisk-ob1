/**
 * \file SC1_HashBoard.c
 *
 * \brief SC1 Hash Board firmware device driver
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
 * Implements the mid level device drivers for the CSS Hash Board.  Provides support to initialize
 * and test the board using the lower level drivers.
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

// Atmel AS7 and START support for SAMG55
// #include "atmel_start.h"
// #include "atmel_start_pins.h"

#include "Ob1Hashboard.h"
#include "Usermain.h"
#include "SPI_Support.h"
#include "MCP23S17_hal.h"
#include "TCA9546A_hal.h"
#include "MCP24AA02_hal.h"
#include "MCP4017_hal.h"
#include "MCP9903_hal.h"
#include "ads1015.h"
#include "Console.h"
#include "gpio_bsp.h"
#include "CSS_SC1_hal.h"
#include "MiscSupport.h"
#include "CSS_DCR1_hal.h"

/***    LOCAL DEFINITIONS     ***/
#define SC1_HASH_SETUP_MSG (MSG_TKN_INFO "Hash Board Initialization\r\n")

#define NUM_TEMPS_CH 3
#define NUM_VOLT_CH 4

/***    LOCAL STRUCTURES/ENUMS      ***/

// Struct for hash board information; should have one per slot
typedef struct {
    bool bPresent; // true if detected
    uint8_t uiRev; // revision
    E_ASIC_TYPE_T eAsicType; // SC1 or DCR1
    int iStatus; // errors, status and related (uses err_codes.h)
    uint64_t ullUIDValue; // ID code from EEPROM IC
    uint16_t uiStringVoltmV; // target string voltage in mV
    uint16_t uiaTemps10thC[NUM_TEMPS_CH]; // associated temperatures; 0.1 deg C
    uint16_t uiaVoltsmV[NUM_VOLT_CH]; // associated voltages in mV
} S_HASH_BOARD_INFO_T;

/***    LOCAL FUNCTION PROTOTYPES   ***/
static int iHashBoardDetect(uint8_t uiBoard);
static int iHashBoardConfigure(uint8_t uiBoard);
static bool bReadHashSense(uint8_t uiBoard);
static void AssertHashBoardReset(uint8_t uiBoard);

/***    LOCAL DATA DECLARATIONS     ***/

static S_HASH_BOARD_INFO_T saHashBoardInfo[MAX_NUMBER_OF_HASH_BOARDS]; // array of information for the boards
static uint8_t uiBoardSpiMask = 0;
static int iNumBoardsDetected = 0;

/***    GLOBAL DATA DECLARATIONS   ***/

/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/

/** *************************************************************
 * \brief Accessor function to return if a hashing board is valid.  To be valid
 * it must be in the acceptable range and have been detected.  A stricter test can
 * be done that enforces it is initialized and error free.
 * \param uiBoard sets which board to check; 0 to  MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \param bBeStrict is true for extra tests
 * \return int status of board (see err_codes.h)
 * \retval ERR_NONE if valid
 * \retval ERR_NOT_FOUND if board is not present
 * \retval ERR_UNSUPPORTED_DEV if board number is out of range
 * \retval other depending on status or other errors
 *
 */
int iIsBoardValid(uint8_t uiBoard, bool bBeStrict)
{
    int iRetval = ERR_UNSUPPORTED_DEV;

    if ((0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard)) {
        if (false == bBeStrict) { // check for stricter tests
            if (true == saHashBoardInfo[uiBoard].bPresent) {
                iRetval = ERR_NONE;
            } else {
                iRetval = ERR_NOT_FOUND;
            }
        } else { // stricter test requested
            if (ERR_NONE == saHashBoardInfo[uiBoard].iStatus) {
                iRetval = ERR_NONE;
            } else {
                iRetval = saHashBoardInfo[uiBoard].iStatus;
            }
        }
    } else {
        iRetval = ERR_UNSUPPORTED_DEV; // not in the acceptable range
    }
    return (iRetval);
} // iIsBoardValid()

/** *************************************************************
 * \brief Accessor function to get board status.
 * \param uiBoard which board to set; 0 to  MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \return iStatus value (see err_codes.h)
 * \retval ERR_NONE if valid
 * \retval ERR_NOT_FOUND if board is not present
 * \retval other depending on status or other errors
 *
 */
int iGetBoardStatus(uint8_t uiBoard)
{
    int iRetval = ERR_UNSUPPORTED_DEV;

    if (iIsBoardValid(uiBoard, false) == ERR_NONE) {
        iRetval = saHashBoardInfo[uiBoard].iStatus;
    } else {
        iRetval = ERR_UNSUPPORTED_DEV; // not in the acceptable range
    }
    return (iRetval);
} // iGetBoardStatus()

/** *************************************************************
 * \brief Accessor function to set board status.
 * \param uiBoard which board to set; 0 to  MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \param iStatus value to set (see err_codes.h)
 * \return int status of board (see err_codes.h)
 * \retval ERR_NONE if valid
 * \retval ERR_NOT_FOUND if board is not present
 * \retval ERR_UNSUPPORTED_DEV if board number is out of range
 * \retval other depending on status or other errors
 *
 */
int iSetBoardStatus(uint8_t uiBoard, int iStatus)
{
    int iRetval = ERR_UNSUPPORTED_DEV;

    if (iIsBoardValid(uiBoard, false) == ERR_NONE) {
        saHashBoardInfo[uiBoard].iStatus = iStatus;
    } else {
        iRetval = ERR_UNSUPPORTED_DEV; // not in the acceptable range
    }
    return (iRetval);
} // iSetBoardStatus()

/** *************************************************************
 * \brief Accessor function to get board type. If return is 0 or more then it defines the type.
 *  If return is less than 0 then an error has occured.
 * \param uiBoard which board to get; 0 to  MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \return E_ASIC_TYPE_T enum of board type (based on resistor straps) or status of board (see err_codes.h)
 * \retval E_TYPE_SC1 if resistor R503 is missing
 * \retval E_TYPE_DCR1 if resistor R503 is installed
 * \retval ERR_NOT_FOUND if board is not present
 * \retval ERR_UNSUPPORTED_DEV if board number is out of range
 * \retval other depending on status or other errors
 *
 */
E_ASIC_TYPE_T eGetBoardType(uint8_t uiBoard)
{
    int iRetval = ERR_UNSUPPORTED_DEV;

    if ((0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard)) {
        iRetval = saHashBoardInfo[uiBoard].eAsicType;
    } else {
        iRetval = ERR_UNSUPPORTED_DEV; // not in the acceptable range
    }
    return (iRetval);
} // eGetBoardType()

bool isBoardPresent(uint8_t uiBoard)
{
    int iRetval = ERR_UNSUPPORTED_DEV;

    if ((0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard)) {
        return saHashBoardInfo[uiBoard].bPresent;
    }
    return false;
}

uint8_t getBoardRevision(uint8_t uiBoard)
{
    if ((0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard)) {
        return saHashBoardInfo[uiBoard].uiRev;
    }
    return 0;
}

uint64_t getBoardUniqueId(uint8_t uiBoard)
{
    if ((0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard)) {
        return saHashBoardInfo[uiBoard].ullUIDValue;
    }
    return 0;
}

/** *************************************************************
 * \brief Function to initialize the boards
 * \param uiBoard sets which board to init; if MAX_NUMBER_OF_HASH_BOARDS or higher then init all
 * \return none; updates the internal information struct which can be queried with accessor functions
 */
int iHashBoardInit(uint8_t uiBoard)
{
    uint8_t uiFan;
    int iResult;
    CONSOLE_OUTPUT_IMMEDIATE(SC1_HASH_SETUP_MSG);

    iNumBoardsDetected = iHashBoardDetect(uiBoard); // Detect which board(s) are present

    // #TODO probably should verify fans work next; if there is a slot 1 board then front fan should work.
    // If there is a board in slot 2 then rear fan should work.
    for (uiFan = 0; uiFan < MAX_NUM_FANS; uiFan++) {
        InitFanStatus(uiFan);
        if (ERR_NONE == iIsBoardValid(uiFan, false)) {
            iResult = iTestFan(uiFan);
            if (ERR_NONE == iResult) {
                SetFanError(uiFan, ERR_NONE);
            }
        } else {
            SetFanError(uiFan, ERR_NOT_FOUND); // if board is not there we have no working fan
        }
    } // for

    // If we have 2 or more boards then we need to have 2 fans working else we should not start up
    // #TODO add code to verify number of fans and number of boards

    iResult = iHashBoardConfigure(uiBoard); // Initialize/test detected boards
    if (ERR_NONE == iResult) {
        iResult = iStringStartup(uiBoard); // start up the strings
    }

#ifdef DO_TEST_JOBS_ON_INIT
    // Submit test jobs to the hashing boards to confirm they are working before we begin real jobs
    if (ERR_NONE == iResult) {
        if (E_TYPE_SC1 == eGetBoardType(0)) { // only checking first board here and assuming all are the same
            do { // run the machine until done (iResult =1) or error
                iResult = iSC1TestJobs();
            } while (ERR_NONE == iResult);
        } else if (E_TYPE_DCR1 == eGetBoardType(0)) {
            do { // run the machine until done (iResult =1) or error
                iResult = iDCR1TestJobs();
            } while (ERR_NONE == iResult);
        }
        if (ERR_NONE <= iResult) {
            iResult = ERR_NONE;
        }
    } // if (ERR_NONE == iResult)
#endif
    return (iResult);

} // iHashBoardInit()

/** *************************************************************
 * \brief Function to reset hash boards. This should assert the reset signal and also reprogram some
 * of the other devices into a known state.
 * \param int iBoard sets which board to reset; if MAX_NUMBER_OF_HASH_BOARDS or higher then do all
 * \return none
 */
void HashBoardReset(uint8_t uiBoard)
{
    int ixI;
    int iStart, iEnd;

    if (iIsBoardValid(uiBoard, false) == ERR_NONE) { // do one or all?
        iStart = uiBoard; // individual board
        iEnd = uiBoard;
    } else {
        iStart = 0; // all boards
        iEnd = LAST_HASH_BOARD;
    } // if ( (0 <= iBoard) && (MAX_NUMBER_OF_HASH_BOARDS > iBoard) )

    AssertHashBoardReset(uiBoard); // reset any/all boards; should stop ASICs clocking and cut string power
    // reprogram any/all of the requested boards
    for (ixI = iStart; ixI <= iEnd; ixI++) {
        // Set power supply pot to lowest voltage (highest count)
        (void)iSetPSControl(uiBoard, MAX_MCP4017_VALUE);
    } // for

} // HashBoardReset()

/** *************************************************************
 * \brief Read the unique ID associated with the hash board using the
 * required lower level functions.  Should put it into the information structure.
 * This is a bridging function to the TCA9546A and MCP24AA02 drivers.
 * \param uint8_t uiBoard sets which board to target;  range 0 to MAX_NUMBER_OF_HASH_BOARDS-1
 * \return int status of board (see err_codes.h)
 */
int iReadHashBoardUID(uint8_t uiBoard)
{
#define UID_MSW_VALUE ((E_MFG_ID_VALUE << 8) | E_DEV_ID_VALUE) // top two bytes of UID should be this (mfg with device ID)
    int iRetval;

    if (iIsBoardValid(uiBoard, false) == ERR_NONE) {
        iRetval = iSetTwiSwitch(uiBoard); // direct Twi comms to the proper port

        if (ERR_NONE == iRetval) {
            iRetval = iGetUID(uiBoard, &saHashBoardInfo[uiBoard].ullUIDValue); // read the UID
            if (ERR_NONE == iRetval) { // The expected value of the top 16 bits of UID is UID_MSW_VALUE
                if (UID_MSW_VALUE == (uint16_t)(saHashBoardInfo[uiBoard].ullUIDValue >> 48)) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "UID 0x%016" PRIX64 "\r\n", saHashBoardInfo[uiBoard].ullUIDValue);
                } else {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "UID 0x%016" PRIX64 " (invalid value)\r\n", saHashBoardInfo[uiBoard].ullUIDValue);
                    iRetval = ERR_BAD_DATA;
                }
            } else if (ERR_TIMEOUT == iRetval) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "UID timeout on read\r\n");
            } else {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "UID error %d\r\n", iRetval);
            } // if (ERR_NONE == iRetval) {
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        }
    } else {
        iRetval = ERR_UNSUPPORTED_DEV; // not in the acceptable range
    } // if ((0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard))
    return (iRetval);

} // iReadHashBoardUID

/** *************************************************************
 * \brief Function to detect presence of hash boards in the slots.
 * Uses the board sense signal to sense if associated GPIO(s) are pulled low.
 * GPIOs should have pull-up resistors on the controller board. Should be called
 * once on initialization/startup and then again as needed to test/update the status.
 * First time called will initialize the information structure with default values.
 * \param uiBoard sets which board to target; MAX_NUMBER_OF_HASH_BOARDS or higher for all
 * \return int is number of boards detected
 * Side effect; updates the internal information struct which can be queried with accessor functions
 */
static int iHashBoardDetect(uint8_t uiBoard)
{
    static bool bDoneOneTime = false;
    uint8_t uiUUT;
    int iNumFound = 0;
    int ixI, ixJ;
    int iStart, iEnd;

    if (false == bDoneOneTime) { // initialize on first pass
        for (ixI = 0; ixI < MAX_NUMBER_OF_HASH_BOARDS; ixI++) {
            // init the struct for the board
            saHashBoardInfo[ixI].bPresent = false;
            saHashBoardInfo[ixI].uiRev = 0;
            saHashBoardInfo[ixI].iStatus = ERR_NOT_FOUND;
            saHashBoardInfo[ixI].ullUIDValue = 0;
            saHashBoardInfo[ixI].uiStringVoltmV = 0;
            for (ixJ = 0; ixJ < NUM_TEMPS_CH; ixJ++) {
                saHashBoardInfo[ixI].uiaTemps10thC[ixJ] = 0;
            } // for
            for (ixJ = 0; ixJ < NUM_VOLT_CH; ixJ++) {
                saHashBoardInfo[ixI].uiaVoltsmV[ixJ] = 0;
            } // for
        } // for
        bDoneOneTime = true;
    } // if (false == bDoneOneTime)

    if (iIsBoardValid(uiBoard, false) == ERR_NONE) {
        iStart = (int)uiBoard; // individual board
        iEnd = (int)uiBoard;
    } else {
        iStart = 0; // all boards
        iEnd = LAST_HASH_BOARD;
    } // if ( (0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard) )

    for (ixI = iStart; ixI <= iEnd; ixI++) {
        uiUUT = (uint8_t)ixI;
        if (true == bReadHashSense(uiUUT)) {
            iNumFound++;
            // Code below might instead be in the configure function. However, by putting it here, it allows a simpler way to
            // setup basic structure.  It also makes it easier to decommission one if a board goes missing.  Note; as
            // of writing this we are not supporting or planning to support hot-plugging of boards. So some of this is unneeded.
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "detecting slot %d: board found\r\n", uiUUT + 1);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

            if (false == saHashBoardInfo[uiUUT].bPresent) { // if board status changed; reinit its information struct

                saHashBoardInfo[uiUUT].iStatus = ERR_NOT_INITIALIZED;
                saHashBoardInfo[uiUUT].uiRev = 0;
                saHashBoardInfo[uiUUT].ullUIDValue = 0;
                saHashBoardInfo[uiUUT].uiStringVoltmV = 0;
                for (ixJ = 0; ixJ < NUM_TEMPS_CH; ixJ++) {
                    saHashBoardInfo[uiUUT].uiaTemps10thC[ixJ] = 0;
                } // for
                for (ixJ = 0; ixJ < NUM_VOLT_CH; ixJ++) {
                    saHashBoardInfo[uiUUT].uiaVoltsmV[ixJ] = 0;
                } // for
                saHashBoardInfo[uiUUT].bPresent = true;
            } // if (false == sSC1BoardInfo[uiUUT].bPresent)
        } else {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "detecting slot %d: empty\r\n", uiUUT + 1);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            if (true == saHashBoardInfo[uiUUT].bPresent) { // if board status changed; update its information struct
                saHashBoardInfo[uiUUT].bPresent = false;
                saHashBoardInfo[uiUUT].iStatus = ERR_NOT_FOUND;
                // not changing the other information on purpose
            } // if (true == sSC1BoardInfo[uiUUT].bPresent)
        } // if (true == bReadHashSense(uiUUT))
    } // for
    return (iNumFound);
} // iHashBoardDetect()

/** *************************************************************
 * \brief Function to assert a reset to the hash boards. Hash boards should reset based on the
 * SPISEL[1:0] lines and the SPICS.  Reset should generate a hardware reset to the GPIO expanders. The
 * result should be to turn off ASIC string power and clocking.  This will not reset the I2C devices
 * and those should be reset or reconfigured by other means (e.g. general call reset or reprogramming).
 * \param int iBoard sets which board to reset; if MAX_NUMBER_OF_HASH_BOARDS or higher then do all
 * \return none
 */
static void AssertHashBoardReset(uint8_t uiBoard)
{
    int ixI;
    int iStart, iEnd;

    if (iIsBoardValid(uiBoard, false) == ERR_NONE) {
        iStart = uiBoard; // individual board
        iEnd = uiBoard;
    } else {
        iStart = 0; // all boards
        iEnd = LAST_HASH_BOARD;
    } // if ( (0 <= iBoard) && (MAX_NUMBER_OF_HASH_BOARDS > iBoard) )

    HBSetSpiMux(E_SPI_RESET);
    // assert any/all of the requested chip selects
    for (ixI = iStart; ixI <= iEnd; ixI++) {
        if (ERR_NONE == iIsBoardValid(ixI, false)) { // board must be there
            saHashBoardInfo[ixI].iStatus = ERR_NOT_INITIALIZED; // status change from reset
            HBSetSpiSelects((uint8_t)ixI, false);
            delay_us(50); // brief delay to allow reset to happen
            //usleep(50);
            HBSetSpiSelects((uint8_t)ixI, true);
        } // if (ERR_NONE == iIsBoardValid(ixI,false))
    } // for

} // AssertHashBoardReset()

/** *************************************************************
 * \brief Function initial configuration of hash boards. Tests and sets up devices on a hash board
 *  if the hash board was detected. This fills in the information structure based on the
 *  operation. Assumes boards were detected prior to this.
 * - If completed successfully, the status in the structure should be ERR_NONE; else the status
 *   will get some other error code.
 * - Should get the board ID, revision, and initialize the on-board devices for temperature, voltage
 *   monitoring of the ASIC string.
 * - Set an intial string power and verify the asics will communicateon the SPI bus.
 *  \param int uiBoard sets which board to configure; if MAX_NUMBER_OF_HASH_BOARDS or higher then do all
 *  \return int status of operation (see err_codes.h)
 *  Side Effect: updates the internal information struct which can be queried with accessor functions
 *
 *  For each hashing board found and before turning on ASIC string power:
 *  - Reset the board; asserts a hw reset to the GPIO devices; turns off string power, etc.
 *  - Use I2C bus to read the UID code from the EEPROM; verify the expected mfg and device codes.
 *  - Use SPI to initialize the GPIO expander(s)
 *  - Use I2C bus to test its devices
 *    - Temperature sensor
 *    - Voltage monitor; test 12V input (AVIN3 is 12V, others should be close to zero until string is powered
 *    - Initialize power supplies
 *       - set up the EEPOT that controls string voltage; read back to verify it works correctly
 *       - enable PS to a minimum voltage; short delay to allow to stabilize
 *       - Read voltage(s) from voltage monitor; verify V15 at AIN2, V15IO (AIN0), and V1 (AIN1)
 *       - adjust voltage as needed to give startup voltage (V1 = 0.5?)
 *  - Use SPI to initialize and do a basic test of hashing board ASICs
 *
 */
static int iHashBoardConfigure(uint8_t uiBoard)
{
#define DONE_AT_POR 0xFFFF // done pins will be all high with asic power off
#define NONCE_AT_POR 0xFFFF // nonce pins will be all high with asic power off
    uint8_t uiUUT;
    uint16_t uiTestData;
    int ixI;
    int iStart, iEnd;
    int iResult = ERR_NONE;
    int iResult2 = ERR_NONE;
    S_ADS1015_DATA_T* psVMonData;
    clock_t transfer_time = 0;
    int tempRet;

    if (iIsBoardValid(uiBoard, false) == ERR_NONE) {
        iStart = (int)uiBoard; // individual board
        iEnd = (int)uiBoard;
    } else {
        iStart = 0; // all boards
        iEnd = LAST_HASH_BOARD;
    } // if ( (0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard) )

    for (ixI = iStart; ixI <= iEnd; ixI++) {
        uiUUT = (uint8_t)ixI;

        if (ERR_NONE == iIsBoardValid(ixI, false)) {

            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INFO "HB%d reset...configure\r\n", uiUUT + 1);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            AssertHashBoardReset(uiUUT); // Reset the board; turn off power, reset some hardware, etc.
            //delay_ms(250);      // need to delay some to allow capacitors to drain and board power to be off
            usleep(250000);
            if (ERR_NONE == iResult) {
                // Read board HW ID from EEPROM IC; does a first test of the I2C comms
                iResult = iReadHashBoardUID(uiUUT);
            } // if (ERR_NONE == iResult)

            // *** GPIO expander setup and read section
            // Even if it failed the above UID read, try to set up the GPIO expanders; they are on SPI instead of TWI
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "initialize I/O: ");
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            iResult2 = iPexInit(uiUUT); // Setup/test GPIO expanders (SPI bus devices)
            if (ERR_NONE != iResult2) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "error %d\r\n", iResult2);
            } else {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "pass\r\n");
            }
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

            if (ERR_NONE == iResult2) {
                // Read revision of board from GPIO pins; also identifies the hash board type
                iResult2 = iReadBoardRevision(uiUUT, &saHashBoardInfo[uiUUT].uiRev, &saHashBoardInfo[uiUUT].eAsicType, &transfer_time);
                if (ERR_NONE != iResult2) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "rev read error %d\r\n", iResult2);
                } else {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "rev: %d\r\n", saHashBoardInfo[uiUUT].uiRev); // report version
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    iResult2 = eGetBoardType(uiUUT);
                    if (E_TYPE_SC1 == iResult2) {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "type: SC1\r\n"); // report type found
                        iResult2 = ERR_NONE; // we used it above for board type so we have to reset to ensure code below works
                    } else if (E_TYPE_DCR1 == iResult2) {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "type: DCR1\r\n"); // report type found
                        iResult2 = ERR_NONE; // we used it above for board type so we have to reset to ensure code below works
                    } else {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "type: ERROR (%d)\r\n", iResult2); // report error
                        saHashBoardInfo[uiUUT].iStatus = ERR_UNSUPPORTED_DEV;
                        iResult2 = ERR_UNSUPPORTED_DEV;
                    }
                }
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            } // if (ERR_NONE == iResult2)

            if (ERR_NONE == iResult2) {
                // Test read of DONE signals.  The board reset above should have turned off ASIC power so the DONE/ATTN signals should be high.
                iResult2 = iReadBoardDoneInts(uiUUT, &uiTestData, &transfer_time);
                if (ERR_NONE != iResult2) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "ERROR reading DONE status %d\r\n", iResult2);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                } else {
                    if (DONE_AT_POR != uiTestData) {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "DONE value: FAIL 0x%x (expected 0x%x)\r\n", uiTestData, DONE_AT_POR);
                    } else {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "DONE value: pass\r\n");
                    }
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                }
            } // if (ERR_NONE == iResult2)

            if (ERR_NONE == iResult2) {
                // Test read of NONCE signals.  The reset above should have turned off ASIC power so the DONE/ATTN signals should be high.
                iResult2 = iReadBoardNonceInts(uiUUT, &uiTestData, &transfer_time);
                if (ERR_NONE != iResult2) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "ERROR reading NONCE status %d\r\n", iResult2);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                } else {
                    if (NONCE_AT_POR != uiTestData) {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "NONCE value: FAIL 0x%x (expected 0x%x)\r\n", uiTestData, NONCE_AT_POR);
                    } else {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INDENT4 "NONCE value: pass\r\n");
                    }
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                }
            } // if (ERR_NONE == iResult2)
            // *** GPIO expander setup and read section END

            // Configure temperature sensor; do first reads; note: doing this with asic power off.  Assumes the power
            // to be off because we reset the circuit previously.
            if (ERR_NONE == iResult) { // back on TWI/I2C bus so we use iResult again
                iResult = iMCP9903Initialize(uiUUT);
                if (ERR_NONE == iResult) {
                    iResult = iMCP9903ReportTemps(uiUUT);
                }
            }
            if (ERR_NONE != iResult2) { // Merge the iResult2 value into iResult before we go further
                iResult = iResult2;
            }

            // Configure voltage monitor IC; do first reads and report initial voltages
            iResult2 = iADS1015Initialize(uiUUT);
            if (ERR_NONE == iResult2) {
                ReportSysVoltages(uiUUT);
                iResult2 = iGetVMonDataPtr(uiUUT, &psVMonData); // get where the data is stored
                // Expecting VIN to be approx 12V nom; others should be 0V nominal;  Test VIN because it is possible that
                // this is running if one hash board has power but other don't or something is wrong with the sensor. We don't
                // want to start power up of board if the input isn't as expected.
                if (ERR_NONE == iResult2) {
                    if ((MIN_VIN_MV > psVMonData->iaDataAsmV[VMON_VIN]) || (MAX_VIN_MV < psVMonData->iaDataAsmV[VMON_VIN])) {
                        iResult2 = VMON_IN_FAULT;
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "ERROR Input voltage out of range\r\n");
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    } else if (1000 <= psVMonData->iaDataAsmV[VMON_V1]) { // Expecting other voltages to be less than 1V
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "ERROR V1 1 voltage out of range\r\n");
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        iResult2 = ERR_FAILURE;
                    } else if (1000 <= psVMonData->iaDataAsmV[VMON_V15]) {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "ERROR Vstring voltage out of range\r\n");
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        iResult2 = ERR_FAILURE;
                    } else if (1000 <= psVMonData->iaDataAsmV[VMON_V15IO]) {
                        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "ERROR V15IO voltage out of range\r\n");
                        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                        iResult2 = ERR_FAILURE;
                    }
                } // if (ERR_NONE == iResult2)
            } // if (ERR_NONE == iResult2)

            // If we have a working temperature sensor and the asic temperature is acceptable then we can start up the
            // string of asics.
            // #TODO add in code to check temperature sensors
            //iMCP9903ReportTemps

            if (ERR_NONE != iResult2) { // Merge the iResult2 value into iResult before we go further
                iResult = iResult2;
            } // if (ERR_NONE != iResult2)

            // Initialize the ASIC string power control loop for its lowest voltage. This tests that we can write and read back
            // from the voltage control potentiometer.
            if (ERR_NONE == iResult) {
                iResult = iSetPSControl(uiUUT, MAX_MCP4017_VALUE);

                if (ERR_NONE == iResult) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "String power 1 control: pass\r\n");
                } else {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "String power 1 control: FAIL\r\n");
                }
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            } // if (ERR_NONE == iResult)

            // IF the above tests passed, it can turn on asic string power at a low voltage .  That is, if we have
            // working fans, temperature sensors, ps control, voltage monitoring, etc.
            if (ERR_NONE == iResult) {
                iResult = iSetPSEnable(uiUUT, true, &transfer_time); // turn on the string
                if (ERR_NONE != iResult) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "ERROR power supply enable\r\n");
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                }
            } // if (ERR_NONE == iResult)

            if (ERR_NONE == iResult) { // if no errors from above...
                ReportSysVoltages(uiUUT);
                //  (void)iGetVMonDataPtr(uiUUT, &psVMonData);    // get where the data is stored <-- we already did this above
                // Expecting VIN to be approx 12V nom; others should be within expected ranges.
                if ((MIN_VIN_MV > psVMonData->iaDataAsmV[VMON_VIN]) || (MAX_VIN_MV < psVMonData->iaDataAsmV[VMON_VIN])) {
                    iResult = VMON_IN_FAULT;
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "ERROR Input voltage out of range\r\n");
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                } else if ((MIN_V15_MV > psVMonData->iaDataAsmV[VMON_V15]) || (MAX_V15_MV < psVMonData->iaDataAsmV[VMON_V15])) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "ERROR Vstring voltage out of range\r\n");
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    iResult = ERR_FAILURE;
                } else if ((MIN_V15IO_MV > psVMonData->iaDataAsmV[VMON_V15IO]) || (MAX_V15IO_MV < psVMonData->iaDataAsmV[VMON_V15IO])) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "ERROR V15IO voltage out of range\r\n");
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    iResult = ERR_FAILURE;
                } else if ((START_V1_MV > psVMonData->iaDataAsmV[VMON_V1]) || (MAX_V1_MV < psVMonData->iaDataAsmV[VMON_V1])) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "ERROR V1 1 voltage out of range\r\n");
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                    iResult = ERR_FAILURE;
                }

            } // if (ERR_NONE == iResult) {

            // Recheck temperatures. We want to test that the temperatures didn't change much from before we turned on the
            // string power.  This is because the asic power supply was observed to corrupt the temperature reading unless the
            // sensor filter is working correctly.
            // #TODO add in code to check temperature sensors
            //iMCP9903ReportTemps

            (void)iSetPSEnable(uiUUT, false, &transfer_time); // At the end, turn off the string while we go on to test any others

            saHashBoardInfo[uiUUT].iStatus = iResult;

        } // if (ERR_NONE == iIsBoardValid(ixI,false)) {
    } // for
    return (iResult);
} // iHashBoardConfigure()

/** *************************************************************
 *  \brief ASIC String Startup Method
 *  Called at startup to do first stage initialization of the ASIC(s) strings.  Might possibly be called as
 *  part of restart of a string.
 *  Determines which type of asic string and calls the appropriate startup method.
 *  \param int uiBoard sets which board to configure; if MAX_NUMBER_OF_HASH_BOARDS or higher then do all
 *  \return int status of operation (see err_codes.h)
 */
int iStringStartup(uint8_t uiBoard)
{
    uint8_t ixI;
    uint8_t uiUUT;
    int iStart, iEnd;
    int iResult = ERR_NONE;

    if (iIsBoardValid(uiBoard, false) == ERR_NONE) {
        iStart = (int)uiBoard; // individual board
        iEnd = (int)uiBoard;
    } else {
        iStart = 0; // all boards
        iEnd = LAST_HASH_BOARD;
    } // if ( (0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard) )

    for (ixI = iStart; ixI <= iEnd; ixI++) { // for each hashing board
        uiUUT = (uint8_t)ixI;
        if (ERR_NONE == iIsBoardValid(uiUUT, true)) { // board must be there to startup and should have passed initial testing

            if (E_TYPE_SC1 == eGetBoardType(uiUUT)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INFO "HB%d SC1 engine power up...\r\n", uiUUT + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = iSC1StringStartup(uiUUT);
                if (ERR_NONE == iResult) {
                    // Load registers with Blake2B information for SC1 (or Decred)
                    iResult = iSC1DeviceInit(uiUUT);
                }

            } else if (E_TYPE_DCR1 == eGetBoardType(uiUUT)) {
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INFO "HB%d DCR1 engine power up...\r\n", uiUUT + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                iResult = iDCR1StringStartup(uiUUT);
                if (ERR_NONE == iResult) {
                    // Load registers with Blake256 information for DCR1
                    iResult = iDCR1DeviceInit(uiUUT);
                }
            } else {
                iResult = ERR_UNSUPPORTED_DEV;
            }
        } // if (ERR_NONE == iIsBoardValid(uiUUT,true))
        if (ERR_NONE != iResult) {
            break; // for
        }
    } // for (ixI = iStart; ixI <= iEnd; ixI++)

    return (iResult);
} // iStringStartup(void)

/** *************************************************************
 *  \brief Support function to report the present system voltages to the console
 *
 *  \param uint8_t uiBoard sets which board to configure; range 0 to MAX_NUMBER_OF_HASH_BOARDS-1
 *  \return none
 */
void ReportSysVoltages(uint8_t uiBoard)
{
    uint8_t uiEpotCount;
    S_ADS1015_DATA_T* psVMonData;
    int iResult;

    if (iIsBoardValid(uiBoard, false) == ERR_NONE) {
        iResult = iADS1015ReadVoltages(uiBoard);
        if (ERR_NONE == iResult) {
            iResult = iGetVMonDataPtr(uiBoard, &psVMonData); // get where the data is stored
        }
        if (ERR_NONE == iResult) {
            (void)iReadMCP4017(&uiEpotCount);
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "-V- HB%d (mV) VIN= %d ;", uiBoard + 1, psVMonData->iaDataAsmV[VMON_VIN]);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, " VString= %d ;", psVMonData->iaDataAsmV[VMON_V15]);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, " V15IO= %d ;", psVMonData->iaDataAsmV[VMON_V15IO]);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, " V1= %d ;", psVMonData->iaDataAsmV[VMON_V1]);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, " PSCTL= %d\r\n", uiEpotCount);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "-V- HB%d Error reading voltage monitor\r\n", uiBoard + 1);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        }
    } // if ( (0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard) )
} // ReportSysVoltages

/** *************************************************************
 *  \brief Support function to set the ASIC supply control potentiometer.
 *  This also verifies the device is set by reading back the value.
 *
 *  \param uint8_t uiBoard sets which board to target; range 0 to MAX_NUMBER_OF_HASH_BOARDS-1
 *  \param uint8_t uiPSControlValue is the value to set and verify
 *  \return int error codes from err_codes.h
 */
int iSetPSControl(uint8_t uiBoard, uint64_t uiPSControlValue)
{
    uint8_t uiVerifyValue;
    int iResult;

    if (iIsBoardValid(uiBoard, false) == ERR_NONE) {
        // This should set Epot to desired value and verify it can be read back
        iResult = iSetTwiSwitch(uiBoard); // direct Twi comms to the proper port
        if (ERR_NONE == iResult) {
            iResult = iWriteMCP4017(uiPSControlValue);
        }
        if (ERR_NONE == iResult) { // read back to verify
            iResult = iReadMCP4017(&uiVerifyValue);
        }
        if (ERR_NONE == iResult) { // read back to verify
            if (uiVerifyValue != uiPSControlValue) {
                iResult = ERR_FAILURE;
            } else {
                // delay to allow new voltage to stabilize
                usleep(200000);
            }
        }
    } // if ( (0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard) )
    return (iResult);
} // iSetPSControl

/** *************************************************************
 * \brief Read the sense line of the desired hash board.
 * These are intended to read states of gpios configured as inputs with
 * internal pull-ups.  If a board is plugged-in the pin should be low, else
 * it will be read as high.
 * \param uint8_t uiBoard sets which board to target; range 0 to MAX_NUMBER_OF_HASH_BOARDS-1
 * \return bool true if detected
 */
static bool bReadHashSense(uint8_t uiBoard)
{
    bool bReturn = false;
    int iRetVal;

    switch (uiBoard) {
    case 0:
        // #TODO - SOLVED add code here to read gpio >
        // SAMG55 doesn't look at the gpio and only allows 1 board
        //bReturn = true;
        iRetVal = gpio_read_pin(HASH_BOARD_ONE_PRESENT);
        break;
    case 1:
        //bReturn = false;
        iRetVal = gpio_read_pin(HASH_BOARD_TWO_PRESENT);
        break;
    case 2:
        //bReturn = false;
        iRetVal = gpio_read_pin(HASH_BOARD_THREE_PRESENT);
        break;
    default:
        // could read them all and return true if all detected
        iRetVal = -1;
        break;
    }

    if (0 == iRetVal) {
        bReturn = true;
    } else {
        bReturn = false;
    }

    return (bReturn);

} // bReadHashSense
