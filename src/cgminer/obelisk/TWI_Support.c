/**
 * \file TWI_Support.c
 *
 * \brief TWI/I2C Support driver related methods.
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
 * \section Purpose
 * Adds custom TWI/I2C functions needed for the application.  This is for an asynchronous master type interface
 * (interrupt driven with callbacks). This has support for multiple TWI buses (ports) where each bus can
 * support multiple devices.  The code is custom to the design and not quite generic enough for general
 * use. It allows a mix of unique and general purpose functions.
 *
 * This includes a very light duty bus claim and release method to support operation of multiple
 * independent state machines that are associated with each device.
 *
 * \section Module History
 * - 20180517 Rev 0.1 Started; using AS7 (build 7.0.1417) under START/ASF4; Atmel SAMG55
*/

/***    INCLUDE FILES       ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

//#include <compiler.h>
//#include <utils.h>
//#include "atmel_start.h"
#include "atmel_start_pins.h"

#include "Usermain.h"
#include "Ob1Hashboard.h"
#include "TCA9546A_hal.h"
#include "TWI_Support.h"

#include "gpio_bsp.h"
#include "i2c.h"
#include "MiscSupport.h"
#include "Console.h"



#if (0 == NUM_TWI_PORTS)
#error "invalid number of ports"    // so we don't have to worry about this in real time
#endif

#if (HW_ATSAMG55_XP_BOARD != HARDWARE_PLATFORM)
//#include "hal_i2c_m_async.h"    // master async type transfers used
#endif // #if (HW_ATSAMG55_XP_BOARD != HARDWARE_PLATFORM)

/***    LOCAL DEFINITIONS     ***/
#define TWI_INIT_MESSAGE "-I- TWI (ATSAMG55) Init: "

// Set these up depending on the ports used
#define TWI_PORT0_NAME  2    // named this in Atmel START
#define TWI_SWC0_NAME   PB9    // named this in Atmel START - CLOCK
#define TWI_SWD0_NAME   PB8    // named this in Atmel START - DATA
#define TWI_SWC0_MUX    MUX_PB9A_FLEXCOM4_TWCK

// Pseudo-reset of I2C; clocks the TWCK/SCK line holding TWD/SDA high.
#define TWCK_RESET_PULSES 12  // num of clock pulses to "reset" I2C; 10 min

// #define ENABLE_TWI_TOKENS 1

#ifdef ENABLE_TWI_TOKENS
// TWI port enumerated values
typedef enum  {
    E_TWI_UNCLAIMED    = 0x00,
    E_TWI_CLAIMED      = 0x01
} E_TWI_PORT_TOKEN_T;

// Each port can have multiple owners; For now it just needs to know if port is idle or busy.
// Using an int type in case we later want to assign a handle to this to know who has the token.
E_TWI_PORT_TOKEN_T eTWIPortToken[NUM_TWI_PORTS];

#endif  // #ifdef ENABLE_TWI_TOKENS

/***    LOCAL FUNCTION PROTOTYPES   ***/
// For port 0; only need one for this application
/*
static void cb_Default_TWI0_tx_complete(struct i2c_m_async_desc *const i2c);
static void cb_Default_TWI0_rx_complete(struct i2c_m_async_desc *const i2c);
static void cb_Default_TWI0_error(struct i2c_m_async_desc *const i2c);
*/

#if (1 < NUM_TWI_PORTS)
// For port 1; if we need another
/*
static void cb_Default_TWI1_tx_complete(struct i2c_m_async_desc *const i2c);
static void cb_Default_TWI1_rx_complete(struct i2c_m_async_desc *const i2c);
static void cb_Default_TWI1_error(struct i2c_m_async_desc *const i2c);
*/
#endif // #if (1 < NUM_TWI_PORTS)

// static void i2c_tx_complete(struct _i2c_m_async_device *const i2c_dev);
/*
static int32_t iTWI_m_async_read_buffer(struct i2c_m_async_desc *const i2c, uint8_t reg, uint8_t *buf, uint16_t n);
static int32_t iTWI_m_async_read_bytes(struct i2c_m_async_desc *const i2c, uint8_t *buf, uint16_t n);
*/

/***    LOCAL DATA DECLARATIONS     ***/

static int i2cSlaveAddress = 0;

static bool baTWIPortValid[NUM_TWI_PORTS];  // false if port is not initialized

// Descriptors and callbacks
static struct io_descriptor *pioa_TWIPort[NUM_TWI_PORTS];
static bool volatile baTWITXCallbackFlag[NUM_TWI_PORTS];
static bool volatile baTWIRXCallbackFlag[NUM_TWI_PORTS];
static bool volatile baTWIErrorCallbackFlag[NUM_TWI_PORTS];
static uint32_t uiCallbackTimeoutCtr[NUM_TWI_PORTS];


/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/

