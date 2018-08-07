/**
 * \file TCA9546A_hal.c
 *
 * \brief generic TCA9546A firmware device driver
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
 * Low level device drivers for the TCA9546A I2C bus switch.  Implemented fas a generic
 * device driver for the ATSAMG55XP board.
 *
 * \section Module History
 * - 20180531 Rev 0.1 Started; using AS7 (build 7.0.1417)
 *
 */

/***    INCLUDE FILES       ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>   // PRIxx type printf; 64-bit type support

#include "Usermain.h"
#include "TWI_Support.h"
#include "TCA9546A_Defines.h"
#include "TCA9546A_hal.h"
#include "i2c.h"
#include "MiscSupport.h"

/***    LOCAL DEFINITIONS     ***/
#define DEV_INIT_DEV_MESSAGE "-I- TCA9546A Init: "

#define DEV_READ_BUFF_SIZE  2  // max bytes size we expect to have to transfer to the device
#define DEV_WRITE_BUFF_SIZE DEV_READ_BUFF_SIZE

/***    LOCAL STRUCTURES/ENUMS      ***/

/***    LOCAL FUNCTION PROTOTYPES   ***/
static uint8_t iHashBoardToTwiSwitch(uint8_t uiBoard);
static int iWriteTwiSwitch(uint8_t uiPortNum);    // set switch mask reg based on downstream port; (0=none; 1-4= port num)
static int iWriteTwiSwitchMask(E_TWI_SWITCH_PORTS_T eSwitchMask);    // set switch mask directly
static int iReadTwiSwitchMask(E_TWI_SWITCH_PORTS_T *peSwitchMask);   //
static int iGetTwiSwitchMask(E_TWI_SWITCH_PORTS_T *peSwitchMask);   //

/***    LOCAL DATA DECLARATIONS     ***/

static uint8_t ucaTWIOutBuf[DEV_WRITE_BUFF_SIZE];    // buffer for sending out
// static uint8_t ucaTWIInBuf[DEV_READ_BUFF_SIZE];      // buffer for reading in

static E_TWI_SWITCH_PORTS_T eMaskMemory = E_TWI_PORT_DISABLE;    // temporary memory until we get readback working

/***    GLOBAL DATA DECLARATIONS   ***/
/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/

/** *************************************************************
 * \brief Accessor function sets the control board TWI/I2C bus switch to one of the four downstream
 *  ports that directs TWI communications to the desired hashing board. Allows only single port connection.
 * \param uiPort defines which board to connect to; range 0 to 2
 * \return int error codes from err_codes.h
 */
int iSetTwiSwitch(uint8_t uiBoard)
{
    return( iWriteTwiSwitch(iHashBoardToTwiSwitch(uiBoard)) );
}       // iSetTwiSwitch()

/** *************************************************************
 * \brief Sets the TWI/I2C bus switch to one of the four downstream ports.
 * Allows only single port connection.  Abstracts slightly away from the bit mask.
 * \param uiPort defines which downstream port number to connect; 0 for none; 1 to 4
 * \return int error codes from err_codes.h
 */
static int iWriteTwiSwitch(uint8_t uiPortNum)
{
    int iRetval = ERR_NONE;
    E_TWI_SWITCH_PORTS_T eSwitchMask;

    switch (uiPortNum) {
        // convert port number into the bit mask of the switch
        default:    // no device (disconnect downstream ports)
        case 0:
            eSwitchMask = E_TWI_PORT_DISABLE;
            break;
        case 1:
            eSwitchMask = E_TWI_PORT_ENB1;
            break;
        case 2:
            eSwitchMask = E_TWI_PORT_ENB2;
            break;
        case 3:
            eSwitchMask = E_TWI_PORT_ENB3;
            break;
        case 4:
            eSwitchMask = E_TWI_PORT_ENB4;
            break;
    } // end switch

    iRetval = iWriteTwiSwitchMask(eSwitchMask);  // #TODO - SOLVED the real controller does this
		
    return(iRetval);
}  // iWriteTwiSwitch()

/** *************************************************************
 * \brief Writes the bit mask to the switch register.  This is the primitive function
 * that supports full device functionality.
 * Assumes we only have one I2C com device numbered as the defined value TWI_PORT0
 * \param uiSwitchMask
 * \return int error codes from err_codes.h
 */
static int iWriteTwiSwitchMask(E_TWI_SWITCH_PORTS_T eSwitchMask)
{
    int iRetval;

    iRetval = iSetTWISlaveAdr(TWI_PORT0_NUM,TWI_SA_TCA9546A);
    if (ERR_NONE == iRetval) {
        ucaTWIOutBuf[0] = (uint8_t)eSwitchMask;
        iRetval = iTWIWriteBuf(TWI_PORT0_NUM, ucaTWIOutBuf, 1, true); // waits for completion
        eMaskMemory = eSwitchMask;
    }

    return(iRetval);
}  // iWriteTwiSwitchMask()

/** *************************************************************
 * \brief Reads the bit mask from the switch register.  This is the primitive function
 * that supports full device functionality.
 * TEMPORARY:just returns the memorized setting (so it is more of a get than a read function)
 * \param puiSwitchMask is ptr to where to return the value
 * \return int error codes from err_codes.h
 */
int iReadTwiSwitchMask(E_TWI_SWITCH_PORTS_T *peSwitchMask)
{
    int iRetVal;

    *peSwitchMask = eMaskMemory;

    /*//TODO:
    iRetVal = iSetTWISlaveAdr(TWI_PORT0_NUM, TWI_SA_TCA9546A);
    if(ERR_NONE == iRetVal)
    {
	iRetVal = iTWIReadBuf(TWI_PORT0_NUM, peSwitchMask, 1, true);
    }
    */

    return(iRetVal);
}  // iReadTwiSwitchMask()

/** *************************************************************

 * TEMPORARY:just returns the memorized setting (so it is more of a get than a read function)
 * \param puiSwitchMask is ptr to where to return the value
 * \return int error codes from err_codes.h
 */
int iGetTwiSwitchMask(E_TWI_SWITCH_PORTS_T *peSwitchMask)
{
    int iRetVal = ERR_NONE;

    *peSwitchMask = eMaskMemory;
    return(iRetVal);
}   // iGetTwiSwitchMask();

/** *************************************************************
 * \brief Convert the hash board number into the I2C switch port
 *  Assumes the board is valid (has already been checked)
 * Translation is trivial here but this allows some abstraction so we can
 * change it in the future or to perform other actions.
 * \param uint8_t uiBoard sets which board to target
 * \return iPortNumber
 */
static uint8_t iHashBoardToTwiSwitch(uint8_t uiBoard)
{
    return (uiBoard+1);
} // iHashBoardToTwiSwitch

