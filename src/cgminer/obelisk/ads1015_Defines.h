/**
 * \file ADS1015_Defines.h
 *
 * \brief ADS1015 firmware related declaration.
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
 * ADS1015 is a 4-channel, 12-bit, delta-sigma type ADC with internal reference, oscillator,
 * PGA and I2C interface.  Conversion rates of up to 3.3KSPS are possible (single channel).
 * Data is read as 12-bit value but left-justified in a 16-bit (2 byte) readback.
 *
 * I2C message format; bit order is msb to lsb
 * For I2C master WRITE: at minimum 3 bytes are required; this sets the register address pointer.
 * byte 1: control byte; value is: 1001xxx R/W#; where
 *         10010xx is fixed 7-bit device code; xx is determined by ADDR pin connection at reset
 *         R/W# single bit indicates type of transfer
 * byte 2: register MSB; is first part of 16-bit register pointer value
 * byte 3: register LSB; is second part of 16-bit register pointer value
 * byte n: optional data payload
 *
 * For I2C master READ: at minimum 3 bytes are required; this does NOT set the register address pointer.
 * The previously set register pointer is used to read the desired data.
 * byte 1: control byte; value is: 1001xxx R/W#; where
 *         10010xx is fixed 7-bit device code; xx is determined by ADDR pin connection at reset
 *         R/W# single bit indicates type of transfer
 * byte n: optional data payload; MSB first, LSB second
 */

#ifndef _ADS1015_DEFINES_H_INCLUDED
#define _ADS1015_DEFINES_H_INCLUDED

/***         INCLUDE FILES         ***/
//#include <compiler.h>
//#include <utils.h>

/***       MACRO DEFINITIONS       ***/
#ifndef _HW_REG_ALIAS_INCLUDED
#define _HW_REG_ALIAS_INCLUDED
#define   _inreg     volatile const       //!< Defines 'read only' permissions
#define   _outreg    volatile             //!< Defines 'write only' permissions
#define   _ioreg     volatile             //!< Defines 'read / write' permissions
#endif   // _HW_REG_ALIAS_INCLUDED

// slave address is 7-bits; this is left shifted by 1 bit and R/W bit is inserted as the lsb.
// Device can be strapped to have 4 different addresses; only of below will be active.
#define ADS1015_TWI_SA_GND 0x48    // ADDR = GND
#define ADS1015_TWI_SA_VDD 0x49    // ADDR = VDD
#define ADS1015_TWI_SA_SDA 0x4A    // ADDR = SDA
#define ADS1015_TWI_SA_SCL 0x4B    // ADDR = SCL

#define ADS1015_TWI_SA7B   ADS1015_TWI_SA_GND    // define the active 7-bit slave address mask
#define ADS1015_GENERAL_CALL_ADDR   0x00   // not used; could be used for reset

// Register listing: see data sheet table 4 for pointer register descriptions
#define ADS1015_REG_CONVERSION      0x00   // RO result of last conversion
#define ADS1015_REG_CONFIG          0x01   // RW write mode; read status
#define ADS1015_REG_LO_THRESH       0x02   // RW comparator; not used
#define ADS1015_REG_HI_THRESH       0x03   // RW comparator; not used

#define ADS1015_RESET               TWI_GENCALL_RESET

