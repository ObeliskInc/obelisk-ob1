/**
 * \file CSS_SC1Defines.h
 *
 * \brief CSS ASIC SC1 related functionality declaration.
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
 * ASIC implements algorithms for Blake2B hashing computation. Device has 64 computation
 * engines or cores. Each engine/core has a number of registers to define inputs and several output
 * registers to read the results.  The registers are typically 64-bits wide but typically use the lower bits.
 *
 * Data I/O done through SPI (AC-coupled).  Each transfer (read/write) requires 11-bytes total. The
 * first 3-bytes (24-bits) constitute a header to define the mode, chip address, engine address and
 * register address. Remaining 8-bytes transfer 64-bits of data to write or read from the targeted address.
 * Due to AC-coupling, the SPI transfers have a limited time to complete to prevent the decay of the slave
 * select control signals.
 *
 * SPI transaction format; 11 bytes total; values transfered as big-endian (network) order
 * Mode (2 bits)
 * Chip Adr (7 bits)
 * Core Adr (8 bits)
 * Reg Adr (7 bits)
 * Data (64 bits)
 *
 * It is believed that all registers that can be written to, can also be read back. Rev A silicon
 * has readback errata.
 */

#ifndef _CSS_SC1_DEFINES_H_INCLUDED
#define _CSS_SC1_DEFINES_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>
#include "Ob1Hashboard.h"

#define MAX_NUMBER_OF_SC1_CORES 64 // each SC1 asic has 64 cores plus a virtual core address
#define LAST_SC1_CORE (MAX_NUMBER_OF_SC1_CORES - 1)
#define SC1_EPOT_BOOST_COUNT 10 // how much to increase string power after registers are configured

// Rev A CSS silicon needs a workaround for reading due to a race condition in the SPI communications for read.
// Regs 0-0x3F must shift data right 1 bit and cannot read regs from 0x40 and higher.  The race can be unpredictable
// so sometimes the fix will work and occasionally it does not.
// To be fixed in rev B and later, change this workaround to 0 or find and delete associated
// defines.  It isn't clear how we can determine silicon rev at run time so we might just abandon rev A parts which
// should not make it to the customer.
#define CSS_SC1_REVA_WORKAROUND 0 // set to 1 for Rev A silicon; cannot read certain regs and readback of others is shifted

#ifndef _HW_REG_ALIAS_INCLUDED
#define _HW_REG_ALIAS_INCLUDED
#define _inreg volatile const //!< Defines 'read only' permissions
#define _outreg volatile //!< Defines 'write only' permissions
#define _ioreg volatile //!< Defines 'read / write' permissions
#endif // _HW_REG_ALIAS_INCLUDED

/**
 * \brief CSS SC1 command mode enumerated values
 * Values will define a 2 bit mode field described in the data sheet
 */
typedef enum {
    E_SC1_MODE_REG_WRITE = 0x0, // write to reg; single target chip and core only
    E_SC1_MODE_REG_READ = 0x1, // read from reg; single target chip and core only
    E_SC1_MODE_CHIP_WRITE = 0x2, // write to reg; all cores on one targeted chip
    E_SC1_MODE_MULTICAST = 0x3 // multicast write to all chips and cores; same reg on all
} E_CSS_CMD_MODE_T;

/**
 * \brief CSS SC1 command defines
 */

// Mode and Address related bit shift defines; First 3 bytes of SPI transfer
// 31 ... 24 | 23 22 | 21 ... 15 | 14 ... 7 | 6 ... 0
//   unused  |  MODE | CHIP ADR  | CORE ADR | REG ADR   <== expressed as 32-bit word

// 31 .. 24 | 23 .. 16 | 15 .. 08 | 07 .. 00
// 00000000   MMCCCCCC   CEEEEEEE   ERRRRRRR        <== OR as bytes
#define SC1_ADR_REG_Pos 0
#define SC1_ADR_CORE_Pos 7
#define SC1_ADR_CHIP_Pos 15
#define SC1_ADR_MODE_Pos 22
#define SC1_ADR_REG_Bits 0x7F
#define SC1_ADR_CORE_Bits 0xFF
#define SC1_ADR_CHIP_Bits 0x7F
#define SC1_ADR_MODE_Bits 0x03
#define SC1_ADR_REG_Mask ((SC1_ADR_REG_Bits) << SC1_ADR_REG_Pos)
#define SC1_ADR_CORE_Mask ((SC1_ADR_CORE_Bits) << SC1_ADR_CORE_Pos)
#define SC1_ADR_CHIP_Mask ((SC1_ADR_CHIP_Bits) << SC1_ADR_CHIP_Pos)
#define SC1_ADR_MODE_Mask ((SC1_ADR_MODE_Bits) << SC1_ADR_MODE_Pos)

