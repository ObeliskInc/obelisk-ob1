/**
 * \file MCP9903_Defines.h
 *
 * \brief MCP9903 related functionality declaration.
 * This is mostly hardware related defines for device driver use.
 *
 * @section Legal
 * **Copyright Notice:**
 * Copyright (c) 2017 All rights reserved.
 *
 * **Proprietary Notice:**
 *       This document contains proprietary information and neither the document
 *       nor said proprietary information shall be published, reproduced, copied,
 *       disclosed or used for any purpose other than the consideration of this
 *       document without the expressed written permission of a duly authorized
 *       representative of said Company.
 */

#ifndef _MCP9903_DEFINES_H_INCLUDED
#define _MCP9903_DEFINES_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>

#ifndef _HW_REG_ALIAS_INCLUDED
#define _HW_REG_ALIAS_INCLUDED
#define   _inreg     volatile const       //!< Defines 'read only' permissions
#define   _outreg    volatile             //!< Defines 'write only' permissions
#define   _ioreg     volatile             //!< Defines 'read / write' permissions
#endif   // _HW_REG_ALIAS_INCLUDED

/**
 * \brief MCP9903 Description
 * The MCP9903 device is a three channel remote diode sensor for temperature measurements.  Provides two
 * external diode and one internal diode channels.  This uses the SMBus interface.  It includes advanced
 * features for resistance error correction and beta compensation.
 */

// Default TWI slave address (7-bit); this gets left shifted by 1 bit and R/W bit is inserted as the lsb.
// Using a 22K ohm resistor on /THERM so SMBus address is 0x3C
#define MCP990X_TWI_SA7B   0x1C

#define NUM_MCP9903_CHANS 3   // MCP9903 supports this many temperature sensors (1 internal)

// Register listing: see data sheet table 5-7 for register descriptions
// Summary: device appears as a series of 8-bit registers.  To read the registers, the TWI repeated
// start method is used to first send the register address and then read back the data.  This device does
// not support auto-incremented writes/reads so each register write/read must be individually done.
//
// To read temperature data; the high byte must be read before the low byte as reading the high byte causes the low
// byte to be frozen so it can be read later in time but still correspond with the high byte.  The low byte contains
// three bits only that define fractional degree readings.  If only 1 deg C precision is needed then the low byte can
// be ignored.  The high byte defines the temperature as an unsigned integer in 1 deg C increments (0 to 127).
#define MCP990X_REG_INTTHB          0x00    // Internal temperature high byte
#define MCP990X_REG_INTTLB          0x29    // Internal temperature low byte
#define MCP990X_REG_EX1THB          0x01    // External chn 1 temperature high byte
#define MCP990X_REG_EX1TLB          0x10    // External chn 1 temperature low byte
#define MCP990X_REG_EX2THB          0x23    // External chn 2 temperature high byte
#define MCP990X_REG_EX2TLB          0x24    // External chn 2 temperature low byte
#define MCP990X_REG_STATUS          0x02    // device status
#define MCP990X_REG_EXTFAULT        0x1B    // external fault status
#define MCP990X_REG_CONFIG1         0x03    // configuration
#define MCP990X_REG_CONVERT1        0x04    // conversion rate
#define MCP990X_REG_FILTER          0x40    // filter selections
#define MCP990X_REG_DEVID           0xFD    // device variant id
#define MCP990X_REG_MFGID           0xFE    // mfg identification; (0x5D expected)
#define MCP990X_REG_MFGREV          0xFF    // mfg revison; (0x00 expected)
#define UNUSED_MCP990X_REG_IHILIM   0x05    // Internal ch high limit
#define UNUSED_MCP990X_REG_ILOLIM   0x06    // Internal ch low limit
#define UNUSED_MCP990X_REG_E1HLHB   0x07    // external ch1 high limit high byte
#define UNUSED_MCP990X_REG_E1LLHB   0x08    // external ch1 low limit high byte

#define MCP990X_LOW_BYTE_MASK       0xE0    // b[7:5] of low byte regs have fractional temperature
#define MCP990X_FRAC_BIT_POS        5

// Device ID codes; returned when reading MCP9903_REG_DEVID
#define MCP9903_DEVID               0x21
#define UNUSED_MCP9902_DEVID        0x20
#define UNUSED_MCP9904_DEVID        0x25
#define MCP990X_MFGID               0x5D
#define UNUSED_MCP990X_MFGREV       0x00

// Status register bit fields
#define MCP990X_STA_BUSY            0x80    // 1 if internal ADC is converting
#define MCP990X_STA_IHIALT          0x40    // 1 if internal diode ch exceeds alert high limit
#define MCP990X_STA_ILOALT          0x20    // 1 if internal diode ch below alert low limit
#define MCP990X_STA_EHIALT          0x10    // 1 if external diode ch1 exceeds alert high limit
#define MCP990X_STA_ELOALT          0x08    // 1 if external diode ch1 below alert low limit
#define MCP990X_STA_FAULT           0x04    // 1 if open circuit or short of diode detected
#define MCP990X_STA_ETHRM           0x02    // 1 if external diode exceeds high limit
#define MCP990X_STA_ITHRM           0x01    // 1 if internal diode exceeds high limit

// External fault status register bit fields
#define MCP990X_EXTFAULT_EXT1       0x02    // 1 if diode open or short
#define MCP990X_EXTFAULT_EXT2       0x03    // 1 if diode open or short

// Config register bit fields: values defined so they can be ORed to build a control word
// see data sheet table 5-6 Config Register (Read/Write); reset/default value is 0x8583
#define MCP990X_CFG_MSKAL           0x80    // 1 to mask alert/therm output
#define MCP990X_CFG_STANDBY         0x40    // active/standby; 0 is active, 1 is standby
#define MCP990X_CFG_ALTMODE         0x20    // alert mode; 0 is interrupt, 1 is comparator
#define MCP990X_CFG_RECD1           0x10    // REC mode for ext diode 1
#define MCP990X_CFG_RECD2           0x08    // REC mode for ext diode 2
#define MCP990X_CFG_RANGE           0x04    // meas range; 0 is 0..127; 1 is extended range
#define MCP990X_CFG_DA_DIS          0x02    // dynamic averaging disable
#define UNUSED_MCP990X_CFG_APDD     0x01    // anti-parallel diode disable (MCP9904 only)

// Convert rate register bit fields:
// Support several conversion rates using a 4-bit field.  When active, the device does continuous conversions and
// compares the result to any programmed limits.  If enabled, output pins signal alerts based on the limits.
#define MCP990X_CVT_SLEEP           0x80    // 0 for cfg R/S bit control; 1 to sleep (overrides cfg R/S)
#define MCP990X_CVT_CONVRATE_MASK   0x0F
#define MCP990X_CVT_CONVRATE_4SPS   0x06    // default is 4 conversions per second

/**@}*/
#endif /* ifndef _MCP9903_DEFINES_H_INCLUDED */