// Configuration register bit fields: values defined so they can be ORed to build a control word
// see data sheet table 6 Config Register (Read/Write); reset/default value is 0x8583
#define UNUSED_COMP_QUEUE_1         0x0000  // -----|
#define UNUSED_COMP_QUEUE_2         0x0001  //      |--- Low two bits are for the comparator.
#define UNUSED_COMP_QUEUE_4         0x0002  //      |--- This uses COMP_DISABLE.
#define COMP_DISABLE                0x0003  // -----|
#define UNUSED_COMP_LATCH           0x0004
#define UNUSED_COMP_POLARITY        0x0008
#define UNUSED_COMP_MODE            0x0010
#define UNUSED_DATA_RATE_128        0x0000  // -----|
#define UNUSED_DATA_RATE_250        0x0020  //      |
#define UNUSED_DATA_RATE_490        0x0040  //      |--- Bits[7:5]
#define UNUSED_DATA_RATE_920        0x0060  //      |--- Data rate selection (SPS)
#define UNUSED_DATA_RATE_1600       0x0080  //      |--- This uses the fastest rate
#define UNUSED_DATA_RATE_2400       0x00A0  //      |    (but single-shot)
#define UNUSED_DATA_RATE_3300       0x00C0  //      |
#define DATA_RATE_3300              0x00E0  // -----|
#define UNUSED_MODE_CONTINUOUS      0x0000
#define MODE_SINGLE_SHOT            0x0100  // Uses single-shot capture then power-down
#define UNUSED_PGA_6V144            0x0000
#define PGA_4V096                   0x0200  // Uses PGA set to +/-4.096V full scale range
#define UNUSED_PGA_2V048            0x0400
#define UNUSED_PGA_1V024            0x0600
#define UNUSED_PGA_0V512            0x0800
#define UNUSED_PGA_0V256            0x0A00
#define UNUSED_MUX_INPUT_0          0x0000  // -----|
#define UNUSED_MUX_INPUT_1          0x1000  //      |--- Input selection takes 3 bits.  MSBit is
#define UNUSED_MUX_INPUT_2          0x2000  //      |--- always set to indicate single-ended operation
#define UNUSED_MUX_INPUT_3          0x3000  // -----|
#define CONFIG_MUX_BITS             0x7000  // bits[14:12], sets input mux/channel
#define UNUSED_DIFFERENTIAL_INPUT   0x0000
#define SINGLE_ENDED                0x4000  // bit14=1 sets single-ended (AINref= 0V)
#define BEGIN_CONVERSION            0x8000  // for write: only valid when in power down mode
#define OP_STATUS                   0x8000  // for read; 0=idle, 1=conversion in process



// Data tags for devices analog mux channels used for the getter functions
// WARNING! These enum values are used to access arrays. DO NOT CHANGE THEM CASUALLY.
typedef enum {
    E_FIRST_ADS1015_CHAN = 0,                // 0
    E_ASIC_V15IO         = E_FIRST_ADS1015_CHAN,  // VDDIO at top of string
    E_ASIC_V1            = 1,                // 1 ASIC 1 core voltage
    E_ASIC_V15           = 2,                // 2 Top of string voltage (DC-DC output)
    E_PS12V              = 3,                // 3 Input power supply voltage
    E_NUM_ADS1015_CHANS  = 4,                // we have 4 analog channels
    E_LAST_ADS1015_CHAN  = E_NUM_ADS1015_CHANS-1
} E_ADS1115_CHANNELS;

// Resistive divider (60.4K and 20K); provides about 0.249 of input
// As this is about divide by 4 we can use binary math to get close enough for calculations.
// For example, either can multiply by 4 or add four of the most recent values.
// Chan 0, 2 and 3 use the divider to bring 12V (approx) input to less than 3V
// Chan 1 is expected to be 0.5 to 0.7V and does not use a divider.
#define AIN_DIVIDER  (20/(20+60.4))    // 0.248756 or (1/4.02)
#define AIN_MULTIPLIER_AS_INT  (4)

// ADC is bipolar to support differential type measurements.  Therefore, for ground-referred,
// single-ended measurements, it should only report outputs in the range of about 0 to 0x7FF0.
// For small (near-ground) signals, this may report negative voltages in the presence of noise.
// For +/-4.096V range, 0V = 0x0 and 4.096V = 0x7FF0. However, since the ADC supply is 3.3V in
// this application, it should only report up to
//

#endif /* ifndef _ADS1015_DEFINES_H_INCLUDED */