/**
 * \brief CSS SC1 chip level register names and offsets
 *
 * Each chip has the registers defined below.  These are accessed using core address of
 * 0x80 (i.e. the 7-bit register field of the SPI transaction)
 */
typedef enum {
    // Each chip has the special registers defined below in addition to the core registers.
    // These are accessed by writing to core adr 0x80 instead of in the range 0 to 0x3F.
    // Access to these is read only.
    // These provide a summary of the done/busy status and which core has priority.
    E_SC1_REG_EDR = 0x40, //!< chip level engine/core done status reg
    E_SC1_REG_EBR = 0x41, //!< chip level engine/core busy status reg
    E_SC1_REG_IVR = 0x42, //!< chip level interrupt vector status

    E_SC1_CHIP_REGS = 0x80
} E_SC1_CHIP_REG_T;

/**
 * \brief CSS SC1 EDR bit values; engine done register
 * E_SC1_REG_EDR : (Core: 0x80 Offset: 0x40) (R/W  64bits)
 * b[63:0] engine/core done bits
 */
/**
 * \brief CSS SC1 EBR bit values; engine busy register
 * E_SC1_REG_EBR : (Core: 0x80 Offset: 0x41) (R/W  64bits)
 * b[63:0] engine/core busy bits
 */

/**
 * \brief CSS SC1 IVR bit values; interrupt vector register
 * E_SC1_REG_IVR : (Core: 0x80 Offset: 0x42) (R/W  64bits)
 * b[7:0] interrupt vector
 * b[13:8] core ID
 * b[63:14] unused; write as 0's
 */
#define SC1_IVR_CORE_ID_POS 8
enum E_SC1_IVR_bits_T {
    E_SC1_IVR_INT_MASK = 0xFF,
    E_SC1_IVR_CORE_ID_MASK = 0x3F00
};

/**
 * \brief CSS SC1 engine/core register names and offsets
 *
 * Each core has the registers defined below.  The value define the offsets into each
 * core address (i.e. the 7-bit register field of the SPI transaction)
 * Reg 0x00 to 0x3F are typically written to (but maybe read back)
 * Reg 0x40 and higher are read only registers
 * The SC1 ASIC Engine/core address range is 0x00 to 0x3F (0 to 63 decimal) but has
 * reserved address space for 0 to 127 cores.
 */
