/**
 * \file TCA9546A_hal.h
 *
 * \brief TCA9546A firmware related declaration.

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
 * TCA9546A is a two line, 1:4 port switch for the TWI/I2C bus.  This allow a
 * single I2C bus peripheral to control multiple downstream bus ports with multiple devices
 * with the same I2C slave address to not conflict.
 */

#ifndef _TCA9546A_HAL_H_INCLUDED
#define _TCA9546A_HAL_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>
#include "TCA9546A_Defines.h"


/***  ASSOCIATED STRUCTURES/ENUMS      ***/

/***   GLOBAL DATA DECLARATIONS     ***/
// Task pending globals; for ISR callbacks, etc

/***   GLOBAL FUNCTION PROTOTYPES   ***/
extern int iSetTwiSwitch(uint8_t uiBoard);    // set switch mask reg based on hashing board (0 to 2)

#endif /* ifndef _TCA9546A_HAL_H_INCLUDED */
