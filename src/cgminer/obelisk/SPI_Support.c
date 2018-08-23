/**
 * \file SPI_Support.c
 *
 * \brief SPI Support driver related methods for the application.
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
 * Adds custom SPI functions needed for the device. Each SPI bus can support multiple devices.
 * The code is custom to the design and not generic for general use. It allows a
 * mix of unique and general purpose functions.  SPI transport uses master asynchronous transfers
 * using interrupt driven control and callback.
 *
 * \section Module History
 * - 20171006 Rev 0.1 Started; using AS7 (build 7.0.1417) under START/ASF4
 */

/***    INCLUDE FILES       ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <unistd.h>

#include "Usermain.h"
#include "Ob1Hashboard.h"
#include "spi.h"
#include "SPI_Support.h"
#include "gpio_bsp.h"
#include "Console.h"
#include "MiscSupport.h"

/*
    // Initialization/declarations for ATSAMG55
#include "atmel_start.h"
#include "atmel_start_pins.h"
#include "ApplicationFiles\Usermain.h"
#include "ApplicationFiles\BSP\HashBoard\Ob1Hashboard.h"
#include "SPI_Support.h"
*/
/***    LOCAL STRUCTURES/ENUMS      ***/

/***    LOCAL FUNCTION PROTOTYPES   ***/
//static void cb_SPI5XferComplete(const struct spi_m_async_descriptor *const io_descr);
static bool bSPI5WriteOne(uint8_t uiValue, bool bLastWrite);
//static uint32_t uiReadSpiStatus(struct _spi_m_async_dev *dev);
//static bool bIsSPITxBusy(struct _spi_m_async_dev *dev);
//static bool bHBGetSpiSelects(uint8_t uiBoard);


/***    LOCAL DATA DECLARATIONS     ***/
static struct io_descriptor *pio_SPI5;    // descriptor for io write and io read methods associated with SPI
static volatile bool bSPI5_CBOccurred = false;    // SPI transfer callback occurred flag

/***    LOCAL DEFINITIONS     ***/
#define STARTUP_SPI_MSG "-I- SPI Initialized\r\n"

/***    LOCAL STRUCTURES/ENUMS      ***/

/***    LOCAL FUNCTION PROTOTYPES   ***/

/***    LOCAL DATA DECLARATIONS     ***/

/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/

/** *************************************************************
 *  \brief SPI Start Data Xfer process for the hash board
 *  \param enum eSPIXferType defines SPI xfer type
 *  \param pucaTxBuf; ptr to unsigned character array to write; can be null if read only
 *  \param pucaRxBuf; ptr to unsigned character array to read; can be null if write only
 *  \param uiLength; number to transfer
 *  \return bool error status is false if no error; true if error
 *
 *  This driver initiates a data transfer from an output buffer and into an input buffer.
 *  To support the CSS ASIC, SPI transfers are done by simultaneous writes and reads. This is because for reads we must
 *  write the first 3 bytes as the mode and address information.  For write only, we could simply use io_write but it would
 *  callback before the final byte is finished clocking which would deassert the slave select prematurely or would have to wait
 *  until the transmit is completed. So, it is easier just to do a full-duplex data transfer and ignore the input data. The read
 *  and write buffers should be separate and sizes should be the same. For reading, the write buffer's non-address fields should
 *  be 0's or 1's to minimize system noise.
 */
bool bSPI5StartDataXfer(E_SPI_XFER_TYPE eSPIXferType, uint8_t const *pucaTxBuf, uint8_t *const pucaRxBuf, const uint16_t uiLength)
{
    bool bError = false;
        
    transfer(fileSPI, pucaTxBuf, pucaRxBuf, uiLength);
    
    // Only sleep for reads
    if (eSPIXferType == E_SPI_XFER_READ) {
        usleep(10);
    }

    return(bError); // return true if error; else false
}  // end of bSPI5StartDataXfer()

/** *************************************************************
 *  \brief Support function to wait for an active SPI transfer process to complete.
 *  Here this is checking that none of the hash boards have its SPI slave selects asserted low.
 *  \param bool bWait if true then wait until idle; if false, just return the status
 *  \return int as 0 if SPI is idle; 1 if busy; less than 0 if error (e.g.timeout)
 */
