/**
 * \file MCP23S17_hal.c
 *
 * \brief MCP23S17 firmware device driver; application specific
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
 * Low level device drivers for the MCP23S17 using SPI interface.  Implemented for custom
 * configuration of the SC1 hashing board rather than as a generic device driver.
 *
 *
 * \section Module History
 * - 20180515 Rev 0.1 Started; using AS7 (build 7.0.1417)
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
#include "Ob1Hashboard.h"
#include "SPI_Support.h"
#include "MCP23S17_Defines.h"
#include "MCP23S17_hal.h"
#include "MiscSupport.h"

/***    LOCAL DEFINITIONS     ***/
#define DEV_INIT_DEV_MESSAGE "-I- MCP23S17 Init: "

#define DEV_READ_BUFF_SIZE 2 // max bytes size we expect to have to transfer to the device (plus a little more)
#define DEV_WRITE_BUFF_SIZE DEV_READ_BUFF_SIZE

/* Configuration Notes; for DONE and NONCE use
 * Port Expander Usage:
 *  Port A[7:0] = DONE/NONCE[7:0]; inputs only
 *  Port B[6:0] = DONE/NONCE[14:8]; inputs only
 *  Port B7 = reserved; input for future cascading; not part of INT output
 * I/O Configuration
 * 1) IOCON.BANK = 0; for 16-bit access
 * 2) IOCON.MIRROR = 1; Int out are mirrored
 * 4) IOCON.SEQOP = 0; Enable sequential operation
 * 3) IOCON.HWEN = 1; Enable HW address
 * ..also interrupt output low and push-pull drive
 */
// Using 16-bit defines since we treat as 16-bit device; DONE and NONCE are set the same
#define DONE_IOCON_DEFAULT (0x4848)
#define DONE_GPINTENB_DEFAULT (0x7FFF) // b15 not used; b[14:0] used for interrupts by default
#define DONE_IODIR_DEFAULT (0xFFFF) // all inputs; (POR default so we don't actually have to program this)
#define DONE_INTCON_DEFAULT (0xFFFF) // interrupt control; compare to DEFVAL register
#define DONE_DEFVAL_DEFAULT (0x0) // ASIC assert DONE/NONCE as active high; so INT if these don't match this

/* Configuration Notes; for Misc use
 * Port Expander Usage:
 *  Port A0 PSENB; output; set high to enable voltage to string (has ext pull-down)
 *  Port A1 HASHOFF; output; set low to enable ASIC oscillators (START_CLK low)
 *  Port A[7:2] not connected; configured as inputs with weak pull-ups
 *  Port B[3:0] reserved; configured as inputs with weak pull-ups
 *  Port B[7:4] Board revision; inputs with internal pullups; rev 0 is all high)
 */
#define PSENB (0x01) // b0 high to turn on DC-DC converter to power ASIC string
#define HASHOFF (0x02) // b1 high to stop hashing clocks; 0 to enable hashing clocks
#define CTL_MASK (HASHOFF | PSENB) // pmask so we only write to the bits we control

#define REV_MASK (0x0F00) // port B[3:0] have revision strapping
#define VARIANT_MASK (0x7000) // port B[6:4] reserved for variant strapping (TBD)
#define SC1_DCR1_MASK (0x8000) // port B[7] has SC1/DCR1 variant strapping; (1 = SC1; 0 = DCR1)
// Revs are interpreted so that minimal resistors are used to effect a revision code but still
// have a sensible decoding scheme. Simple method is to invert the bits and convert to a number.
// Bit value    revision
//  0x0F        prerelease
//  0x0E        A
//  0x0D        B
//  0x0C        C
//  etc....
#define CONVERT_REV(x) ((((~(x)) & REV_MASK) >> 8) & 0x0F)

/*
 * Port Expander Configuration
 * 1) IOCON.BANK = 0; for 16-bit access
 * 2) IOCON.MIRROR = 1; Int out are mirrored
 * 3) Enable HW address
 */
