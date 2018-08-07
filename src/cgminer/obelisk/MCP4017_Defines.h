/**
 * \file MCP4017_Defines.h
 *
 * \brief MCP4017 related functionality declaration.
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
 *
 * \section Description
 * The MCP4017 device is a volatile, 7-bit digital potentiometer with I2C interface and Rheostat configuration.
 * Provides 128 steps of resistance between a wiper and B terminals. Default wiper setting is mid-scale. */

#ifndef _MCP4017_DEFINES_H_INCLUDED
#define _MCP4017_DEFINES_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>

#ifndef _HW_REG_ALIAS_INCLUDED
#define _HW_REG_ALIAS_INCLUDED
#define   _inreg     volatile const       //!< Defines 'read only' permissions
#define   _outreg    volatile             //!< Defines 'write only' permissions
#define   _ioreg     volatile             //!< Defines 'read / write' permissions
#endif   // _HW_REG_ALIAS_INCLUDED

/**

 */

// Default TWI slave address (7-bit); this gets left shifted by 1 bit and R/W bit is inserted as the lsb.
#define MCP4017_DEFAULT_ADDR   0x2F

// Device operation is simple; write to set and read to get the present value
// Data value is 7-bit, right justified with msb as don't care.  Setting the data higher
// will result in higher wiper resistance WRT the B terminal.
#define MAX_MCP4017_VALUE   127
#define MIN_MCP4017_VALUE   10      // pot can go to 0 but don't want to go lower then this on the design

/**@}*/
#endif /* ifndef _MCP4017_DEFINES_H_INCLUDED */