// TWI port token support related; this is used if there are multiple state machines sharing a TWI port and provides
// a way for them to claim and release the bus
#ifdef ENABLE_TWI_TOKENS
/** ============================================================================
 * \brief Accessor function to get access to the desired TWI port.  Call this to check if
 * the resource is available and to claim the token to prevent others from using it.  This may
 * be used if supporting multiple state machines for communicating with multiple devices that
 * share a common TWI bus. Claiming the token does not prevent others from using it and corrupting
 * operation.  But it allows functions to share a bus, to attempt to claim the token, retry if
 * it is busy, and such.
 * \param[in] uint8_t uiTWIPort defines which TWI bus is wanted
 * \return int
 * \retval ERR_NONE if port access granted; else port is busy (ERR_BUSY) or other
 *
 * Implements a very basic claim/release that will probably suffice for simple systems.
 * This allows functions to access the port(s) without conflicting by grabbing the token.
 */
int iTWITokenClaim(uint8_t uiTWIPort)
{
    int iRetVal = ERR_NONE;

    if (NUM_TWI_PORTS > uiTWIPort) {
        if (E_TWI_UNCLAIMED == eTWIPortToken[uiTWIPort]) {  // if TWI Port is idle then allow the claim
            eTWIPortToken[uiTWIPort] = E_TWI_CLAIMED; // allow claim
        } else {
            iRetVal = ERR_BUSY;
        }
    } else {
        iRetVal = ERR_INVALID_DATA;
    } // if (NUM_TWI_PORTS > uiTWIPort)
    return(iRetVal);
}   // iTWITokenClaim()

/** ============================================================================
 * \brief Accessor function to release access to the TWI port.  This should be done after
 * completing whatever transfers are needed so other functions can share the bus.
 * \param[in] uint8_t uiTWIPort defines which port number
 * \return int
 * \retval ERR_NONE if port release granted; else error
 */
int iTWITokenRelease(uint8_t uiTWIPort)
{
    int iRetVal = ERR_NONE;

    if (NUM_TWI_PORTS > uiTWIPort) {
        eTWIPortToken[uiTWIPort] = E_TWI_UNCLAIMED; // release claim
    } else {
        iRetVal = ERR_INVALID_DATA;
    } // if (NUM_TWI_PORTS > uiTWIPort)
    return(iRetVal);
}   // iTWITokenRelease()
#endif  // #ifdef ENABLE_TWI_TOKENS

/** ============================================================================
 * \brief bIsTWIValid
 * Accessor function to get if the TWI port is valid.  The definition of valid may be
 * application dependent but it probably means it exists and is initialized.
 * \param[in] uint8_t uiTWIPort defines which port number to return
 * \return bool is true if the port is valid and initialized
 */
bool bIsTWIValid(uint8_t uiTWIPort)
{
    if (NUM_TWI_PORTS > uiTWIPort) {
        return(baTWIPortValid[uiTWIPort]);
    } else {
        return(false);
    } // if (NUM_TWI_PORTS > uiTWIPort)
}   // bIsTWIValid()

/** *************************************************************
 * \brief Transport the TWI message to the device.
 * \param psOutBuf structure that defines the data transfer
 * \return int error codes from err_codes.h
 */
int iTWIInitXFer(S_TWI_TRANSFER_T *psOutBuf)
{
    int iResult, i2cData;
    // If application uses it, arbitrate for control of the TWI bus

    // Note: normally psOutBuf->ePort defines with TWI port in a multiport design.  In this design we
    // only have one TWI port but we have multiple boards that use a TWI port switch so we repurpose the
    // variable to define which board to switch to.
    iResult = iSetTwiSwitch(psOutBuf->ePort); // Based on routing information (which board) set the TWI bus switch
    if (ERR_NONE == iResult) {
        iResult = iSetTWISlaveAdr(TWI_PORT0_NUM, psOutBuf->uiAddr);
    }
	
	if (ERR_NONE == iResult) 
	{
        // Check for mode as write or read
        if (E_TWI_WRITE == psOutBuf->eXferMode) 
		{
            iResult = iTWIWriteBuf(TWI_PORT0_NUM, psOutBuf->puiaData, psOutBuf->uiCount, true);
			if(0 > iResult)
			{
				printf("-E- I2C Write Buffer Failed\n");	
			}
        } 
		else if (E_TWI_READ == psOutBuf->eXferMode) 
		{
            iResult = iTWIReadBuf(TWI_PORT0_NUM, psOutBuf->puiaData, psOutBuf->uiCount, true);
			if(0 > iResult)
			{
				printf("-E- I2C Read Buffer Failed\n");	
			}
        } 
		else if (E_TWI_READ_REG == psOutBuf->eXferMode) 
		{
            iResult = iTWIReadReg(TWI_PORT0_NUM, psOutBuf->uiReg, psOutBuf->puiaData, psOutBuf->uiCount, true);
			if(0 > iResult)
			{
				printf("-E- I2C Read Register Failed\n");	
			}
        } 
		else 
		{
            iResult = ERR_INVALID_DATA;
        }
    }
	
	//TODO: translate i2cStatus into a valid error code and return that code
	
    return (iResult);

} // TWIInitXFer()