// Using 16-bit defines since we treat as 16-bit device
#define MISC_IOCON_DEFAULT (0x4848) // E_IOCON_MIRROR with E_IOCON_HAEN
#define MISC_GPINTENB_DEFAULT (0x0) // interrupts not used
#define MISC_IODIR_DEFAULT (0xFFFC) // D[1:0] output; others are inputs
#define MISC_GPPU_DEFAULT (0xFFFC) // pull-ups on all inputs; none on outputs
#define MISC_OLAT_DEFAULT (HASHOFF) // Hashing clock off (high); PSENB off (low)

// SPI address (7-bit); R/W# bit is lsb in control byte so these get shifted left 1 bit when sent
#define PEX_DONE_ADR (0x20) // ASIC Done signals
#define PEX_NONCE_ADR (0x21) // ASIC Nonce signals
#define PEX_MISC_ADR (0x22) // PS enable, Hash enable, rev ID, etc

/***    LOCAL STRUCTURES/ENUMS      ***/

/***    LOCAL FUNCTION PROTOTYPES   ***/
static int iPexSpiTransfer(S_MCP23S17_TRANSFER_T* psXferBuf);

/***    LOCAL DATA DECLARATIONS     ***/

// Note: assuming that char is same size as uint8_t here; else should convert these char buffers into uint8_t
// static struct io_descriptor *pio_SC1SPI;    // descriptor for io write and io read methods associated with SPI

static S_MCP23S17_TRANSFER_T sPexXferBuf; // Local buffer for SPI transfer to/from device

static uint8_t ucaSPIOutBuf[DEV_WRITE_BUFF_SIZE]; // buffer for sending out
static uint8_t ucaSPIInBuf[DEV_READ_BUFF_SIZE]; // buffer for reading in

/***    GLOBAL DATA DECLARATIONS   ***/
/***********************************/
/***                             ***/
/***     THE CODE BEGINS HERE    ***/
/***                             ***/
/***********************************/
/** *************************************************************
 * \brief Toggle HASHENB signal; test function to test set and get; this is not expected to
 * be used in actual software.
 * \param uiBoard sets which board to target; can only target one board at a time
 * \return int error codes from err_codes.h
 */
int iToggleHashClockEnable(uint8_t uiBoard)
{
    int iRetval = ERR_NONE;
    bool bValue;

    // Get the present value
    iRetval = iGetHashClockEnable(uiBoard, &bValue);

    if (ERR_NONE == iRetval) {
        // Set to the opposite value
        if (false == bValue) {
            iRetval = iSetHashClockEnable(uiBoard, true);
        } else {
            iRetval = iSetHashClockEnable(uiBoard, false);
        }
    }
    return (iRetval);

} // iToggleHashClockEnable()

/** *************************************************************
 * \brief Set HASHENB signal; set true to enable the clocking of the ASICS
 * Setting true will start clocking by enabling the oscillators.  This sets the STARTCLK
 * signal of the ASIC low.
 * \param uiBoard sets which board to target; can only target one board at a time
 * \param bEnable set true to enable clocking
 * \return int error codes from err_codes.h
 */
int iSetHashClockEnable(uint8_t uiBoard, bool bEnable)
{
    int iRetval = ERR_NONE;
    uint16_t uiValue;

    iRetval = iIsBoardValid(uiBoard, false);
    if (ERR_NONE == iRetval) {
        // get port status so we can change only the signal we want
        iRetval = iReadPexPins(uiBoard, PEX_MISC_ADR, &uiValue);
        uiValue = uiValue & CTL_MASK;
    }
    if (ERR_NONE == iRetval) {
        if (false == bEnable) {
            uiValue |= HASHOFF; // set HASHOFF bit to turn hashing off
        } else {
            uiValue &= ~HASHOFF; // clear HASHOFF bit to turn hashing on
        }
        iRetval = iWritePexPins(uiBoard, PEX_MISC_ADR, uiValue);
    }
    return (iRetval);

} // iSetHashClockEnable()

/** *************************************************************
 * \brief Get HASHENB signal; reads the port status to get the present state of
 * asic clock enable. True indicates clocking is enabled (STARTCLK is low).
 * \param uiBoard sets which board to target; can only target one board at a time
 * \param pbEnable is ptr to bool to return; true if hashing enabled
 * \return int error codes from err_codes.h
 */
