/**
 * \file ADS1015.h
 *
 * \brief Voltage monitor firmware related declaration

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

#ifndef _ADS1015_H_INCLUDED
#define _ADS1015_H_INCLUDED

#include "ads1015_Defines.h"

/***  ASSOCIATED STRUCTURES/ENUMS      ***/
// #define PROTOTYPE_REV0 1
#ifdef PROTOTYPE_REV0
#warning "Compiled for prototype voltage monitor channel order"
// #TODO - SOLVED - temporary reordering of the ADC inputs due to prototype footprint error (PROTOTYPE_REV0)
// These need to be changed back in the non-prototype hardware.
#define VMON_V15IO E_ASIC_V15IO
#define VMON_VIN 1 // will be E_PS12V in next rev
#define VMON_V1 2 // will be E_ASIC_V1 in next rev
#define VMON_V15 3 // will be E_ASIC_V15 in next rev
#define VMON_IN_FAULT ERR_NONE // // ignore because one or more test boards are broken
#else

#define VMON_V15IO E_ASIC_V15IO
#define VMON_VIN E_PS12V
#define VMON_V1 E_ASIC_V1
#define VMON_V15 E_ASIC_V15
#define VMON_IN_FAULT ERR_FAILURE

#endif // #ifdef PROTOTYPE_REV0

typedef struct {
    // Using signed type because can get small negative values around 0
    int16_t iaDataRawInt[E_NUM_ADS1015_CHANS];
    int16_t iaDataAsmV[E_NUM_ADS1015_CHANS];
} S_ADS1015_DATA_T;

#define MIN_VIN_MV 11500 // 11.5V is min expected power supply input
#define MAX_VIN_MV 12500 // 12.5V is max expected power supply input
#define MIN_V15_MV 7200 // 7.2V is min expected string voltage
#define MAX_V15_MV 11500 // 11.5V is max expected voltage
#define MIN_V15IO_MV 7600 // 7.2V is min expected voltage
#define MAX_V15IO_MV 11500 // 11.5V is max expected string voltage
#define START_V1_MV 250 // 0.25V is min voltage at asic 1 core at min string voltage
#define MIN_V1_MV 500 // 0.5V is min operational voltage at asic 1 core
#define MAX_V1_MV 800 // 0.8V is max operational voltage at asic 1 core

/***   GLOBAL DATA DECLARATIONS     ***/
// Task pending globals; for ISR callbacks, etc

/***   GLOBAL FUNCTION PROTOTYPES   ***/
extern int iADS1015Initialize(uint8_t uiBoard);
extern int iADS1015ReadVoltages(uint8_t uiBoard);
extern void ReportVMon(uint8_t uiBoard);
extern int iGetVMonDataPtr(uint8_t uiBoard, S_ADS1015_DATA_T** ppVMonDataStruct);

extern double getASIC_V15IO(uint8_t uiBoard);
extern double getASIC_V1(uint8_t uiBoard);
extern double getASIC_V15(uint8_t uiBoard);
extern double getPS12V(uint8_t uiBoard);

// extern int iGetPSmV(uint8_t uiBoard, uint16_t *puiPSmV);

#endif /* ifndef _ADS1015_H_INCLUDED */