/** ============================================================================
 *  \brief iSetTWISlaveAdr
 *  Update the slave address to the TWI port.  This remembers the slave address for each of its
 *  supported TWI ports and only changes as needed.  The ATSAMG55 TWI peripheral has a separate register
 *  for slave address that this will set.  The slave address is not normally an integral part of the data
 *  buffer associated with the transfer.
 *  This also, includes a potential fix for a bug in the autogenerated code for _i2c_m_async_transfer()
 *  that results in output of the TWI transfer before the slave adr gets updated.
 *  \param[in] uint8_t uiTWIPort defines which port number to use
 *  \param[in] uint16_t is slave address (the typical 7-bit I2C value but as a 16-bit)
 *  \return int error codes from err_codes.h
 *  \retval ERR_NONE if success
 *  \retval ERR_IO if error
 *  \retval ERR_INVALID_DATA if port is not recognized
 */
int iSetTWISlaveAdr(uint8_t uiTWIPort, uint16_t uiSlaveTgt)
{
	 
	i2cSlaveAddress = (int)uiSlaveTgt;
	
	return ERR_NONE;

} // iSetTWISlaveAdr()

/** ============================================================================
 *  \brief iTWIWriteBuf
 *  Write buffer to device. Assumes the target slave address is already set. The writes can be
 *  blocking or non-blocking on completion of the transfer.  If non-blocking then the caller can
 *  call repeatedly periodically and check for the ERR_BUSY status. It is expected that the process
 *  will eventually timeout and reset based on actions of a lower level function.
 *  \param[in] uint8_t uiTWIPort defines which TWI port to use (if there are multiple TWI ports)
 *  \param[in] uint8_t *puiValues is ptr to buffer to write
 *  \param[in] uint8_t iCount is count of bytes to write (does not include slave adr)
 *  \param[in] bool bWait is true for blocking wait; false for non-blocking
 *  \return int error codes from err_codes.h
 *  \retval ERR_NONE if success
 *  \retval ERR_BUSY if waiting for the call back
 *  \retval ERR_TIMEOUT if timeout
 *  \return other values (Timeout, etc)
 */
int iTWIWriteBuf(uint8_t uiTWIPort, uint8_t *puiValues, uint8_t iCount, bool bWait)
{
	int iRetVal;
	iRetVal = i2cset(i2cSlaveAddress, iCount, puiValues);
		   
	if(0 <= iRetVal)
	{
		return ERR_NONE;
	}
	else
	{
		return iRetVal;
	}

}    // iTWIWriteBuf

/** ============================================================================
 *  \brief iTWIReadReg
 *  Read value(s) from device register(s). Assumes the slave address is already set. Also, assumes that
 *  this is only called by a single process on a single TWI bus.
 *  Process can be blocking or non-blocking.  If non-blocking then the caller must call repeatedly until it no longer
 *  returns ERR_BUSY before taking the data.  The input parameters must be unchanged during the repeated calls and
 *  so cannot be preloaded for the next read operation.  This function is not reentrant but it can handle more than
 *  one TWI bus as it has one state machine per bus instantiation.
 *  A state machine design is used to support non-blocking operation so other tasks can be done while the transfer occurs.
 *  \param[in] uint8_t is register address
 *  \param[out] uint8_t is ptr to buffer to put data
 *  \param[in] uint8_t is count of bytes to read (does not include slave adr)
 *  \param[in] bool bWait is true for blocking wait; false for non-blocking
 *  \return int
 *  \retval ERR_NONE if success
 *  \retval ERR_BUSY if waiting for completion
 *  \retval ERR_TIMEOUT if timeout
 *  \retval ERR_IO if other error
 *
 */
int iTWIReadReg(uint8_t uiTWIPort, uint8_t uiReg, uint8_t *puiBuf, uint8_t iCount, bool bWait)
{
	int iRetVal;
	iRetVal = i2cget_reg(i2cSlaveAddress, uiReg, (int)iCount, puiBuf);
	
	if(0 <= iRetVal)
	{
		return ERR_NONE;
	}
	else
	{
		return iRetVal;
	}

}    // iTWIReadReg

/** ============================================================================
 *  \brief iTWIReadBuf()
 *  Reads n bytes from the TWI bus; differs from iTWIReadReg in that it does not send a
 *  register address before the read.
 *  \param[in] uint8_t uiTWIPort defines which port number
 *  \param[out] uint8_t is ptr to buffer to put data
 *  \param[in] uint8_t is count of bytes to read (does not include slave adr)
 *  \param[in] bool bWait is true for blocking wait; false for non-blocking
 *  \return ERR_NONE for success; any other value for failure to initialize
 *
 *  Assumes slave address was previously set (with iSetTWISlaveAdr()) and that any device
 *  comms routing is already configured.
 */