int iGetHashClockEnable(uint8_t uiBoard, bool* pbEnable)
{
    int iRetval = ERR_NONE;
    uint16_t uiValue;

    iRetval = iIsBoardValid(uiBoard, false);
    if (ERR_NONE == iRetval) {
        // get port status so we can change only the signal we want
        iRetval = iReadPexPins(uiBoard, PEX_MISC_ADR, &uiValue);
    }

    if (HASHOFF == (uiValue & HASHOFF)) {
        *pbEnable = false;
    } else {
        *pbEnable = true;
    }
    return (iRetval);

} // iGetHashClockEnable()

/** *************************************************************
 * \brief Toggle power supply enable signal; test function to test set and get; this is not expected to
 * be used in actual software. Can only target one board at a time.
 * \param uiBoard sets which board to target; range 0 to MAX_NUMBER_OF_HASH_BOARDS-1
 * \return int error codes from err_codes.h
 */
int iTogglePSEnable(uint8_t uiBoard)
{
    int iRetval = ERR_NONE;
    bool bValue;

    // Get the present value
    iRetval = iGetPSEnable(uiBoard, &bValue);

    if (ERR_NONE == iRetval) {
        if (false == bValue) { // Set to the opposite value
            iRetval = iSetPSEnable(uiBoard, true);
        } else {
            iRetval = iSetPSEnable(uiBoard, false);
        }
    }
    return (iRetval);

} // iTogglePSEnable()

/** *************************************************************
 * \brief Set Power Supply enable signal; set true to enable the power to the ASIC string
 *  by setting the PS Enable signal high. Can only target one board at a time.
 * \param uiBoard sets which board to target; range 0 to MAX_NUMBER_OF_HASH_BOARDS-1
 * \param bEnable set true to enable power to asic string
 * \return int error codes from err_codes.h
 */
int iSetPSEnable(uint8_t uiBoard, bool bEnable)
{
    int iRetval = ERR_NONE;
    uint16_t uiValue;

    iRetval = iIsBoardValid(uiBoard, false);
    if (ERR_NONE == iRetval) {
        // get port status so we can change only the signal we want
        iRetval = iReadPexPins(uiBoard, PEX_MISC_ADR, &uiValue);
        uiValue = uiValue & CTL_MASK;
    }

    if (ERR_NONE == iRetval) {
        if (true == bEnable) {
            uiValue |= PSENB; // set PSENB bit to turn power supply on
        } else {
            uiValue &= ~PSENB; // clear PSENB bit to turn power supply off
        }
        iRetval = iWritePexPins(uiBoard, PEX_MISC_ADR, uiValue);
        delay_ms(400); // delay; allows write to complete and power supply to stabilize
    }
    return (iRetval);

} // iSetPSEnable()

/** *************************************************************
 * \brief Get Power supply enable signal; reads the port status to get the present state of
 * asic power supply. True indicates power supply is enabled (PSENB is high).
 * \param uiBoard sets which board to target; can only target one board at a time
 * \param pbEnable is ptr to bool to return; true if power is enabled
 * \return int error codes from err_codes.h
 */
int iGetPSEnable(uint8_t uiBoard, bool* pbEnable)
{
    int iRetval = ERR_NONE;
    uint16_t uiValue;

    iRetval = iIsBoardValid(uiBoard, false);
    if (ERR_NONE == iRetval) {
        // get port status so we can change only the signal we want
        iRetval = iReadPexPins(uiBoard, PEX_MISC_ADR, &uiValue);
    }

    if (PSENB == (uiValue & PSENB)) {
        *pbEnable = true;
    } else {
        *pbEnable = false;
    }
    return (iRetval);

} // iGetPSEnable()

