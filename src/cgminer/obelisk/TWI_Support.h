/**
 * \file TWI_Support.h
 *
 * \brief I2C/TWI Support driver related declares
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
 */

#ifndef _TWI_SUPPORT_H_INCLUDED
#define _TWI_SUPPORT_H_INCLUDED

#define NUM_TWI_PORTS 1                // up to 7 possible on ATSAMG55 FLEXCOMS
#define TWI_CALLBACK_TIMEOUT 100000    // arbitrary big num just so we don't hang

// #define ENABLE_TWI_TOKENS 1      // used if there are multiple state machines sharing a TWI bus

#define TWI_PORT0_NUM    0   // handle (just a number) of the TWI port; we only use 1 TWI port here; numbered 0 to n
// #define TWI_PORT1_NUM    1

/***   GLOBAL DATA DECLARATIONS     ***/
// I2C general call commonly used values on many devices
#define TWI_GENERAL_CALL_SA 0x00
#define TWI_GENCALL_RESET   0x06

typedef enum  {
    E_TWI_WRITE = 0x00,
    E_TWI_READ  = 0x01,     // simple read of n bytes
    E_TWI_READ_REG  = 0x02  // read a register (uses repeated start)
} E_TWI_XFER_MODE_T;

// Enumerated values to select the TWI switch port on the controller board
typedef enum  {
    E_I2C_CONTROLLER    = 0x00, // stays on controller board
    E_I2C_HASH1         = 0x01,
    E_I2C_HASH2         = 0x02,
    E_I2C_HASH3         = 0x03,
    E_I2C_UNUSED_PORT   = 0x04
} E_TWI_XFER_PORT_T;

// Struct to fill in to transfer data (write/read) on TWI to the device
typedef struct {
    E_TWI_XFER_PORT_T ePort;    // For multiple TWI port support; hijacked here for hash board
    E_TWI_XFER_MODE_T eXferMode;
    uint8_t  uiAddr;     // slave address; 7-bit (to be left shifted and add in R/W bit)
    uint8_t  uiReg;      // sub-register address to access (e.g. in repeated start read)
    uint8_t  uiCount;    // how many to transfer
    uint8_t  *puiaData;  // ptr to data container for transfer
} S_TWI_TRANSFER_T;

/***   GLOBAL FUNCTION PROTOTYPES   ***/
#ifdef ENABLE_TWI_TOKENS
    extern int iTWITokenClaim(uint8_t uiTWIPort);
    extern int iTWITokenRelease(uint8_t uiTWIPort);
#endif  // #ifdef ENABLE_TWI_TOKENS

extern bool IsTWIValid(uint8_t uiTWIPort);
extern int iTWIInitXFer(S_TWI_TRANSFER_T *psOutBuf);
extern int iSetTWISlaveAdr(uint8_t uiTWIPort, uint16_t uiSlaveTgt);
extern int iTWIWriteBuf(uint8_t uiTWIPort, uint8_t *puiValues, uint8_t iCount, bool bWait);
extern int iTWIReadReg(uint8_t uiTWIPort, uint8_t uiReg, uint8_t *puiBuf, uint8_t iCount, bool bWait);
extern int iTWIReadBuf(uint8_t uiTWIPort, uint8_t *puiBuf, uint8_t iCount, bool bWait);
extern int iTWIPortInit(uint8_t uiTWIPort);
extern void CycleTWCK(uint8_t uiTWIPort);

extern void ClearCallbackFlags(uint8_t uiTWIPort);
extern int iWaitForTWITXCallback(uint8_t uiTWIPort, bool bWait);
extern int iWaitForTWIRXCallback(uint8_t uiTWIPort, bool bWait);


#endif /* ifndef _TWI_SUPPORT_H_INCLUDED */