int iIsHBSpiBusy(bool bWait)
{
    #define SC1_WAIT_TIMEOUT  100000    // 100ms for 1us delay primitive below

    int  iRetval = 0;
    uint32_t  uiTimeOut;

    // Uses the SPI slave select as indication of active transfer. This works on the test SPI board
    // since we only have one slave select.  On the real controller/hash boards we will need to check
    // multiple slave selects or use a semaphore flag to do the same.
    if (true == bWait) {
        uiTimeOut = SC1_WAIT_TIMEOUT;   // wait until idle or timeout
        while (false == bHBGetSpiSelects(MAX_NUMBER_OF_HASH_BOARDS)) {
            delay_us(1);
            //usleep(1);
            if (0 == --uiTimeOut) {
                iRetval = -1;   // timeout
                break;  // while
            }
        }  // while
    } else {
        if (false == bHBGetSpiSelects(MAX_NUMBER_OF_HASH_BOARDS)) {  // not waiting; just return the status
            iRetval = 1;    // busy
        }
    }  // if (true == bWait)

    return(iRetval);
}  // iIsHBSpiBusy

/** *************************************************************
 * \brief Function to control the SPI port mux control lines.  Like the SPI bus, these
 * are common to all the hash boards. This should be done before running a SPI communications transfer.
 * It should not be done if the SPI transfer is active or it will corrupt the transfer.
 * \param E_SC1_SPISEL_T eSPIMUX defines how to set the SPI mux
 * \return none
 * Side Effects: this is twiddling the SPI mux control signals and assumes that there is no SPI transfers happening.
 */
void HBSetSpiMux(E_SC1_SPISEL_T eSPIMUX)
{
    static E_SC1_SPISEL_T eSPIMUXMemory = E_SPI_INVALID;

    if (eSPIMUXMemory != eSPIMUX) { // change if needed
        // prevent changes while there are is an active SPI slave selects
        if (0 == iIsHBSpiBusy(false)) {
            switch (eSPIMUX) {
                case E_SPI_RESET:
                    gpio_set_pin_level(SPI_ADDR0,false);  // Generate reset on CS
                    gpio_set_pin_level(SPI_ADDR1,false);
                    break;
                case E_SPI_ASIC:
                    gpio_set_pin_level(SPI_ADDR0,true);  // target the ASICs
                    gpio_set_pin_level(SPI_ADDR1,false);
                    break;
                case E_SPI_GPIO:
                    gpio_set_pin_level(SPI_ADDR0,false);  // target the GPIO expanders
                    gpio_set_pin_level(SPI_ADDR1,true);
                    break;
                case E_SPI_INVALID:     // can target nothing; then a SPI cs will have no effect
                    gpio_set_pin_level(SPI_ADDR0,true);   // target nothing
                    gpio_set_pin_level(SPI_ADDR1,true);
                    break;
                default:
                    break;
            }
            eSPIMUXMemory = eSPIMUX;    // remember for next time
        }

    } else if (E_SPI_INVALID == eSPIMUXMemory) {
        gpio_set_pin_level(SPI_ADDR0,true);   // target nothing
        gpio_set_pin_level(SPI_ADDR1,true);
    }

} // HBSetSpiMux()

void HBSetSpiSelectsTrue(uint8_t uiBoard)
{
	switch(uiBoard) {
		case 0:
			gpio_set_pin_level(SPI_SS1, true);
			break;
		case 1:
			gpio_set_pin_level(SPI_SS2, true);
			break;
		case 2:
			gpio_set_pin_level(SPI_SS3, true);
			break;
		default:
			break;
	}
}

void HBSetSpiSelectsFalse(uint8_t uiBoard)
{
	switch(uiBoard) {
		case 0:
			gpio_set_pin_level(SPI_SS1, false);
			break;
		case 1:
			gpio_set_pin_level(SPI_SS2, false);
			break;
		case 2:
			gpio_set_pin_level(SPI_SS3, false);
			break;
		default:
			break;
	}
}