/** *************************************************************
 * \brief MCP23S17 Port Expander Device Initialize Method
 * \param uiBoard sets which board to target; can only target one board at a time
 * \return int error codes from err_codes.h
 * \retval ERR_NONE if successful
 * \retval other error codes from err_codes.h
 *
 *  Called once at startup or on demand. Resets and programs the device(s) for a custom configuration used
 *  by this application. Assumes the associated SPI port is initialized and available.
 *  This will initialize all three gpio expanders as it is unlikely we need to do these separately.
 * 1) Set IOCON; default is to ignore address; means all devices will be responding to same data on first write
 *    until the IOCON.HAEN=1; so we must enable addressing first
 * 2) Configure IO
 *    - IODIR (0x00): all inputs (default)
 *    - IPOL (0x01): normal input polarity (default)
 *    - GPINTEN (0x02): enable IOC on inputs; (A port = 0xFF; B port = 0x7F)
 *    - DEFVAL (0x03): all low (i.e. generate interrupt if input is high)
 *    - INTCON (0x04): all high (i.e. compare to DEFVAL register)
 *    - GPPU (0x06): disable internal pull-ups (default)
 *
 */
int iPexInit(uint8_t uiBoard)
{
    int iRetval = ERR_NONE;
    //     bool bError = false;

    iRetval = iIsBoardValid(uiBoard, false);
    if (ERR_NONE == iRetval) {
        sPexXferBuf.uiBoard = uiBoard;

        // On POR the MCP23S17 defaults to ignore the address bits; so if we talk to one chip we talk to all of them on the same board.
        // So, first thing is to set IOCON with HWEN=1 so they will only respond to their address.
        // OTOH, we additionally send explicitly to all devices in-case this is a software init that does not follow a POR event.
        sPexXferBuf.uiPexChip = PEX_DONE_ADR;
        sPexXferBuf.eXferMode = E_GPIO_XP_WRITE2;
        sPexXferBuf.uiReg = E_S17_REG0_IOCONA;
        sPexXferBuf.uiData = DONE_IOCON_DEFAULT;

        iRetval = iPexSpiTransfer(&sPexXferBuf); // should do all on POR or only DONE after that
        if (ERR_NONE == iRetval) {
            sPexXferBuf.uiPexChip = PEX_NONCE_ADR;
            iRetval = iPexSpiTransfer(&sPexXferBuf); // should do the NONCE input port; (same as DONE port)
        }
        if (ERR_NONE == iRetval) {
            sPexXferBuf.uiPexChip = PEX_MISC_ADR; // misc expander has the PSENB and HASHENB lines
            iRetval = iPexSpiTransfer(&sPexXferBuf);
        }

        if (ERR_NONE == iRetval) {
            // Program the PSENB and HASHENB values for output BEFORE we enable the port outputs
            sPexXferBuf.uiPexChip = PEX_MISC_ADR;
            sPexXferBuf.eXferMode = E_GPIO_XP_WRITE1; // 1 byte to port A
            sPexXferBuf.uiReg = E_S17_REG0_OLATA;
            sPexXferBuf.uiData = MISC_OLAT_DEFAULT;
            iRetval = iPexSpiTransfer(&sPexXferBuf);
        }

        if (ERR_NONE == iRetval) {
            // Configure the pin directions (enable output pins)
            sPexXferBuf.uiPexChip = PEX_MISC_ADR;
            sPexXferBuf.eXferMode = E_GPIO_XP_WRITE1; // 1 byte to port A
            sPexXferBuf.uiReg = E_S17_REG0_IODIRA;
            sPexXferBuf.uiData = MISC_IODIR_DEFAULT;
            iRetval = iPexSpiTransfer(&sPexXferBuf);
        }

        if (ERR_NONE == iRetval) {
            // Configure the pin pull-ups (inputs only)
            sPexXferBuf.uiPexChip = PEX_MISC_ADR;
            sPexXferBuf.eXferMode = E_GPIO_XP_WRITE2; // 2 bytes; for both ports
            sPexXferBuf.uiReg = E_S17_REG0_GPPUA;
            sPexXferBuf.uiData = MISC_GPPU_DEFAULT;
            iRetval = iPexSpiTransfer(&sPexXferBuf);
        }

        // DONE and NONCE ports are by default configured as inputs without pull-ups.
        // These have external pull-ups, so we don't have to change the defaults.
        // The interrupt on change output needs to be configured to combine the desired inputs.
        // This uses the default compare register value of 0 so it does not need to be programmed.
        if (ERR_NONE == iRetval) { // Configure what is used for combined DONE output
            sPexXferBuf.uiPexChip = PEX_DONE_ADR;
            sPexXferBuf.eXferMode = E_GPIO_XP_WRITE2; // 2 byte; to port A and B
            sPexXferBuf.uiReg = E_S17_REG0_GPINTENA;
            sPexXferBuf.uiData = DONE_GPINTENB_DEFAULT;
            iRetval = iPexSpiTransfer(&sPexXferBuf);
        }
        if (ERR_NONE == iRetval) { // Configure to compare with default value reg
            sPexXferBuf.uiPexChip = PEX_DONE_ADR;
            sPexXferBuf.eXferMode = E_GPIO_XP_WRITE2; // 2 byte; to port A and B
            sPexXferBuf.uiReg = E_S17_REG0_INTCONA;
            sPexXferBuf.uiData = DONE_INTCON_DEFAULT;
            iRetval = iPexSpiTransfer(&sPexXferBuf);
        }

        if (ERR_NONE == iRetval) { // Configure what is used for combined NONCE output
            sPexXferBuf.uiPexChip = PEX_NONCE_ADR;
            sPexXferBuf.eXferMode = E_GPIO_XP_WRITE2; // 2 byte; to port A and B
            sPexXferBuf.uiReg = E_S17_REG0_GPINTENA;
            sPexXferBuf.uiData = DONE_GPINTENB_DEFAULT;
            iRetval = iPexSpiTransfer(&sPexXferBuf);
        }
        if (ERR_NONE == iRetval) { // Configure to compare with default value reg
            sPexXferBuf.uiPexChip = PEX_NONCE_ADR;
            sPexXferBuf.eXferMode = E_GPIO_XP_WRITE2; // 2 byte; to port A and B
            sPexXferBuf.uiReg = E_S17_REG0_INTCONA;
            sPexXferBuf.uiData = DONE_INTCON_DEFAULT;
            iRetval = iPexSpiTransfer(&sPexXferBuf);
        }

    } // if (ERR_NONE == iRetval)
    return (iRetval);
} // iPexInit()