int iTWIReadBuf(uint8_t uiTWIPort, uint8_t *puiBuf, uint8_t iCount, bool bWait)
{
	int iRetVal;
	iRetVal = i2cget(i2cSlaveAddress, iCount, puiBuf);
	
	if(0 <= iRetVal)
	{
		return ERR_NONE;
	}
	else
	{
		return iRetVal;
	}

} // iTWIReadBuf

/** ============================================================================
 *  \brief iTWIPortInit()
 *  Called once at startup to set up the TWI port, callbacks, etc.
 *  \param[in] uint8_t uiTWIPort defines which port number to init; >=NUM_TWI_PORT to init all
 *  \return ERR_NONE for success; any other value for failure to initialize
 *
 *  TWI clock rate is controlled from START/ASF4 in lower level config functions by
 *  autogenerated code.  Here we register basic callbacks.  All ports use the
 *  master async mode drivers.  These don't block during execution unless made to.
 *  Usage: expect startup to call as something like:
 *      for (ixI = 0; ixI < NUM_TWI_PORTS; ixI) {
 *          (void)iTWIPortInit(ixI);  // ignoring return
 *      }
 *   or simply (void)iTWIPortInit(NUM_TWI_PORTS);
 *
 *  If this fails, it is possible that no TWI device was found or the device didn't respond to
 *  the issued general call.
 *  This requires an I2C type device to be present on the bus.  The controller board has an
 *  I2C bus switch.
 */
int iTWIPortInit(uint8_t uiTWIPort)
{
	return ERR_NONE;
//TODO: TEST I2C switch
//NOTE: Give it values of 0, 1, 2, 4, and 8
	
	
/*
    if (NUM_TWI_PORTS > uiTWIPort) {
        uiStart = uiTWIPort;
        uiEnd   = uiStart +1;
    } else {
        uiStart = 0;
        uiEnd   = NUM_TWI_PORTS;
    } // if (NUM_TWI_PORTS > uiTWIPort)
    for (uixI = uiStart; uixI < uiEnd; uixI++) {
        baTWIPortValid[uixI] = false;
        baError[uixI] = false;
#ifdef ENABLE_TWI_TOKENS
        eTWIPortToken[uiTWIPort] = E_TWI_UNCLAIMED;
#endif  // #ifdef ENABLE_TWI_TOKENS
    }   // for

    if ((0 == uiTWIPort) || (NUM_TWI_PORTS <= uiTWIPort)) {  // if just this one or all...

        CONSOLE_OUTPUT_IMMEDIATE(TWI_INIT_MESSAGE);

        // Run some transfers on startup; this tests basic operation including whether
        // the pull-ups are working, a target board is attached, etc.
        // This also sends a general call reset command to the bus.
        iResult = iSetTWISlaveAdr(TWI_PORT0_NUM,TEST_DEFAULT_ADDR);
        if (ERR_NONE != iResult) {
           baError[TWI_PORT0_NUM] = true;
        } else {
            for (uixI = 0; uixI < TWI_STARTUP_XFER_COUNT; uixI++) {
                if (0 == uixI) {
                    caChar[0] = TWI_GENCALL_RESET;    // general call SW reset code
                } else {    // if xfer count > 1 then fill remainder of buffer with 0's (or modify to put out other data)
                    caChar[uixI] = 0x0;
                }
            }   // for
            for (uixI = 0; uixI < TWI_STARTUP_TEST_CYCLES; uixI++) {
                caChar[0] = (uint8_t)uixI;    //  incrementing value to change the pot
                caChar[1] = (uint8_t)uixI;    //  incrementing value to change the pot
                ClearCallbackFlags(TWI_PORT0_NUM);
//TODO:                iResult = io_write(pioa_TWIPort[TWI_PORT0_NUM],caChar,(uint16_t)TWI_STARTUP_XFER_COUNT); // returns number written (count)
                if (TWI_STARTUP_XFER_COUNT != iResult) {
                    baError[TWI_PORT0_NUM] = true;
                } else {
                    // wait for the call back to verify it happens; ok to block here
                    iResult = iWaitForTWITXCallback(TWI_PORT0_NUM,true);
                    if (ERR_NONE != iResult) {
                        baError[TWI_PORT0_NUM] = true;
                    }
//TODO:                    delay_us(20);
                } // if (TWI_STARTUP_XFER_COUNT != iResult)
                if (true == baError[TWI_PORT0_NUM]) {
                    break; // for()
                }
            } // for()

            if (false == baError[TWI_PORT0_NUM]) {
                baTWIPortValid[TWI_PORT0_NUM] = true;   // validate the port if no error
            } // if (false == baError[TWI_PORT0_NUM])
        }   // if (ERR_NONE != iResult)

#ifdef DEBUG_CONSOLE
        if (false == baError[TWI_PORT0_NUM]) {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "passed\r\n");
        } else {
            (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "failed\r\n");
        }
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
#endif // #ifdef DEBUG_CONSOLE

    } // if ((0 == uiTWIPort) || (NUM_TWI_PORTS <= uiTWIPort))

    // <Add other TWI port initializations here>

    // Set return value based on errors found
    for (uixI = uiStart; uixI < uiEnd; uixI++) {
        if (true == baError[uixI]) {
            iRetVal = ERR_IO;  // report a problem; non-specific port
            break; // for loop
        }
    }   // for	
    return (iRetVal);
*/
} // iTWIPortInit