/** *************************************************************
 * \brief Function to control the SPI slave select control lines.  These are distinct
 * for the boards. This should be done before running a SPI communications transfer and AFTER
 * setting the SPISEL[1:0] signals that set the SPI mux function.
 * \param uint8_t uiBoard sets which board to target
 * \param bool bState is false to assert (low) the slave select and true to deassert
 * \return none
 * Side Effects: this is twiddling the SPI slave select lines and assumes that there is no SPI transfers happening.
 * Written for the SAMG55 eval board. In the real hardware this will be replaced.
 *
 *  Note: Due to limitations of Atmel START/ASF4, the SS line must be directly controlled instead of using auto-assert.
 *  This is likely to be like the intended hash board controller anyway since there will be multiple slave select lines
 *  to support simultaneous writing to multiple boards.
 *
 * For the ATSAMG55, using master asynchronous type transfers, the callback will deassert the slave select
 * at the completion of the transfer, presumably, by calling this function.
 */
void HBSetSpiSelects(uint8_t uiBoard, bool bState)
{

    uint8_t uiUUT;
    int ixI;
    int iStart, iEnd;

    if ( (0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard) ) {       // do one or all?
        iStart = uiBoard; // individual board
        iEnd   = uiBoard;
    } else {
        iStart = 0; // all boards
        iEnd   = LAST_HASH_BOARD;
    }  // if ( (0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard) )

    for (ixI = iStart; ixI <= iEnd; ixI++) {
        uiUUT = (uint8_t)ixI;
        switch(uiUUT) {
            case 0:
                gpio_set_pin_level(SPI_SS1, bState);
                break;
            case 1: // board 1 cs
                // < #TODO - SAMG55 doesn't have this >
                gpio_set_pin_level(SPI_SS2, bState);
                break;
            case 2: // board 2 cs
                // < #TODO - SAMG55 doesn't have this >
                gpio_set_pin_level(SPI_SS3, bState);
                break;
            default:
                break;
        } // switch

    }   // for

} // HBSetSpiSelects()

/** *************************************************************
 * \brief Function to read back the SPI slave select control lines.  These are distinct
 * for the boards. This can be done to determine if the SPI bus is actively transferring data.
 * The iIsHBSpiBusy() function should be used publicly instead of this.
 * \param uint8_t uiBoard sets which board to target; MAX_NUMBER_OF_HASH_BOARDS or more tests all
 * \return false if any of the slave selects is active asserted (low)
 * Written for the SAMG55 eval board. In the real hardware this will be replaced.
 *
 */
bool bHBGetSpiSelects(uint8_t uiBoard)
{
    uint8_t uiUUT;
    int ixI;
    int iStart, iEnd;
    bool bResult = true;    // assume that no selects are asserted

    if ( (0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard) ) {       // do one or all?
        iStart = uiBoard; // individual board
        iEnd   = uiBoard;
        } else {
        iStart = 0; // all boards
        iEnd   = LAST_HASH_BOARD;
    }  // if ( (0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard) )

    for (ixI = iStart; ixI <= iEnd; ixI++) {
        uiUUT = (uint8_t)ixI;
        switch(uiUUT) {
            case 0:
                //if (false == gpio_get_pin_level(SPI5_SS1)) {
                if (false == gpio_get_pin_level(SPI_SS1)) {
                    bResult = false;
                }
                break;
            case 1: // board 1 cs
                // < #TODO - SOLVED - SAMG55 doesn't have this >
                
                if (false == gpio_get_pin_level(SPI_SS2)) {
                    bResult = false;
                }
                
                //bResult = false;
                break;
            case 2: // board 2 cs
                // < #TODO - SOLVED - SAMG55 doesn't have this >
                
                if (false == gpio_get_pin_level(SPI_SS3)) {
                    bResult = false;
                }
                
                //bResult = false;
                break;
            default:
                break;
        }

    }   // for

    return(bResult);
} // bHBGetSpiSelects()


/** *************************************************************
 *  \brief Implement a recovery for SPI timeouts; maybe report to console.
 */
void SPITimeOutError(void)
{
    // Note: slave selects will probably be messed up if the callback didn't occur.
    HBSetSpiSelects(MAX_NUMBER_OF_HASH_BOARDS,true);  // < implement a recovery function if this happens >
}