typedef enum {
    // V-regs; 16-total; set once using mfg recommended values
    E_SC1_REG_V00 = 0x00,
    E_SC1_REG_V01 = 0x01,
    E_SC1_REG_V02 = 0x02,
    E_SC1_REG_V03 = 0x03,
    E_SC1_REG_V04 = 0x04,
    E_SC1_REG_V05 = 0x05,
    E_SC1_REG_V06 = 0x06,
    E_SC1_REG_V07 = 0x07,
    E_SC1_REG_V08 = 0x08,
    E_SC1_REG_V09 = 0x09,
    E_SC1_REG_V10 = 0x0A,
    E_SC1_REG_V11 = 0x0B,
    E_SC1_REG_V12 = 0x0C,
    E_SC1_REG_V13 = 0x0D,
    E_SC1_REG_V14 = 0x0E,
    E_SC1_REG_V15 = 0x0F,

    // M-regs; with LB and UB define the input parameters of problem
    E_SC1_NUM_MREGS = 10,
    E_SC1_REG_M0 = 0x10,
    E_SC1_REG_M1 = 0x11,
    E_SC1_REG_M2 = 0x12,
    E_SC1_REG_M3 = 0x13,
    E_SC1_REG_M4_RSV = 0x14, // reg M4 is reserved and not accessed on SC1
    E_SC1_REG_M5 = 0x15,
    E_SC1_REG_M6 = 0x16,
    E_SC1_REG_M7 = 0x17,
    E_SC1_REG_M8 = 0x18,
    E_SC1_REG_M9 = 0x19,

    E_SC1_REG_V0MATCH = 0x1A,
    E_SC1_REG_V8MATCH = 0x1B,

    E_SC1_REG_FCR = 0x1C, //!< Fifo control register
    E_SC1_REG_UB = 0x1D, //!< Upper bound
    E_SC1_REG_LB = 0x1E, //!< Lower bound
    E_SC1_REG_LIMITS = 0x1F,

    E_SC1_REG_ECR = 0x20, //!< engine/core control register
    E_SC1_REG_STEP = 0x21, //!< iteration step
    E_SC1_REG_OCR = 0x2F, //!< oscillator control register
    E_SC1_REG_TEST = 0x3F, //!< engine/core test register

    // Fifo data registers; not true FIFO, more like a register file
    E_SC1_REG_FDR0 = 0x40, //!< Fifo Data 0
    E_SC1_REG_FDR1 = 0x41, //!< Fifo Data 1
    E_SC1_REG_FDR2 = 0x42, //!< Fifo Data 2
    E_SC1_REG_FDR3 = 0x43, //!< Fifo Data 3
    E_SC1_REG_FDR4 = 0x44, //!< Fifo Data 4
    E_SC1_REG_FDR5 = 0x45, //!< Fifo Data 5
    E_SC1_REG_FDR6 = 0x46, //!< Fifo Data 6
    E_SC1_REG_FDR7 = 0x47, //!< Fifo Data 7
    E_SC1_REG_POR0 = 0x48, //!< pipeline output reg 0
    E_SC1_REG_POR1 = 0x49, //!< pipeline output reg 1
    E_SC1_REG_ICR = 0x4A, //!< interrupt chain reg
    E_SC1_REG_FSR = 0x4B, //!< Fifo status reg
    E_SC1_REG_TDR0 = 0x4C, //!< Test Data Reg 0
    E_SC1_REG_TDR1 = 0x4D, //!< Test Data Reg 1
    E_SC1_REG_ESR = 0x4E, //!< Engine/core status reg

    E_SC1_REG_LASTREG = E_SC1_REG_ESR
} E_SC1_CORE_REG_T;

/**
 * \brief CSS SC1 fixed register values
 * V0 Match, M8 Match, limits and the V00 to V15 registers are programmed after
 * reset/power-up with constant values for the design according to the data sheet.
 * Values were cut/pasted from CSS data sheet v1.9.6
 */

// related bit shift defines
#define SC1_LIMITS_VAL (0x0000000000000031)
#define SC1_STEP_VAL (0x00000000000003F1)
#define SC1_V00_VAL (0x6A09E667F2BDC928)
#define SC1_V01_VAL (0xBB67AE8584CAA73B)
#define SC1_V02_VAL (0x3C6EF372FE94F82B)
#define SC1_V03_VAL (0xA54FF53A5F1D36F1)
#define SC1_V04_VAL (0x510E527FADE682D1)
#define SC1_V05_VAL (0x9B05688C2B3E6C1F)
#define SC1_V06_VAL (0x1F83D9ABFB41BD6B)
#define SC1_V07_VAL (0x5BE0CD19137E2179)
#define SC1_V08_VAL (0x6A09E667F3BCC908)
#define SC1_V09_VAL SC1_V01_VAL
#define SC1_V10_VAL SC1_V02_VAL
#define SC1_V11_VAL SC1_V03_VAL
#define SC1_V12_VAL (0x510E527FADE68281)
#define SC1_V13_VAL SC1_V05_VAL
#define SC1_V14_VAL (0xE07C265404BE4294)
#define SC1_V15_VAL SC1_V07_VAL

#define SC1_V0_MATCH_VAL SC1_V00_VAL
#define SC1_V8_MATCH_VAL (0x000000FFFFFFFFFF)

/**
 * \brief CSS SC1 FCR bit values; fifo control register
 * E_SC1_REG_FCR : (Offset: 0x1C) (R/W  64bits)
 * b[7:0] Fifo data register mask; 0 to enable, 1 to mask Nonce Int
 * b[12:8] strobe control (test purposes); write as 0's
 * b[63:13] unused; write as 0's
 */