/** ============================================================================
 * \brief I2C devices don't have a reset line and may get stuck in a mode that prevents use.
 * To "reset" the I2C devices, we pulse the TWCK 10 or more times to ensure they are out of any
 * prior modes they may have been stuck in.  This happens if the device is reset unexpectedly
 * as when using the ICE or when a reset happens unexpectedly.
 */
void CycleTWCK(uint8_t uiTWIPort)
{
//TODO: Tom - Not Needed?
/*
    int ixI;

    if (0 == uiTWIPort) {
        // Need to set the TWCK port as a gpio
        i2c_m_async_disable(&TWI_PORT0_NAME);
        gpio_set_pin_function(TWI_SWC0_NAME, GPIO_PIN_FUNCTION_OFF);
        gpio_set_pin_direction(TWI_SWC0_NAME, GPIO_DIRECTION_OUT);
        for (ixI = 0; ixI < TWCK_RESET_PULSES; ixI++) {
            gpio_set_pin_level(TWI_SWC0_NAME, false);
            delay_us(5);
            gpio_set_pin_level(TWI_SWC0_NAME, true);
            delay_us(5);
        }
        // Need to put the pin for TWCK back to I2C
        gpio_set_pin_function(TWI_SWC0_NAME, TWI_SWC0_MUX);

        i2c_m_async_enable(&TWI_PORT0_NAME);

#if (1 < NUM_TWI_PORTS)
    } else if (1 == uiTWIPort) {
        // Need to set the TWCK port as a gpio
        i2c_m_async_disable(&TWI_PORT1_NAME);
        gpio_set_pin_function(TWI_SWC1_NAME, GPIO_PIN_FUNCTION_OFF);
        gpio_set_pin_direction(TWI_SWC1_NAME, GPIO_DIRECTION_OUT);
        for (ixI = 0; ixI < TWCK_RESET_PULSES; ixI++) {
            gpio_set_pin_level(TWI_SWC1_NAME, false);
            delay_us(5);
            gpio_set_pin_level(TWI_SWC1_NAME, true);
            delay_us(5);
        }
        // Need to put the pin for TWCK back to I2C
	    gpio_set_pin_function(TWI_SWC1_NAME, TWI_SWC1_MUX);

        i2c_m_async_enable(&TWI_PORT1_NAME);
#endif  // #if (1 < NUM_TWI_PORTS)

    }
*/
}   // CycleTWCK

/** ============================================================================
 * \brief ClearCallbackFlags()
 * Clears the Call back flags on the designated port; also clears the timeout counter.
 * This should be called before beginning the transfer.
 * \param[in] uint8_t uiTWIPort defines which port number to use
 * \return none
 */
void ClearCallbackFlags(uint8_t uiTWIPort)
{
    int ixI;
    uint8_t uiStart, uiEnd;

    if (NUM_TWI_PORTS > uiTWIPort) {
        uiStart = uiTWIPort;
        uiEnd   = uiStart +1;
    } else {
        uiStart = 0;
        uiEnd   = NUM_TWI_PORTS;
    } // if (NUM_TWI_PORTS > uiTWIPort)
    for (ixI = uiStart; ixI < uiEnd; ixI++) {
        baTWITXCallbackFlag[ixI] = false;
        baTWIRXCallbackFlag[ixI] = false;
        baTWIErrorCallbackFlag[ixI] = false;
        uiCallbackTimeoutCtr[ixI] = 0;
    }   // for

} // ClearCallbackFlags


/** ============================================================================
 * \brief iWaitForTWITXCallback()
 * Function waits for the TX callback on the desired port. Assumes the callback flag
 * and timeout counter was previously cleared.
 * \param[in] uint8_t uiTWIPort defines which port number to use
 * \param[in] bool is true to wait (block); false to return ERR_BUSY if not set
 * \return int
 * \retval ERR_NONE if bool is true
 * \retval ERR_TIMEOUT if this times out
 * \retval ERR_IO if a call back error is true
 * \retval ERR_BUSY if pcallback flag is not true and not waiting
 * \retval ERR_INVALID_DATA if port is not valid
 */