/** *************************************************************
 * \brief Reads and interprets the board revision/option pins from the misc port expander.
 * The value returned does not reflect the actual pin values but its converted value.
 * \param uiBoard sets which board to target
 * \param puiRevision ptr to uint8_t to return revision code; 0 if error
 * \param puiType ptr to return type of hash board code
 * \return int error codes from err_codes.h
 */
int iReadBoardRevision(uint8_t uiBoard, uint8_t* puiRevision, E_ASIC_TYPE_T* puiType)
{
    int iRetVal;
    uint16_t uiValue;

    iRetVal = iReadPexPins(uiBoard, PEX_MISC_ADR, &uiValue);
    if (ERR_NONE == iRetVal) {
        *puiRevision = (uint8_t)CONVERT_REV(uiValue); // convert to a number
        if (SC1_DCR1_MASK == (uiValue & SC1_DCR1_MASK)) {
            *puiType = E_TYPE_SC1;
        } else {
            *puiType = E_TYPE_DCR1;
        }
    } else {
        *puiRevision = 0;
    }
    return (iRetVal);

} // iReadBoardRevision()

/** *************************************************************
 * \brief Read the Done status of the ASICs for one of the boards.  Returned
 * value reflects which ASICs on the board are signaling they are done (bit is 1)
 * \param uiBoard sets which board to target
 * \param puiValue ptr to uint16_t to return values read back
 * \return int error codes from err_codes.h
 */
int iReadBoardDoneInts(uint8_t uiBoard, uint16_t* puiValue)
{
    int iRetVal = ERR_NONE;

    iRetVal = iReadPexPins(uiBoard, PEX_DONE_ADR, puiValue);
    return (iRetVal);

} // iReadBoardDoneInts()

/** *************************************************************
 * \brief Read the Nonce status of the ASICs for one of the boards.  Returned
 * value reflects which ASICs on the board are signaling they have a Nonce (bit is 1)
 * \param uiBoard sets which board to target
 * \param puiValue ptr to uint16_t to return values read back
 * \return int error codes from err_codes.h
 */
int iReadBoardNonceInts(uint8_t uiBoard, uint16_t* puiValue)
{
    int iRetVal = ERR_NONE;

    iRetVal = iReadPexPins(uiBoard, PEX_NONCE_ADR, puiValue);
    return (iRetVal);

} // iReadBoardNonceInts()