enum E_SC1_FCR_bits_T {
    E_SC1_FCR_MASK_D0 = 0x01,
    E_SC1_FCR_MASK_D1 = 0x02,
    E_SC1_FCR_MASK_D2 = 0x04,
    E_SC1_FCR_MASK_D3 = 0x08,
    E_SC1_FCR_MASK_D4 = 0x10,
    E_SC1_FCR_MASK_D5 = 0x20,
    E_SC1_FCR_MASK_D6 = 0x40,
    E_SC1_FCR_MASK_D7 = 0x80,
    E_SC1_FCR_MASK_ALL = 0xFF
};

/**
 * \brief CSS SC1 FSR bit values; fifo status register
 * E_SC1_REG_FSR : (Offset: 0x4B) (R/W  64bits)
 * b[7:0] Fifo data valid mask; 0 empty, 1 has data
 * b[15:8] core interrupt vector
 * b[63:16] unused; write as 0's
 */
enum E_SC1_FSR_bits_T {
    E_SC1_FSR_FIFO_MASK = 0xFF,
    E_SC1_FSR_CORE_ID_MASK = 0xFF00
};

/**
 * \brief CSS SC1 ECR bit values; engine control register
 * E_SC1_REG_ECR : (Offset: 0x20) (R/W  64bits)
 * b0 reset engine spi; restarts SPI FSM
 * b1 read complete acknowledge; ends computation
 * b2 valid data ready; starts computation
 * b3 engine reset; clears NONCE FIFO, DONE, etc
 * b4 strobe status
 * b5 strobe test data
 * b6 enable man strobe
 * b7 enable man chip status
 * b8 power save enable
 * b[63:9] unused; write as 0's
 * Function of b1 and b2 are described in data sheet section 3.3.
 * b[8:4] functions are not well described (assumed for test purposes)
 */
enum E_SC1_ECR_bits_T {
    E_SC1_ECR_RESET_SPI_FSM = 0x01,
    E_SC1_ECR_READ_COMPLETE = 0x02,
    E_SC1_ECR_VALID_DATA = 0x04,
    E_SC1_ECR_RESET_CORE = 0x08
};

/**
 * \brief CSS SC1 OCR bit values; oscillator control register
 * E_SC1_REG_OCR : (Offset: 0x2F) (R / W  64bits)
 * Note: the oscillator controls are grouped in banks of 16
 * Cores[15:0] are controlled by Core at offset 0x07
 * Cores[31:16] are controlled by Core at offset 0x17
 * Cores[47:32] are controlled by Core at offset 0x27
 * Cores[63:48] are controlled by Core at offset 0x37
 * All cores can be updated globally by using the global write
 *
 * b0 bias control; 0 - VCO control lowers frequency; 1 - VCO control increases frequency
 * b1 oscillator divider reset; test function
 * b2 external clock enable; test mode
 * b3 div by 8; core clock is osc clock divided by 8
 * b4 div by 4; core clock is osc clock divided by 4
 * b5 div by 2; core clock is osc clock divided by 2
 * b6 div by 1; core clock is osc clock divided by 1
 * b[22:7] engine clock enable mask; 0 to disable, 1 to enable
 * b[27:23] VCO control settings c[5:1]; lowers VCO for b0=0
 * b[32:28] VCO control settings c[10:6]; increases VCO for b0=0
 * b33 enable test clock out; puts out oscillator group 4 clock/(64*divider)
 * b[63:34] unused; write as 0's
 * Note: for b3 to b6 divider bit; set one and only one at a time as per datasheet.
 */
