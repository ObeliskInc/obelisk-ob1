/**
 * \file MCP9903_hal.c
 *
 * \brief MCP9903 firmware device driver; application specific
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
 * Low level device drivers for the MCP9903 using an SMBus (TWI) interface.  Implemented for custom
 * configuration of the SC1 hashing board rather than as a generic device driver.
 *
 * The MCP9903 is a three channel remote diode temperature sensor. Application is expected to use
 * all three channels for measurement of two on-board temperatures and the internal temperature of the
 * MCP9903.
 *
 * Summary notes:
 *   This device is capable of resistance error correction (automatic) and beta compensation.  The typical
 * remote diode will be a 2N3904 type transistor wired as a diode.
 *
 *
 * \section Module History
 * - 20180620 Rev 0.1 Started; using AS7 (build 7.0.1417)
 *
 */

/***    INCLUDE FILES       ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h> // PRIxx type printf; 64-bit type support
#include <unistd.h>

#include "Usermain.h"
#include "TWI_Support.h"
#include "Ob1Hashboard.h"
#include "MCP9903_hal.h"
#include "MiscSupport.h"
#include "Console.h"

/***    LOCAL DEFINITIONS     ***/
// ASF4/START seems to have a problem writing single bytes to the TWI port.  It fails to issue an I2C STOP
// condition so it hangs the bus.  If needed we send the same thing twice (doesn't work for a Gen Call reset though).
#define MCP9903_XFER_BUFF_SIZE 2 // max bytes size we expect to have to transfer to the device
#define MCP9903_WRITE_BUFF_SIZE MCP9903_XFER_BUFF_SIZE // actually write 2; 1 for reg and 1 for data
#define MCP9903_READ_BUFF_SIZE 1 // we only read 1 byte; register is treated separately

#define MAX_NUM_OF_MCP9903 MAX_NUMBER_OF_HASH_BOARDS // one per board in this design

#define MCP9903_REG_WRITE_COUNT MCP9903_WRITE_BUFF_SIZE
#define MCP9903_REG_READ_COUNT MCP9903_READ_BUFF_SIZE

/***    LOCAL STRUCTURES/ENUMS      ***/

/***    LOCAL FUNCTION PROTOTYPES   ***/
static int iMCP9903ReportPrint(uint8_t uiBoard);
static int iMCP9903ReadReg(uint8_t uiBoard, uint8_t uiReg);
// static int iMCP9903WriteReg(uint8_t uiBoard);
// static int iUpdateTempSensors(uint8_t uiBoard);

/***    LOCAL DATA DECLARATIONS     ***/

// In out buffers must be static so they remain for any called functions that transport data
// static uint8_t ucaMCP990XOutBuf[MCP9903_XFER_BUFF_SIZE];   // buffer for sending out
static uint8_t ucaMCP990XInBuf[MCP9903_READ_BUFF_SIZE]; // buffer for reading in

static S_MCP9903_DATA_T saTempData[MAX_NUM_OF_MCP9903]; // Most recent conversion data
static bool bMCP9903Valid[MAX_NUM_OF_MCP9903]; // is it valid

// ADS1015 does not have an automatic channel sequencer so we make one as a list.  We can fill the list with whatever
// order of channels we want.  The same list sequence is used for all boards but each board has its own index.
static uint8_t uiaChanSeq[NUM_MCP9903_CHANS]; // channel sequencer list; gets init at start; common to all boards
//static uint8_t uiaChanSeqIndex[MAX_NUM_OF_MCP9903];   // each board has its own list index

static S_TWI_TRANSFER_T sMPC9903TwiXferBuf;

/***    GLOBAL DATA DECLARATIONS   ***/
/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/

/** *************************************************************
 * \brief Accessor function to get a pointer to the most recent data from the MCP9903 for the
 *  desired hash board. The data location does not change so the ptr can be gotten once and then
 *  the data can be accessed as needed.  User should call the iUpdateTempSensors() function first
 *  (or periodically) to force the update of the data.
 * \param uiBoard defines which board to target; 0 to MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \param ppTempDataStruct is ptr to ptr to S_MCP9903_DATA_T where the most recent MCP9903 data is stored.
 * \return int iError
 * \retval ERR_NONE if success
 * \retval ERR_INVALID_DATA if hash board is not detected or other problem
 */
int iGetTemperaturesDataPtr(uint8_t uiBoard, S_MCP9903_DATA_T** ppTempDataStruct)
{
    int iResult = ERR_NONE;

    if (ERR_NONE != iIsBoardValid(uiBoard, true)) {
        iResult = ERR_INVALID_DATA;
    } // if (ERR_NONE != iIsBoardValid(uiBoard,false))
    *ppTempDataStruct = &saTempData[uiBoard];

    return (iResult);
} // iGetTemperaturesDataPtr()