/** *************************************************************
 * \brief Read the port expander GPIO input levels. Returns 16-bit value as pin states.
 * \param uiBoard sets which board to target; can only be one board at a time for reads
 * \param uiChip defines which chip to read
 * \param puiValue ptr to uint16_t to return values read back
 * \return int error codes from err_codes.h
 */
int iReadPexPins(uint8_t uiBoard, uint8_t uiChip, uint16_t* puiValue)
{
    int iRetVal;

    iRetVal = iIsBoardValid(uiBoard, false); // non-strict check
    if (ERR_NONE == iRetVal) {
        sPexXferBuf.uiBoard = uiBoard;
        sPexXferBuf.eXferMode = E_GPIO_XP_READ2;
        sPexXferBuf.uiPexChip = uiChip;
        sPexXferBuf.uiReg = E_S17_REG0_GPIOA;
        iRetVal = iPexSpiTransfer(&sPexXferBuf); // Read to the chip; should block until completed
    }
    if (ERR_NONE != iRetVal) {
        sPexXferBuf.uiData = 0; // zero data if error returned
    }
    *puiValue = sPexXferBuf.uiData;
    return (iRetVal);

} // iReadPexPins()

/** *************************************************************
 * \brief Write the port expander GPIO output latches with 16-bit value to output.
 *  Assumes pin directions are already configured.  Written as generic for any chip although
 *  in this design we only have one port expander with outputs.
 * \param uiBoard sets which board to target; can only target one board at a time
 * \param uiChip defines which chip to write
 * \param uiValue uint16_t is value to set
 * \return int error codes from err_codes.h
 */
int iWritePexPins(uint8_t uiBoard, uint8_t uiChip, uint16_t uiValue)
{
    int iRetVal = ERR_NONE;

    iRetVal = iIsBoardValid(uiBoard, false);
    if (ERR_NONE == iRetVal) {
        sPexXferBuf.uiBoard = uiBoard;
        sPexXferBuf.eXferMode = E_GPIO_XP_WRITE2;
        sPexXferBuf.uiPexChip = uiChip;
        sPexXferBuf.uiReg = E_S17_REG0_OLATA; // output latch reg
        sPexXferBuf.uiData = uiValue;
        iRetVal = iPexSpiTransfer(&sPexXferBuf); // Write to the chip
    }
    return (iRetVal);

} // iWritePexPins()

/** *************************************************************
 *  \brief Port Expander Device SPI Transfer Method; use for generic read/write access to the device.
 *  Converts the structure pointed to by psXferBuf to a byte buffer and then calls the SPI transfer.
 *  Does a (blocking) check in SPI status to prevent changing the output buffer of an on-going transfer.
 *  Will return error if timeout occurs.  For reads, this will wait for the data.  For writes, this will
 *  start the transfer but not wait. However, the structure can be preconfigured with the next transfer since
 *  it has already been used to construct the SPI buffer.
 *  If caller doesn't want to block while waiting on the SPI bus then it should call iIsSC1SpiBusy(false)
 *  prior to calling this to ensure SPI bus is available and ready to go.  If multiple tasks use this
 *  resource then arbitration and/or mutex methods should be used.
 *
 *  \param psXferBuf is ptr to struct with address and data value to write/read into
 *  \return int error codes from err_codes.h
 *  \retval ERR_NONE Completed successfully.
 *  \retval ERR_BUSY SPI comms busy or timeout; try again later
 *  \retval ERR_INVALID_DATA invalid parameter?
 */
