/**
 * \file MCP24AA02_hal.c
 *
 * \brief MCP 24AA02UID firmware device driver; application specific
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
 * Low level device drivers for the Microchip 24AA02UID I2C bus EEPROM.  Implemented for custom
 * configuration of the SC1 hashing board rather than as a generic device driver.
 *
 *
 * \section Module History
 * - 20180531 Rev 0.1 Started; using AS7 (build 7.0.1417)
 *
 */

/***    INCLUDE FILES       ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "Usermain.h"
#include "TWI_Support.h"
#include "MCP24AA02_Defines.h"
#include "MCP24AA02_hal.h"

/***    LOCAL DEFINITIONS     ***/
#define MCP24AA02_PORT  TWI_PORT0_NUM
#define UID_ERROR_VALUE     0x1122334455667788   // return this if we can't get the UID from device
#define UID_TIMEOUT_VALUE   0x8877665544332211   // return this if we can't get the UID from device

/***    LOCAL STRUCTURES/ENUMS      ***/

/***    LOCAL FUNCTION PROTOTYPES   ***/
static int iReadUID(uint64_t *puiUIDValue);

/***    LOCAL DATA DECLARATIONS     ***/


/***    GLOBAL DATA DECLARATIONS   ***/
/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/

/** *************************************************************
 * \brief Accessor function to return the unique ID information as a 64-bit number.
 * If the expected device cannot be read, the UID will be set to UID_ERROR_VALUE.
 * \param uiBoard sets which board to target
 * \param puiUID ptr to uint64_t to return unique id code
 * \return int error codes from err_codes.h
 * \retval ERR_NONE if successful
 * \retval other for errors
 * Assumes any I2C routing connections have been previously configured.
 */
int iGetUID(uint8_t uiBoard, uint64_t *puiUIDValue)
{
    int iRetval;

    iRetval = iReadUID(puiUIDValue); // have to read the device since we don't have a valid code
    if (ERR_NONE != iRetval) {
        *puiUIDValue = UID_ERROR_VALUE;
    }

    return iRetval;
}   // iGetUID()

/** *************************************************************
 * \brief Reads the unique ID information.  First sets the address pointer and then reads back
 * the data. The 24AA02 device used has a unique 32-bit number and 2-bytes of manufacturer ID and device ID.
 * All are combined into a 64-bit value of unique ID.
 * The unique ID is formatted as follows:
 * bytes 0-3 Unique 32-bit value
 * bytes 4-5 should be 0 for now (but could include a full 48-bit value in future)
 * byte 6  storage device ID; expected as 0x41 for now but may change in future
 * byte 7  storage device mfg code; expected ax 0x29 for now but may change in future
 * \param puiUID ptr to uint64_t to return unique id code
 * \return int error codes from err_codes.h
 */
static int iReadUID(uint64_t *puiUIDValue)
{
#define MCP24AA02_READ_BUFFER_BYTES 6
#define MCP24AA02_READ_UID_COUNT 6
    uint8_t ucaUIDInBuf[MCP24AA02_READ_BUFFER_BYTES];     // buffer for reading in
    uint64_t uiUIDTemp = UID_ERROR_VALUE;
    int iResult = ERR_NONE;
    int ixI;

    // clear the input buffer
    for (ixI = 0; ixI < MCP24AA02_READ_BUFFER_BYTES; ixI++) 
    {
        ucaUIDInBuf[ixI] = 0;
    }

    // Address the device
    iResult = iSetTWISlaveAdr(MCP24AA02_PORT,TWI_SA_MCP24AA02);
    if (ERR_NONE == iResult) 
    {

        for (ixI = 0; ixI < MCP24AA02_READ_UID_COUNT; ixI++) 
	{
            // The read buffer also sets the address pointer (where we will read from)
            // Reading 1 at a time
            do
            {
                iResult = iTWIReadReg(MCP24AA02_PORT, (E_MFG_ID_OFFSET+ixI), &ucaUIDInBuf[ixI], 1, true);  // wait for it to happen
            }
	    while (ERR_BUSY == iResult);
            usleep(1000);
        }  // for

        if (ERR_NONE == iResult) 
        {
            // Build the returned value into the UID
            uiUIDTemp  = ((uint64_t)ucaUIDInBuf[0] << 56);    // Mfg ID into msByte; clears out rest of the value
            uiUIDTemp |= ((uint64_t)ucaUIDInBuf[1] << 48);    // add in Device ID
            uiUIDTemp |= ((uint64_t)ucaUIDInBuf[2]);          // add in bytes of the 32-bit UID
            uiUIDTemp |= ((uint64_t)ucaUIDInBuf[3] << 8);
            uiUIDTemp |= ((uint64_t)ucaUIDInBuf[4] << 16);
            uiUIDTemp |= ((uint64_t)ucaUIDInBuf[5] << 24);
        } 
	else
	{
            // got an error or a timeout
            uiUIDTemp = UID_TIMEOUT_VALUE;
        }
    } // if (ERR_NONE == iResult)
    *puiUIDValue = uiUIDTemp;

    return(iResult);
}  // iReadUID()