int iWaitForTWITXCallback(uint8_t uiTWIPort, bool bWait)
{
    int iRetVal = ERR_NONE;

    if (NUM_TWI_PORTS > uiTWIPort) {
        // check the call back to verify we got it
        while (false == baTWITXCallbackFlag[uiTWIPort]) {
            if (TWI_CALLBACK_TIMEOUT <= ++uiCallbackTimeoutCtr[uiTWIPort]) {
                iRetVal = ERR_TIMEOUT;
                break; // while
            } else if (true == baTWIErrorCallbackFlag[uiTWIPort]) {
                iRetVal = ERR_IO;
                break; // while
            } else if (false == bWait) {
                iRetVal = ERR_BUSY;
                break; // while
            }
        } // while()

    } else {
        return(ERR_INVALID_DATA);
    }

    return(iRetVal);
}  // iWaitForTWITXCallback

/** ============================================================================
 * \brief iWaitForTWIRXCallback()
 * Function waits for the TX callback on the desired port. Assumes the callback flag
 * was previously cleared.
 * \param[in] uint8_t uiTWIPort defines which port number to use
 * \param[in] bool is true to wait (block); false to return ERR_BUSY if not set
 * \return int
 * \retval ERR_NONE if bool is true
 * \retval ERR_TIMEOUT if this times out
 * \retval ERR_IO if a call back error is true
 * \retval ERR_BUSY if pcallback flag is not true and not waiting
 * \retval ERR_INVALID_DATA if port is not valid
 */
int iWaitForTWIRXCallback(uint8_t uiTWIPort, bool bWait)
{
    int iRetVal = ERR_NONE;

    // Selective compile; If running on the XP eval board then we don't have the TWI connections we expect. So don't wait.
#if (HW_ATSAMG55_XP_BOARD != HARDWARE_PLATFORM)

    if (NUM_TWI_PORTS > uiTWIPort) {
        // check the call back to verify we got it
        while (false == baTWIRXCallbackFlag[uiTWIPort]) {
            if (TWI_CALLBACK_TIMEOUT <= ++uiCallbackTimeoutCtr[uiTWIPort]) {
                iRetVal = ERR_TIMEOUT;
                break; // while
            } else if (true == baTWIErrorCallbackFlag[uiTWIPort]) {
                iRetVal = ERR_IO;
                break; // while
            } else if (false == bWait) {
                iRetVal = ERR_BUSY;
                break; // while
            }
        } // while()

    } else {
        return(ERR_INVALID_DATA);
    }

#endif // #if (HW_ATSAMG55_XP_BOARD != HARDWARE_PLATFORM)

    return(iRetVal);
}  // iWaitForTWIRXCallback

/** ============================================================================
 * \brief TWI transmit completed callback; actually may callback when TX buffer is empty but
 * the transfer may still be active.
 *  DO NOT CALL THIS DIRECTLY!  This is registered to an event and is executed by that event.
 */
/*
static void cb_Default_TWI0_tx_complete(struct i2c_m_async_desc *const i2c)
{
     baTWITXCallbackFlag[0] = true;
} // cb_Default_TWI0_tx_complete
*/

/** ============================================================================
 * \brief TWI receive completed callback
 *  DO NOT CALL THIS DIRECTLY!  This is registered to an event and is executed by that event.
 */
/*
static void cb_Default_TWI0_rx_complete(struct i2c_m_async_desc *const i2c)
{
     baTWIRXCallbackFlag[0] = true;
} // cb_Default_TWI0_rx_complete
*/

/** ============================================================================
 * \brief TWI error detected callback
 *  DO NOT CALL THIS DIRECTLY!  This is registered to an event and is executed by that event.
 */
/*
static void cb_Default_TWI0_error(struct i2c_m_async_desc *const i2c)
{
     baTWIErrorCallbackFlag[0] = true;
} // cb_Default_TWI0_error
*/

#if 0   // compiled out; because we only need 1 TWI port
/** ============================================================================
 * \brief TWI transmit completed callback; actually may callback when TX buffer is empty but
 * the transfer may still be active.
 *  DO NOT CALL THIS DIRECTLY!  This is registered to an event and is executed by that event.
 */
/*
static void cb_Default_TWI1_tx_complete(struct i2c_m_async_desc *const i2c)
{
     baTWITXCallbackFlag[TWI_PORT_MCU] = true;
} // cb_Default_TWI1_tx_complete
*/

/** ============================================================================
 * \brief TWI receive completed callback
 *  DO NOT CALL THIS DIRECTLY!  This is registered to an event and is executed by that event.
 */
/*
static void cb_Default_TWI1_rx_complete(struct i2c_m_async_desc *const i2c)
{
     baTWIRXCallbackFlag[TWI_PORT_MCU] = true;
} // cb_Default_TWI1_rx_complete
*/

/** ============================================================================
 * \brief TWI error detected callback
 *  DO NOT CALL THIS DIRECTLY!  This is registered to an event and is executed by that event.
 */