enum E_SC1_OCR_bits_T {
    SC1_OCR_BIAS_BIT = 0x01, // Inverts C1-C10 bit affect; can keep as 0 value
    SC1_OCR_OSC_RESET = 0x02, // set to 1 then back to 0
    SC1_OCR_EXT_CLK_ENB = 0x04, // do not use; this is for CSS test purposes
    SC1_OCR_CLK_DIV_MSK = 0x78, // core clock divider bits; set only 1 at a time
    SC1_OCR_CLK_DIV_8 = 0x08, // cores get osc divided by 8
    SC1_OCR_CLK_DIV_4 = 0x10, // cores get osc divided by 4
    SC1_OCR_CLK_DIV_2 = 0x20, // cores get osc divided by 2
    SC1_OCR_CLK_DIV_1 = 0x40, // cores get osc divided by 1
    SC1_OCR_CORE_ENB = 0x00000000007FFF80, // bits [22:7]; 1 enables clocks to the cores
    SC1_OCR_CORE_MSK = SC1_OCR_CORE_ENB,
    SC1_OCR_CORE0 = 0x00000080,
    SC1_OCR_CORE15 = (SC1_OCR_CORE0 << 15),
    SC1_OCR_C1_C5 = 0x000000000F800000, // bits [27:23]; VCO bias decrease
    SC1_OCR_C6_C10 = 0x00000001F0000000, // bits [32:28]; VCO bias increase
    SC1_OCR_BIAS_MSK = (SC1_OCR_C1_C5 | SC1_OCR_C6_C10),
#ifdef ENABLE_TEST_CLK_OUT // enable test clock out for test; else we don't want it enabled
    SC1_OCR_ENB_TEST_CLK_OUT
    = 0x0000000200000000 // bit 33; enable osc group 4 output
#else
    SC1_OCR_ENB_TEST_CLK_OUT
    = 0x0000000000000000 // bit 33; enable osc group 4 output
#endif
};

#define SC1_OCR_VCO_BIAS_SLOW_POS 23
#define SC1_OCR_VCO_BIAS_FAST_POS 28
#define SC1_OCR_VCO_BIAS_BITS_1 0x1
#define SC1_OCR_VCO_BIAS_BITS_2 0x3
#define SC1_OCR_VCO_BIAS_BITS_3 0x7
#define SC1_OCR_VCO_BIAS_BITS_4 0xF
#define SC1_OCR_VCO_BIAS_BITS_5 0x1F

#define SC1_OCR_CLK_DIV_INIT SC1_OCR_CLK_DIV_8
#define SC1_OCR_OSC_GROUP1 0x07
#define SC1_OCR_OSC_GROUP2 0x17
#define SC1_OCR_OSC_GROUP3 0x27
#define SC1_OCR_OSC_GROUP4 0x37

#define SC1_OCR_FASTEST_VAL (SC1_OCR_C6_C10 | SC1_OCR_CLK_DIV_1)
#define SC1_OCR_FAST_VAL (SC1_OCR_CLK_DIV_1)

#define SC1_OCR_MED_VAL (SC1_OCR_CLK_DIV_2)

// Based on info from CSS, expected oscillator of about 72MHz; clock out 1.125MHz for 0.6V core with values below
// This will not enable the clocks to the cores (i.e. bits 22-7 are 0's)
// Expected OCR settings for lowest clock rate at whatever the core voltage allows
#define SC1_OCR_SLOW_VAL (SC1_OCR_C1_C5 | SC1_OCR_CLK_DIV_8)

#define SC1_OCR_DIV8_VAL SC1_OCR_CLK_DIV_8
#define SC1_OCR_DIV4_VAL SC1_OCR_CLK_DIV_4
#define SC1_OCR_DIV2_VAL SC1_OCR_CLK_DIV_2
#define SC1_OCR_DIV1_VAL SC1_OCR_CLK_DIV_1

// Initialization value is; slow oscillator, disable clocks to cores, enable test clock output for now
#define SC1_OCR_STARTUP_VAL (SC1_OCR_SLOW_VAL | SC1_OCR_ENB_TEST_CLK_OUT)
#define SC1_OCR_INIT_VAL (SC1_OCR_CLK_DIV_8)

/**
 * \brief CSS SC1 ESR bit values; engine status register
 * E_SC1_REG_ECR : (Offset: 0x4E) (RO  64bits)
 * b0 engine done
 * b1 engine busy
 * b[3:2] style = 01
 * b[6:4] revision = 001
 * b[63:7] unused; undefined on read
 */
enum E_SC1_ESR_bits_T {
    E_SC1_ESR_DONE = 0x01,
    E_SC1_ESR_BUSY = 0x02,
    E_SC1_ESR_CORE_STYLE_MASK = 0x0C,
    E_SC1_ESR_CORE_REV_MASK = 0x70,
    E_SC1_ESR_CORE_STYLE_VALUE = 0x04, // b[3:2] = b'01
    E_SC1_ESR_CORE_REV_VALUE = 0x10 // b[6:4] = b'001
};

/**@}*/
#endif /* ifndef _CSS_SC1_DEFINES_H_INCLUDED */