double fracsToDouble(uint16_t fracs)
{
    return ((double)fracs) / 8.0;
}

double getIntTemp(uint8_t uiBoard)
{
    return fracsToDouble(saTempData[uiBoard].iaDataRawFrac[TEMP_INT]);
}

double getAsicTemp(uint8_t uiBoard)
{
    return fracsToDouble(saTempData[uiBoard].iaDataRawFrac[TEMP_ASIC]);
}

int16_t getAsicTempInt(uint8_t uiBoard)
{
    return saTempData[uiBoard].iaDataRawInt[TEMP_ASIC];
}

double getPSTemp(uint8_t uiBoard)
{
    return fracsToDouble(saTempData[uiBoard].iaDataRawFrac[TEMP_PS]);
}

/** *************************************************************
 * \brief Initialize the MSP9903 device, driver and related.
 *  Verifies it can read the device and then configures it to start temperature conversions.
 * The default conversion rate is 4SPS which should be good enough.
 *
 * \param uiBoard defines which board to target; 0 to MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \return int error codes from err_codes.h
 */
int iMCP9903Initialize(uint8_t uiBoard)
{
    static bool bDoneOnce = false;
    int iResult;
    int ixI;

    if (false == bDoneOnce) {
        for (ixI = 0; ixI < MAX_NUM_OF_MCP9903; ixI++) {
            bMCP9903Valid[ixI] = false; // default as invalid until we initialize
            saTempData[ixI].iaDataRawInt[TEMP_INT] = 0;
            saTempData[ixI].iaDataRawInt[TEMP_ASIC] = 0;
            saTempData[ixI].iaDataRawInt[TEMP_PS] = 0;
        } // for
        bDoneOnce = true;
    }

    // verify the device is there and we can read and verify the mfg and device codes.
    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_INFO "HB%d temperature sensor init: ", uiBoard + 1);
    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
    iResult = iMCP9903ReadReg(uiBoard, MCP990X_REG_MFGID);
    if (ERR_NONE != iResult) {
        // error on transfer;
        // #TODO handle this event
        printf("-E- iMCP9903ReadReg(%d, MCP990X_REG_MFGID) Result: %d\n", uiBoard, iResult);
    } else {
        if (MCP990X_MFGID != ucaMCP990XInBuf[0]) { // verify the returned value
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "FAIL (invalid mfg ID 0x%x)\r\n", ucaMCP990XInBuf[0]);
            CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        } else {
            iResult = iMCP9903ReadReg(uiBoard, MCP990X_REG_DEVID);
            if (ERR_NONE == iResult) {
                if (MCP9903_DEVID != ucaMCP990XInBuf[0]) {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "FAIL (invalid dev ID 0x%x)\r\n", ucaMCP990XInBuf[0]);
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                } else {
                    (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "pass\r\n");
                    CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
                } // if (MCP9903_DEVID != ucaMCP990XInBuf[0])
            } // if (ERR_NONE == iResult)
        } // if (MCP990X_MFGID != ucaMCP990XInBuf[0])
    } // if (ERR_NONE != iResult)

    if (ERR_NONE == iResult) {
        bMCP9903Valid[uiBoard] = true;
        // delay_ms(100);   // delay needed?
        iResult = iMCP9903ReadReg(uiBoard, MCP990X_REG_STATUS); // read the status reg; check for faults and such
        if (ERR_NONE == iResult) {
            if (MCP990X_STA_FAULT == (ucaMCP990XInBuf[0] & MCP990X_STA_FAULT)) {
                // status returns fault condition on one of the external sensors; force the fault value
                // #TODO read the fault status register and further decode which is at fault
                saTempData[ixI].iaDataRawInt[TEMP_ASIC] = TEMP_SENSOR_FAULT_VALUE;
                saTempData[ixI].iaDataRawInt[TEMP_PS] = TEMP_SENSOR_FAULT_VALUE;
                (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, MSG_TKN_ERROR "HB%d temperature sensor fault\r\n", uiBoard + 1);
                CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
            } // if (MCP990X_STA_FAULT == (ucaMCP990XInBuf[0] & MCP990X_STA_FAULT))
        } // if (ERR_NONE == iResult)
    } // if (ERR_NONE == iResult)

    return (iResult);
} // iMCP9903Initialize()

/** *************************************************************
 *  \brief Support function to report the present system temperatures to the console
 *  Serves as an example of how to read the values and print them
 *
 *  \param uint8_t uiBoard sets which board to configure
 *  \return int error codes from err_codes.h
 */
int iMCP9903ReportTemps(uint8_t uiBoard)
{
    int iResult = ERR_NONE;

    iResult = iUpdateTempSensors(uiBoard);

    if (ERR_NONE == iResult) {
        iMCP9903ReportPrint(uiBoard);
    } //iMCP9903ReportTemps()

    return (iResult);
} // iMCP9903ReportTemps()