/*
static void cb_Default_TWI1_error(struct i2c_m_async_desc *const i2c)
{
     baTWIErrorCallbackFlag[TWI_PORT_MCU] = true;
} // cb_Default_TWI1_error
*/

#endif // #if 0   // compiled out; we only need 1 TWI port

/** ============================================================================
 * \brief iTWI_m_async_read_buffer
 * Custom version of hal_i2c_m_sync.c function i2c_m_async_cmd_read() but reads a buffer instead of a single byte.
 * This only starts the read and the caller needs to wait for the callback to know when
 * this has completed.  This uses the repeated start method to first send an address pointer
 * (e.g. a register offset) and then read the value(s) from that address.  Many devices
 * auto-increment the address pointer.  The original Atmel function only read a single byte.
 * The sequence of this routine is:
 * sta->address(write)->ack->reg address->ack->sta_repeat->address(read)->ack->reg
 * value->ack->..regvalue->ack->..regvalue->nack->..stt
 *
 * \param[in] i2c An I2C master descriptor, which is used to communicate through I2C
 * \param[in] reg The internal address/register of the I2C slave device
 * \param[out] *buf is ptr to buffer to put read values
 * \param[in] n is count of number of bytes to read
 *
 * \return int error codes from err_codes.h
 * \retval ERR_NONE Reading from register is completed successfully
 * \retval <0 The passed parameters were invalid or read fail
 */
/*
static int32_t iTWI_m_async_read_buffer(struct i2c_m_async_desc *const i2c, uint8_t reg, uint8_t *buf, uint16_t n)
{
    static _i2c_complete_cb_t pfuncCBTXComplete; // added: remembers the previous TX complete call back
	static struct _i2c_m_msg        msg;
	int32_t         ret;

    // start of send the register address
    msg.addr   = i2c->slave_addr;
	msg.len    = 1;
	msg.flags  = 0;
	msg.buffer = &reg;

    // First part this sends the register address using a start/write.
    // The while loop should block for a short time only.
    pfuncCBTXComplete = i2c->device.cb.tx_complete;
	i2c->device.cb.tx_complete = NULL;

	ret = _i2c_m_async_transfer(&i2c->device, &msg);
	if (ret != ERR_NONE) {
    	// error occurred 
    	// re-register to enable notify user callback
    	i2c->device.cb.tx_complete = pfuncCBTXComplete;
    	return ERR_IO;
    }

    // Wait for register write to complete; slightly blocking and probably not worth turning into
    // a state machine here. Polling busy flag to wait for send finished here (set not busy by irq callback)
    // This is only waiting for the TX buf to clear so it doesn't take long.
	while (i2c->device.service.msg.flags & I2C_M_BUSY) {
    	;
	}

	// re-register to enable notify user callback
	i2c->device.cb.tx_complete = pfuncCBTXComplete;

    // begin the read part of the transfer
    msg.len    = n;
	msg.flags  = I2C_M_RD;
    msg.buffer = buf;

    ret = _i2c_m_async_transfer(&i2c->device, &msg);
	if (ret != ERR_NONE) {
    	// error occurred
    	return ERR_IO;
	}

	return ERR_NONE;

}   // iTWI_m_async_read_buffer()
*/

/** ============================================================================
 * \brief iTWI_m_async_read_bytes
 * Like iTWI_m_async_read_buffer but does not send a register address prior to the read.
 * This only starts the read and the caller needs to wait for the callback to know when
 * this has completed.
 * The sequence of this routine is:
 * sta->address(read)->ack->reg value->ack->..regvalue->ack->..regvalue->nack->..stt
 *
 * \param[in] i2c An I2C master descriptor, which is used to communicate through I2C
 * \param[out] *buf is ptr to buffer to put read values
 * \param[in] n is count of number of bytes to read
 *
 * \return int error codes from err_codes.h
 * \retval ERR_NONE Reading from register is completed successfully
 * \retval <0 The passed parameters were invalid or read fail
 */
/*
static int32_t iTWI_m_async_read_bytes(struct i2c_m_async_desc *const i2c, uint8_t *buf, uint16_t n)
{
	static struct _i2c_m_msg msg;
	int32_t         ret;

    // begin the read part of the transfer
    msg.addr   = i2c->slave_addr;
    msg.len    = n;
	msg.flags  = I2C_M_RD;
    msg.buffer = buf;

    ret = _i2c_m_async_transfer(&i2c->device, &msg);
	if (ret != ERR_NONE) {
    	// error occurred
    	return ERR_IO;
	}

	return ERR_NONE;

}   // iTWI_m_async_read_bytes()
*/


#if 0   // optional method of calculation of a checksum for some TWI devices
/* PEC.c
 * Implements a CRC-8 checksum using the direct method.
 * Modified from Intersil AN2030.pdf
 */
// For support of devices that require CRC code with data (e.g. MLX90614).  Can be used if we want to write
// to it or check its return CRC when we read. For now we aren't doing this so we don't need the function.
/* function prototypes */
uint8_t ucPEC_ProcessByte(uint8_t ucInputData);
void PEC_ResetCRC(void);

