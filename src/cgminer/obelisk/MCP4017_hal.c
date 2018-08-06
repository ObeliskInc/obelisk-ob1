/**
 * \file MCP4017_hal.c
 *
 * \brief MCP4017 firmware device driver; application specific
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
 * Low level device drivers for the MCP4017 using TWI interface.  Implemented for custom
 * configuration of the SC1 hashing board rather than as a generic device driver.
 *
 * The MCP4017 is a digital potentiometer with 127 steps.  Application is expected to use
 * a 5Kohm nominal value (range with tolerance in data sheet is 4K to 6K ohm).
 *
 * Application specific notes:
 *   This forms the bottom leg of a voltage divider to the feedback terminal of a DC-DC converter.
 * NOTE: Increasing the resistance lowers the feedback voltage and thus increases the output voltage.
 *       Decreasing the resistance should increase the output voltage of the DC-DC converter.
 *
 * An actual transfer function of step number to output voltage depends on the input voltage which
 * is expected to be from 11.5 to 12.5V.  Due to low accuracy of the digital pot and the range of the
 * input voltage, an accurate and stable output voltage is best achieved by a control loop that
 * adjusts the digital potentiometer to maintain a desired voltage.
 *
 * \section Module History
 * - 20180606 Rev 0.1 Started; using AS7 (build 7.0.1417)
 *
 */

/***    INCLUDE FILES       ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>   // PRIxx type printf; 64-bit type support
#include <unistd.h>

#include "Usermain.h"
#include "TWI_Support.h"
#include "MCP4017_hal.h"
#include "MiscSupport.h"

/***    LOCAL DEFINITIONS     ***/
// ASF4/START seems to have a problem writing single bytes to the TWI port.  It fails to issue an I2C STOP
// condition so it hangs the bus.  Instead we send the same thing twice.
#define MCP4017_XFER_BUFF_SIZE  1  // max bytes size we expect to have to transfer to the device
#define MCP4017_READ_BUFF_SIZE  1
#define MCP4017_WRITE_BUFF_SIZE MCP4017_XFER_BUFF_SIZE

/***    LOCAL STRUCTURES/ENUMS      ***/

/***    LOCAL FUNCTION PROTOTYPES   ***/

/***    LOCAL DATA DECLARATIONS     ***/

// In out buffers must be static so they remain for any called functions that transport data
static uint8_t ucaTWIOutBuf[MCP4017_XFER_BUFF_SIZE];   // buffer for sending out
static uint8_t ucaTWIInBuf[MCP4017_XFER_BUFF_SIZE];    // buffer for reading in

/***    GLOBAL DATA DECLARATIONS   ***/
/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/

/** *************************************************************
 * \brief Writes a value to the potentiometer data by TWI bus.  Assumes the TWI bus
 *  routing is already been configured.
 * \param uiValue is value 0 to 127 to set the device
 * \return int error codes from err_codes.h
 * Value 0 sets wiper to 0 ohm
 * Value 127 sets wiper to max (5Kohm)
 *
 * Assumes the TWI bus is available and any routing switches are set.
 */
int iWriteMCP4017(uint8_t uiValue)
{
    int iResult = ERR_NONE;

    if (MAX_MCP4017_VALUE < uiValue) {  // enforce limits
        uiValue = MAX_MCP4017_VALUE;
    } else if (MIN_MCP4017_VALUE > uiValue) {
        uiValue = MIN_MCP4017_VALUE;
    }

    // Address the device
    iResult = iSetTWISlaveAdr(TWI_PORT0_NUM,MCP4017_DEFAULT_ADDR);
    if (ERR_NONE == iResult) {
        // Send the message
        ucaTWIOutBuf[0] = (uiValue & 0x7F);
        iResult = iTWIWriteBuf(TWI_PORT0_NUM, ucaTWIOutBuf, MCP4017_XFER_BUFF_SIZE, true);
    }   // if (ERR_NONE == iResult)

    return(iResult);

}  // iWriteMCP4017()

/** *************************************************************
 * \brief Reads a value from the potentiometer data by TWI bus.  Assumes the TWI bus
 *  routing is already been configured.
 * \param puiValue is ptr to where to put the value
 * \return int error codes from err_codes.h
 */
int iReadMCP4017(uint8_t *puiValue)
{
    int iResult;

    // Address the device
    iResult = iSetTWISlaveAdr(TWI_PORT0_NUM,MCP4017_DEFAULT_ADDR);
    if (ERR_NONE == iResult) {
        do {
            iResult = iTWIReadBuf(TWI_PORT0_NUM, ucaTWIInBuf, 1, true);
        } while (ERR_BUSY == iResult);
	usleep(100000);
    }
    *puiValue = ucaTWIInBuf[0];

    return(iResult);
}  // iReadMCP4017()

