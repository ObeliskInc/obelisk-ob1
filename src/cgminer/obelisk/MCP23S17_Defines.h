/**
 * \file MCP23S17_Defines.h
 *
 * \brief SPI bus GPIO expander related functionality declaration.
 * This is mostly hardware related defines for device driver use.
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
 * Microchip MCP23S17 is a SPI bus based 16-bit I/O port expander.  This provides two 8-bit I/O ports
 * with many configuration options. Two interrupts can be generated based on changes to port values or
 * comparison (mismatch) with default states.  The two interrupts can be combined as a single interrupt
 * with active low, high or open-drain type.
 *
 * Device operations is defined in its datasheet and also Microchip AN1043.
 * Note: chip rev A silicon has errata related to addressing when IOCON.HAEN=0; A2 is not ignored.
 */

#ifndef _MCP23S17_DEFINES_H_INCLUDED
#define _MCP23S17_DEFINES_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>

/* SPI message format; bit order is msb to lsb
 * byte 1: control byte; value is: 0100 A[2:0] R/W#; where
 *         0100 is fixed 4-bit device code
 *         A[2:0] is 3-bit device address; enabled if IOCON.HAEN=1
 *         R/W# single bit indicates type of transfer
 * byte 2: register byte; is 8-bit register address
 * byte n: data payload; port A before port B for 2-byte data (assuming IOCON.SEQOP = 0)
 */

/**
 * \brief MCP23S17 SPI register names and offsets
 *
 * Each device has the registers defined below.  There are 22 individual registers that can
 * be considered as 11 register pairs. These are mapped in two different ways; The IOCON register
 * BANK bit controls the method used.  Register addresses are 8-bit field in the second byte of message.
 * IOCON.BANK=0 method is used for pseudo 16-bit; generally preferred to treat as a 16-bit device
 * IOCON.BANK=1 is for 8-bit access; treats as dual 8-bit devices
 * With sequential address mode enabled, a two byte write/read with BANK=0 will act like a 16-bit access
 */
typedef enum {
    // IOCON.BANK = 0 mapping; 16-bit sequential addressing
    E_S17_REG0_IODIRA   = 0x00,     // I/O direction register; RW default 0xFF
    E_S17_REG0_IODIRB   = 0x01,     // 1 = input; 0 = output

    E_S17_REG0_IPOLA    = 0x02,     // Input polarity register; RW default = 0
    E_S17_REG0_IPOLB    = 0x03,     // 0 = normal; 1 = inverted (0 input is read as high)

    E_S17_REG0_GPINTENA = 0x04,     // interrupt on-change enable; RW 0x00 (disabled)
    E_S17_REG0_GPINTENB = 0x05,     // 0 = pin not part of INT output; 1 = pin generates INT

    E_S17_REG0_DEFVALA  = 0x06,     // Default compare register for interupt on change; RW default = 0
    E_S17_REG0_DEFVALB  = 0x07,     // INT generated if input pin does not match depending on enables

    E_S17_REG0_INTCONA  = 0x08,     // Interrupt on change control; RW default = 0
    E_S17_REG0_INTCONB  = 0x09,     // 0=compare to prev pin state; 1=pin compared to DEFVAL mismatch

    E_S17_REG0_IOCONA   = 0x0A,     // Device configuration register; RW default = 0
    E_S17_REG0_IOCONB   = 0x0B,     // various configurations for different bits

    E_S17_REG0_GPPUA    = 0x0C,     // Weak (100K) pull-up resistor on inputs; RW default = 0
    E_S17_REG0_GPPUB    = 0x0D,     // 0 = no pull-up; 1 = pull-up enabled (inputs only)

    E_S17_REG0_INTFA    = 0x0E,     // Interrupt flag register; RO
    E_S17_REG0_INTFB    = 0x0F,     // 0 = pin not causing INT; 1 = pin causing INT

    E_S17_REG0_INTCAPA  = 0x10,     // Interrupt capture value; RO
    E_S17_REG0_INTCAPB  = 0x11,     // Bit reflects state of pin at the INT assertion point

    E_S17_REG0_GPIOA    = 0x12,     // Pin value on port; RW
    E_S17_REG0_GPIOB    = 0x13,     // Writing sets output latch; reading returns value at IO pad

    E_S17_REG0_OLATA    = 0x14,     // Output latch register; RW
    E_S17_REG0_OLATB    = 0x15,     // Write to change an output; read to return latch value (not pin value)

/*  commented out since we won't be using these
    // IOCON.BANK = 1 mapping; 8-bit banked addressing
    E_S17_REG1_IODIRA   = 0x00,
    E_S17_REG1_IODIRB   = 0x10,
    E_S17_REG1_IPOLA    = 0x01,
    E_S17_REG1_IPOLB    = 0x11,
    E_S17_REG1_GPINTENA = 0x02,
    E_S17_REG1_GPINTENB = 0x12,
    E_S17_REG1_DEFVALA  = 0x03,
    E_S17_REG1_DEFVALB  = 0x13,
    E_S17_REG1_INTCONA  = 0x04,
    E_S17_REG1_INTCONB  = 0x14,
    E_S17_REG1_IOCONA   = 0x05,
    E_S17_REG1_IOCONB   = 0x15,
    E_S17_REG1_GPPUA    = 0x06,
    E_S17_REG1_GPPUB    = 0x16,
    E_S17_REG1_INTFA    = 0x07,
    E_S17_REG1_INTFB    = 0x17,
    E_S17_REG1_INTCAPA  = 0x08,
    E_S17_REG1_INTCAPB  = 0x18,
    E_S17_REG1_GPIOA    = 0x09,
    E_S17_REG1_GPIOB    = 0x19,
    E_S17_REG1_OLATA    = 0x0A,
    E_S17_REG1_OLATB    = 0x1A
*/

} E_MCP23S17_REG_T;

/**
 * \brief IOCON Register bit values; configures the interrupt output behavior
 * b0 unused; set to 0
 * b1 INTPOL; INT pin output polarity; 0/1=active low/high
 * b2 ODR; 1=open drain output; overrides INTPOL bit
 * b3 HAEN; hardware address enable; 1=use address pins; 0=ignore address pins
 * b4 DISSLW; (I2C device slew rate limit on SDA for FAST mode) 1=disable; 0=enable
 * b5 SEQOP; 0=enable sequential operations; 1=disable sequential operations
 * b6 MIRROR; 0=int outputs are separate; 1= int output are connected
 * b7 BANK; 0=registers are sequential 16-bit; 1=registers are banked 8-bit addressed
 */
typedef enum {
    E_IOCON_INTPOL    = 0x02,
    E_IOCON_ODR       = 0x04,
    E_IOCON_HAEN      = 0x08,
    E_IOCON_DISSLW    = 0x10,
    E_IOCON_SEQOP     = 0x20,
    E_IOCON_MIRROR    = 0x40,
    E_IOCON_BANK      = 0x80
} E_IOCON_bits_T;


/**@}*/
#endif /* ifndef _MCP23S17_DEFINES_H_INCLUDED */