static uint8_t ucPEC_CurrentCRC; //CRC calculation of all bytes called through CRC_Process_Byte since last call to CRC_Reset

// CRC_Process_Byte performs a direct-mode calculation of
// one byte along with a previous CRC calculation
// See Atmel Doc2583 for app note about SMbus.
uint8_t ucPEC_ProcessByte(uint8_t ucInputData)
{
#define PEC_CRC8_POLYNOMIAL 0x8380 //polynomial of x^8 + x^2 + x^1 + 1 in most significant 9 bits
#define PEC_CRC8_DONE_MASK  0x0083 //CRC is done after polynomial shifts one byte

    uint16_t uiCRCTemp;
    uint16_t uiPolyTemp = PEC_CRC8_POLYNOMIAL; // polynomial shifts as opposed to CRC
    uint16_t uiTestMask = 0x8000;  // used to evaluate whether we should XOR

    // XOR previous CRC and current input for multi-byte CRC calculations
    // temporary is shifted left one byte to perform direct mode calculation
    uiCRCTemp = ( ( (uint16_t)(ucPEC_CurrentCRC ^ ucInputData) ) << 8 );
    do {
        if (uiCRCTemp & uiTestMask) {
            uiCRCTemp = uiCRCTemp ^ uiPolyTemp;
        }
        uiTestMask = uiTestMask >> 1;
        uiPolyTemp = uiPolyTemp >> 1;
    } while (uiPolyTemp != PEC_CRC8_DONE_MASK);
    ucPEC_CurrentCRC = (uint8_t) uiCRCTemp;
    return(ucPEC_CurrentCRC);
} // ucPEC_ProcessByte()

//CRC_Reset will reset PEC_CurrentCRC to 0
//this should be called before a new multi-byte calculation needs to be done
void PEC_ResetCRC(void)
{
    ucPEC_CurrentCRC = 0;
}
#endif // #if 0  // optional method of calculation of a checksum for some TWI devices

#if 0   // leave here but compile out; was a workaround to fix a problem
/** ============================================================================
 * \brief Modified version from hpl_twi.c.  This corrects the order of operations
 * problem that results in an immediate interrupt on a TX empty condition before the
 * operation is properly configured.
 *
 * Duplicated here so this doesn't get lost when rebuilt.  Just cut and paste back into
 * the hpl_twi.c file to replace the existing function.
 * Updated note (6/6/2018): looks like they fixed this problem so leaving this code here
 * for now as dead code.
 */
int32_t _i2c_m_async_transfer(struct _i2c_m_async_device *const dev, struct _i2c_m_msg *msg)
{
    ASSERT(dev && msg);

    if (dev->service.msg.flags & I2C_M_BUSY) {
        return I2C_ERR_BUSY;
    }

    msg->flags |= I2C_M_BUSY;
    dev->service.msg = *msg;

    if (msg->flags & I2C_M_RD) {
        if (msg->addr & I2C_M_TEN) {
            hri_twi_write_MMR_reg(dev->hw, TWI_MMR_DADR(0x78 | (msg->addr >> 8)) | TWI_MMR_IADRSZ(1) | TWI_MMR_MREAD);
            hri_twi_write_IADR_reg(dev->hw, msg->addr & 0xff);
            } else {
            hri_twi_write_MMR_reg(dev->hw, TWI_MMR_DADR(msg->addr) | TWI_MMR_MREAD);
        }
        /* In single data byte master read, the START and STOP must both be set */
        hri_twi_write_CR_reg(dev->hw, TWI_CR_START | ((msg->len == 1) ? TWI_CR_STOP : 0));
        // Workaround: modified auto-generated code; next line moved to after the above statements. 20171012
        // not as much of a problem on reads as writes since rx buffer is probably empty
        hri_twi_set_IMR_reg(dev->hw, TWI_IER_RXRDY);
    } else {
    	// Line moved.....to below if....hri_twi_set_IMR_reg(dev->hw, TWI_IER_TXRDY | TWI_IER_TXCOMP);
        if (msg->addr & I2C_M_TEN) {
            hri_twi_write_MMR_reg(dev->hw, TWI_MMR_DADR(0x78 | (msg->addr >> 8)) | TWI_MMR_IADRSZ(1));
            hri_twi_write_IADR_reg(dev->hw, msg->addr & 0xff);
            } else {
            hri_twi_write_MMR_reg(dev->hw, TWI_MMR_DADR(msg->addr));
        }
        // Workaround: modified auto-generated code; next line moved to after the above IF statement. 20171012
        hri_twi_set_IMR_reg(dev->hw, TWI_IER_TXRDY | TWI_IER_TXCOMP);
    }
    return ERR_NONE;
}
#endif  // // leave here but compile out; was a workaround to fix a problem