/** *************************************************************
 *  \brief Support function to report the present system temperatures to the console
 *
 *  \param uint8_t uiBoard sets which board to configure
 *  \return int error codes from err_codes.h
 */
static int iMCP9903ReportPrint(uint8_t uiBoard)
{
    int iResult = ERR_NONE;

    if (ERR_NONE != iResult) {
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "-T- HB%d (deg C) TI= --.--- T1= --.--- TP= --.--- (UPDATE ERROR %d)\r\n", uiBoard + 1, iResult);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
    } else {
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "-T- HB%d (deg C) TI= %d.%03d ",
            uiBoard + 1, saTempData[uiBoard].iaDataRawInt[TEMP_INT], saTempData[uiBoard].iaDataRawFrac[TEMP_INT]);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        if (TEMP_SENSOR_FAULT_VALUE <= saTempData[uiBoard].iaDataRawInt[TEMP_PS]) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "T1= FAULT  ");
        } else {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "T1= %d.%03d ",
                saTempData[uiBoard].iaDataRawInt[TEMP_ASIC], saTempData[uiBoard].iaDataRawFrac[TEMP_ASIC]);
        }
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        if (TEMP_SENSOR_FAULT_VALUE <= saTempData[uiBoard].iaDataRawInt[TEMP_ASIC]) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "TP= FAULT  ");
        } else {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "TP= %d.%03d\r\n",
                saTempData[uiBoard].iaDataRawInt[TEMP_PS], saTempData[uiBoard].iaDataRawFrac[TEMP_PS]);
        }
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

    } // if (ERR_NONE != iResult)

    return (iResult);
} // iMCP9903ReportPrint()

/** *************************************************************
 *  \brief Updates the temperature sensor variables and status. This should be done periodically (i.e. at a regular
 *   interval) or  prior to getting the values and status.
 *
 *  \param uint8_t uiBoard sets which board to configure
 *  \return int error codes from err_codes.h
 */
int iUpdateTempSensors(uint8_t uiBoard)
{
    int iResult;
    uint8_t uiFaultStatus;

    if (ERR_NONE != iIsBoardValid(uiBoard, false)) {
        bMCP9903Valid[uiBoard] = false;
        saTempData[uiBoard].iaDataRawInt[TEMP_INT] = TEMP_SENSOR_FAULT_VALUE;
        saTempData[uiBoard].iaDataRawInt[TEMP_ASIC] = TEMP_SENSOR_FAULT_VALUE;
        saTempData[uiBoard].iaDataRawInt[TEMP_PS] = TEMP_SENSOR_FAULT_VALUE;
        iResult = ERR_INVALID_DATA;
    } else {
        if (false == bMCP9903Valid[uiBoard]) { // verify the device is there
            iResult = ERR_INVALID_DATA;
        } else { // read/report the temperatures
            iResult = iMCP9903ReadReg(uiBoard, MCP990X_REG_INTTHB); // read internal high byte
            if (ERR_NONE == iResult) {
                saTempData[uiBoard].iaDataRawInt[TEMP_INT] = ucaMCP990XInBuf[0];
                iResult = iMCP9903ReadReg(uiBoard, MCP990X_REG_INTTLB); // read internal low byte
                if (ERR_NONE == iResult) {
                    saTempData[uiBoard].iaDataRawFrac[TEMP_INT] = (uint16_t)(ucaMCP990XInBuf[0] >> MCP990X_FRAC_BIT_POS) * 125;
                } else {
                    saTempData[uiBoard].iaDataRawFrac[TEMP_INT] = 0;
                } // if (ERR_NONE == iResult)

            } else {
                saTempData[uiBoard].iaDataRawInt[TEMP_INT] = TEMP_SENSOR_FAULT_VALUE;
            } // if (ERR_NONE == iResult)

            iResult = iMCP9903ReadReg(uiBoard, MCP990X_REG_EXTFAULT); // check for external faults
            if (ERR_NONE == iResult) {
                uiFaultStatus = ucaMCP990XInBuf[0];

                if (MCP990X_EXTFAULT_EXT1 == (uiFaultStatus & MCP990X_EXTFAULT_EXT1)) {
                    // external diode 1 (ASIC sensor) fault detected
                    saTempData[uiBoard].iaDataRawInt[TEMP_ASIC] = TEMP_SENSOR_FAULT_VALUE + 1;
                } else { // no fault so read the temperature channel
                    iResult = iMCP9903ReadReg(uiBoard, MCP990X_REG_EX1THB); // read asic sensor high byte
                    if (ERR_NONE == iResult) {
                        saTempData[uiBoard].iaDataRawInt[TEMP_ASIC] = ucaMCP990XInBuf[0];
                        iResult = iMCP9903ReadReg(uiBoard, MCP990X_REG_EX1TLB); // read low byte for fractions
                        if (ERR_NONE == iResult) {
                            saTempData[uiBoard].iaDataRawFrac[TEMP_ASIC] = (uint16_t)(ucaMCP990XInBuf[0] >> MCP990X_FRAC_BIT_POS) * 125;
                        } else {
                            saTempData[uiBoard].iaDataRawFrac[TEMP_ASIC] = 0;
                        } // if (ERR_NONE == iResult)

                    } else {
                        saTempData[uiBoard].iaDataRawInt[TEMP_ASIC] = TEMP_SENSOR_FAULT_VALUE + 2; // error on read of temperature
                    } // if (ERR_NONE == iResult)

                } // if (MCP990X_EXTFAULT_EXT1 == (ucaMCP990XInBuf[0] & MCP990X_EXTFAULT_EXT1))

                if (MCP990X_EXTFAULT_EXT2 == (uiFaultStatus & MCP990X_EXTFAULT_EXT2)) {
                    // external diode 2 (power supply sensor) fault detected
                    saTempData[uiBoard].iaDataRawInt[TEMP_PS] = TEMP_SENSOR_FAULT_VALUE + 1;
                } else { // no fault so read the temperature channel
                    iResult = iMCP9903ReadReg(uiBoard, MCP990X_REG_EX2THB); // read power supply sensor high byte
                    if (ERR_NONE == iResult) {
                        saTempData[uiBoard].iaDataRawInt[TEMP_PS] = ucaMCP990XInBuf[0];
                        iResult = iMCP9903ReadReg(uiBoard, MCP990X_REG_EX2TLB); // read low byte for fractions
                        if (ERR_NONE == iResult) {
                            saTempData[uiBoard].iaDataRawFrac[TEMP_PS] = (uint16_t)(ucaMCP990XInBuf[0] >> MCP990X_FRAC_BIT_POS) * 125;
                        } else {
                            saTempData[uiBoard].iaDataRawFrac[TEMP_PS] = 0;
                        } // if (ERR_NONE == iResult)

                    } else {
                        saTempData[uiBoard].iaDataRawInt[TEMP_PS] = TEMP_SENSOR_FAULT_VALUE + 2; // error on read of temperature
                    } // if (ERR_NONE == iResult)
                } // if (MCP990X_EXTFAULT_EXT2 == (ucaMCP990XInBuf[0] & MCP990X_EXTFAULT_EXT2))

            } // if (ERR_NONE == iResult)

        } // if (false == bMCP9903Valid[uiBoard])

    } //  if (ERR_NONE != iIsBoardValid(uiBoard,false))

    return (iResult);
} // iUpdateTempSensors()