static int iPexSpiTransfer(S_MCP23S17_TRANSFER_T* psXferBuf)
{
#define PEX_READ_BIT 0x01
#define PEX_WRITE_BIT 0x00
#define PEX_COUNT_ONE_BYTE 3 // 2 bytes adr plus 1 of data
#define PEX_COUNT_TWO_BYTE 4 // 2 bytes adr plus 2 of data

    int iRetval = ERR_NONE;
    uint8_t uiRWnot = PEX_WRITE_BIT; // lsb sets read (1) or write (0) mode of device
    uint8_t uiCount = PEX_COUNT_TWO_BYTE;

    // Do some basic checks on the input; not testing everything
    if (NULL == psXferBuf) {
        iRetval = ERR_INVALID_DATA;
    } else {
        // Check that requested chip adr is in the expected range
        if ((PEX_DONE_ADR <= psXferBuf->uiPexChip) && (PEX_MISC_ADR >= psXferBuf->uiPexChip)) {
            switch (psXferBuf->eXferMode) {
            case E_GPIO_XP_WRITE1:
                uiCount = PEX_COUNT_ONE_BYTE;
                uiRWnot = PEX_WRITE_BIT;
                break;
            case E_GPIO_XP_WRITE2:
                uiCount = PEX_COUNT_TWO_BYTE;
                uiRWnot = PEX_WRITE_BIT;
                break;
            case E_GPIO_XP_READ1:
                uiCount = PEX_COUNT_ONE_BYTE;
                uiRWnot = PEX_READ_BIT;
                break;
            case E_GPIO_XP_READ2:
                uiCount = PEX_COUNT_TWO_BYTE;
                uiRWnot = PEX_READ_BIT;
                break;
            default:
                iRetval = ERR_INVALID_DATA;
            } // end switch
        } else {
            iRetval = ERR_INVALID_DATA;
        }
    } // if (NULL == psXferBuf)

    if (ERR_NONE == iRetval) {
        // Verify the SPI port is idle and ready to go; if not we wait to allow an active, interrupt driven transfer
        // to complete.  We don't want to corrupt the transfer buffer or change the SPI mux during an active transfer.
        if (0 != iIsHBSpiBusy(true)) {
            iRetval = ERR_BUSY; // got a timeout?
            SPITimeOutError();
        }
    } // if (ERR_NONE == iRetVal)

    if (ERR_NONE == iRetval) {
        // Build the buffer to transfer....
        ucaSPIOutBuf[0] = (uint8_t)((psXferBuf->uiPexChip << 1) | uiRWnot); // Set up the mode-address in bytes [2:0]; big-endian order
        ucaSPIOutBuf[1] = (uint8_t)(psXferBuf->uiReg);
        // Data is 8 or 16 bits; here we assume 16 and transfer counter will take care of length
        ucaSPIOutBuf[2] = (uint8_t)(psXferBuf->uiData & 0xFF);
        ucaSPIOutBuf[3] = (uint8_t)((psXferBuf->uiData >> 8) & 0xFF);

        HBSetSpiMux(E_SPI_GPIO);
        HBSetSpiSelects(psXferBuf->uiBoard, false);

        if (PEX_READ_BIT == uiRWnot) {
            (void)bSPI5StartDataXfer(E_SPI_XFER8, ucaSPIOutBuf, ucaSPIInBuf, uiCount); // reading 8 bits per word
            HBSetSpiSelects(psXferBuf->uiBoard, true);
            // wait for the read transfer to complete; then the result should be in input buffer
            if (0 != iIsHBSpiBusy(true)) { // verify SPI is no longer busy
                iRetval = ERR_BUSY; // got a timeout?
                SPITimeOutError();
            } else {
                if (PEX_COUNT_ONE_BYTE == uiCount) {
                    psXferBuf->uiData = (uint16_t)ucaSPIInBuf[2];
                } else {
                    psXferBuf->uiData = (uint16_t)(ucaSPIInBuf[2]) | ((uint16_t)(ucaSPIInBuf[3]) << 8); // assemble input to 16 bit data
                } // if (PEX_COUNT_ONE_BYTE == uiCount)
            } // if (0 != iIsSC1SpiBusy(true))
        } else { // write transfer; start the transfer but don't wait
            // If we want to monitor then we can poll the callback flag or wait for SPI busy
            (void)bSPI5StartDataXfer(E_SPI_XFER_WRITE, ucaSPIOutBuf, ucaSPIInBuf, uiCount);
            HBSetSpiSelects(psXferBuf->uiBoard, true);
        } // if (PEX_READ_BIT == uiRWnot)

    } // if (ERR_NONE == iRetVal)

    return (iRetval);
} // iPexSpiTransfer
