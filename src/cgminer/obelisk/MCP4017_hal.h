/**
 * \file MCP4017_hal.h
 *
 * \brief Digital E-Poteniometer firmware related declaration

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
 * Expose functions and public data needed for the driver.
 */

#ifndef _MCP4017_HAL_H_INCLUDED
#define _MCP4017_HAL_H_INCLUDED

#include "MCP4017_Defines.h"


/***  ASSOCIATED STRUCTURES/ENUMS      ***/

/***   GLOBAL DATA DECLARATIONS     ***/
// Task pending globals; for ISR callbacks, etc

/***   GLOBAL FUNCTION PROTOTYPES   ***/
extern int iWriteMCP4017(uint8_t uiValue);
extern int iReadMCP4017(uint8_t *puiValue);

#endif /* ifndef _MCP4017_HAL_H_INCLUDED */
