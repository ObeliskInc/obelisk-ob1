/**
 * \file MCP23S17_hal.h
 *
 * \brief SPI bus GPIO expander firmware related declaration.

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
 * \section Description
 * On the SC1 design we will use the device primarily as a 16:1 interrupt port to consolidate signals from the
 * multiple ASICs into single outputs to the controller.  This allows up to 15 ASICs per device plus one remaining
 * pin as a cascade input.
 *
 * Application specific configuration notes:
 * Accessed via SPI bus; set SPI clock rate at 5MHz or slower; use 8-bit transfers
 * SC1 uses three devices at different addresses
 * Adr = 0; DONE[15:1]
 * Adr = 1; NONE[15:1]
 * Adr = 2; miscellaneous I/O
 *
 * For DONE and NONCE
 *  - Ints are mirrored; active low (not open drain)
 *  - All pins are inputs
 *  - INTn int-on-change configured for mismatch to default value
 *  - Host MCU will poll the INTn pins as multiple GPIO to check if any ASICs have NONCE or DONE.
 *    If asserted, the MCU can then read the device GPIO inputs to determine which ASICs are requesting
 *    service.  If MCU uses interrupt driven process, then interrupts should be level triggered.
 *    The interrupt will clear only if ASIC signal is cleared and the expander GPIO reg is read.
 */

#ifndef _MCP23S17_HAL_H_INCLUDED
#define _MCP23S17_HAL_H_INCLUDED

//TODO: Remove this, Could not find E_ASIC_TYPE_T without it
#include "Ob1Hashboard.h"
#include "MCP23S17_Defines.h"

/***  ASSOCIATED STRUCTURES/ENUMS      ***/
/**
 * \brief application related enumerated values for gpio expander
 * Allows to read/write one or two bytes for 8-bit or 16-bit access
 */
typedef enum  {
    E_GPIO_XP_READ1  = 0x0,     // read 1 byte from device
    E_GPIO_XP_READ2  = 0x1,     // read 2 bytes from device; port A then port B
    E_GPIO_XP_WRITE1 = 0x2,     // write 1 byte to device
    E_GPIO_XP_WRITE2 = 0x3      // write 2 bytes to device; port A then port B
} E_MCP23S17_XFER_MODE_T;

// Struct to fill in to transfer data (write/read) on SPI to the device
typedef struct {
    uint8_t  uiBoard;    // hash board [0-3]; 0 for all (write only); 1-3 for individual boards
    uint8_t  uiPexChip;     // chip address [0-2]; ignored if multicast (IOCON.HWEN = 0)
    E_MCP23S17_XFER_MODE_T eXferMode;
    uint8_t  uiReg;      // register address to access
    uint16_t uiData;     // data container for transfer (port A then port B)
} S_MCP23S17_TRANSFER_T;


/***   GLOBAL DATA DECLARATIONS     ***/
// Task pending globals; for ISR callbacks, etc

/***   GLOBAL FUNCTION PROTOTYPES   ***/
extern int iPexInit(uint8_t uiBoard);
extern int iReadBoardRevision(uint8_t uiBoard, uint8_t *puiRevision, E_ASIC_TYPE_T *puiType);
extern int iReadBoardDoneInts(uint8_t uiBoard, uint16_t *puiValue);
extern int iReadBoardNonceInts(uint8_t uiBoard, uint16_t *puiValue);
extern int iReadPexPins(uint8_t uiBoard, uint8_t uiChip, uint16_t *puiValue);
extern int iWritePexPins(uint8_t uiBoard, uint8_t uiChip, uint16_t uiValue);
extern int iSetHashClockEnable(uint8_t uiBoard, bool bEnable);
extern int iGetHashClockEnable(uint8_t uiBoard, bool *pbEnable);
extern int iToggleHashClockEnable(uint8_t uiBoard);
extern int iSetPSEnable(uint8_t uiBoard, bool bEnable);
extern int iGetPSEnable(uint8_t uiBoard, bool *pbEnable);
extern int iTogglePSEnable(uint8_t uiBoard);

#endif /* ifndef _MCP23S17_HAL_H_INCLUDED */