/** *************************************************************
 * \brief Read the device.  Sets the register pointer to the desired register and then
 * uses a repeated start to read back the data.
 *
 * \param uiBoard defines which board to target; 0 to MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \param uiReg which reg
 * \return int error codes from err_codes.h
 * Assumes the registers are the same size
 */
static int iMCP9903ReadReg(uint8_t uiBoard, uint8_t uiReg)
{
    int iResult;

    sMPC9903TwiXferBuf.ePort = uiBoard;
    sMPC9903TwiXferBuf.eXferMode = E_TWI_READ_REG;
    sMPC9903TwiXferBuf.uiAddr = MCP990X_TWI_SA7B;
    sMPC9903TwiXferBuf.uiReg = uiReg;
    sMPC9903TwiXferBuf.uiCount = MCP9903_READ_BUFF_SIZE;
    sMPC9903TwiXferBuf.puiaData = ucaMCP990XInBuf;

    iResult = iTWIInitXFer(&sMPC9903TwiXferBuf);
    usleep(1000);  // TODO: Reduce this sleep

    return (iResult);

} // iMCP9903ReadReg()

/** *************************************************************
 * \brief Write a device register.  Does a simple write of two bytes with the
 *  first byte being the register and second byte the data to write.
 *
 * \param uiBoard defines which board to target; 0 to MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \return int error codes from err_codes.h
 * Assumes the registers are the same size.
 */

// static int iMCP9903WriteReg(uint8_t uiBoard)
// {
//
//     sMPC9903TwiXferBuf.ePort         = uiBoard;
//     sMPC9903TwiXferBuf.eXferMode     = E_TWI_WRITE;
//     sMPC9903TwiXferBuf.uiAddr        = MCP990X_TWI_SA7B;
//     sMPC9903TwiXferBuf.uiCount       = MCP9903_WRITE_BUFF_SIZE;
//     sMPC9903TwiXferBuf.puiaData      = ucaTWIOutBuf;
//
//     return (iTWIInitXFer(&sMPC9903TwiXferBuf));
//
// } // iMCP9903WriteReg()
