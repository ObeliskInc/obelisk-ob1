/**
 * \file TCA9546A_Defines.h
 *
 * \brief TWI/I2C bus 1:4 mux and isolator related functionality declaration.
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
 * Texas Instruments TCS9546A is an I2C bus based 1 to 4 switch/mux.  This provides four bidirectional
 * voltage-translating switched ports for I2C expansion and isolation.  On power-on/reset (POR) the
 * downstream ports are deselected.
 *
 * Device operation is defined in its datasheet: http://www.ti.com/lit/ds/symlink/tca9546a.pdf
 */

#ifndef _TCA9546A_DEFINES_H_INCLUDED
#define _TCA9546A_DEFINES_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>

// slave address is 7-bits; this is left shifted by 1 bit and R/W bit is inserted as the lsb.
#define TWI_SA_TCA9546A 0x70    // 7-bit slave address mask; SA[2:0] must match pin strapped values

/* I2C message format; bit order is msb to lsb
 * byte 1: slave address byte; value is: 1110 A[2:0] R/W#; where
 *         1110 is fixed 4-bit device code
 *         A[2:0] is 3-bit device address to match pin strapped inputs
 *         R/W# single bit indicates type of transfer
 * byte 2: swtich control byte; is 8-bit values; bits[3:0] are valid
 */

/**
 * \brief TCA9546A register names and offsets
 * Device operation is simple.  A single control register contains a 4-bit mask that enables
 * the downstream switched I2C bus port. Set bit 0 to enable port 0, bit 1 to enable port 1, etc.
 * The enumerated values convert directly to the bit masks.
 */
typedef enum {
    E_TWI_PORT_DISABLE = 0x00,
    E_TWI_PORT_ENB1    = 0x01,
    E_TWI_PORT_ENB2    = 0x02,
    E_TWI_PORT_ENB3    = 0x04,
    E_TWI_PORT_ENB4    = 0x08
} E_TWI_SWITCH_PORTS_T;


/**@}*/
#endif /* ifndef _TCA9546A_DEFINES_H_INCLUDED */
