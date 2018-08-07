/**
 * \file MCP24AA02_Defines.h
 *
 * \brief I2C bus EEPROM and Unique ID chip
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
 * \section Description
 * Microchip 24AA02UIDT-I/OT is a 2K bit EEPROM with a 32-bit unique ID preprogrammed.  The UID is its
 * primary use as it provides a way to serialize the boards and aid in future customer support.
 * Memory is organized as two blocks of 128 x 8 bit memory.  Additional EEPROM space may be used in the
 * future for other non-volatile information storage.
 *
 * Device operation is defined in its datasheet: http://ww1.microchip.com/downloads/en/DeviceDoc/20005202A.pdf
 */

#ifndef _MCP24AA02_DEFINES_H_INCLUDED
#define _MCP24AA02_DEFINES_H_INCLUDED

/* I2C message format summary; bit order is msb to lsb
 * For writes:
 * byte 1: control byte; value is: 1010XXX R/W#; where
 *         1010 is fixed 4-bit device code
 *         XXX is 3-bits of don't care state; set here for 0's
 *         R/W# single bit indicates type of transfer; 0 for write
 * byte 2: address pointer; is 8-bit address
 * byte n: data payload; 1 (byte write) to 8 bytes (page write)
 *         For page writes, the address pointer must respect page boundaries
 *         Payload is not sent if simply setting the address pointer for a
 *         following read.
 *
 * For reads:
 * byte 1: control byte; value is: 1010XXX R/W#; where
 *         1010 is fixed 4-bit device code
 *         XXX is 3-bits of don't care state; set here for 0's
 *         R/W# single bit indicates type of transfer; 1 for read
 * byte n: data payload; 1 (byte read) to n bytes (sequential read)
 *         Data is read from the current address pointer which is then
 *         auto-incremented. Use the write operation to set the address
 *         pointer.
 *
 * For writes, the address pointer can range from 0x00 to 0x7F
 * Address 0x80 to 0xFF are read-only.
 */

// slave address is 7-bits; this is left shifted by 1 bit and R/W bit is inserted as the lsb.
#define TWI_SA_MCP24AA02 0x50    // 7-bit slave address mask


/**@}*/
#endif /* ifndef _MCP24AA02_DEFINES_H_INCLUDED */
