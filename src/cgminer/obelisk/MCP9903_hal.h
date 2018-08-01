/**
 * \file MCP9903_hal.h
 *
 * \brief Remote diode temperature sensor firmware related declaration

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

#ifndef _MCP9903_HAL_H_INCLUDED
#define _MCP9903_HAL_H_INCLUDED

#include "MCP9903_Defines.h"

#define TEMP_SENSOR_FAULT_VALUE 256 // use absurd value for faulted sensor
#define TEMP_SENSOR_MAX_VALUE 128 // maximum temperature we should get from device
#define TEMP_SENSOR_MIN_VALUE 0 // minimum temperature we should get from device

/***  ASSOCIATED STRUCTURES/ENUMS      ***/
#define TEMP_INT 0 // Internal MCP9903 junction
#define TEMP_ASIC 1 // PN junction in ASIC1
#define TEMP_PS 2 // PN junction near the power supply

typedef struct {
    // Using signed type but expect data to be from 0 to 127 in 0.125 deg C increments
    int16_t iaDataRawInt[NUM_MCP9903_CHANS]; // integer degrees
    int16_t iaDataRawFrac[NUM_MCP9903_CHANS]; // fractional 8ths of degrees
} S_MCP9903_DATA_T;

/***   GLOBAL DATA DECLARATIONS     ***/
// Task pending globals; for ISR callbacks, etc

/***   GLOBAL FUNCTION PROTOTYPES   ***/
extern int iMCP9903Initialize(uint8_t uiBoard);
extern int iMCP9903ReportTemps(uint8_t uiBoard);
extern int iUpdateTempSensors(uint8_t uiBoard);
extern int iGetTemperaturesDataPtr(uint8_t uiBoard, S_MCP9903_DATA_T** ppTempDataStruct);

extern double getIntTemp(uint8_t uiBoard);
extern double getAsicTemp(uint8_t uiBoard);
extern double getPSTemp(uint8_t uiBoard);

#endif /* ifndef _MCP9903_HAL_H_INCLUDED */
