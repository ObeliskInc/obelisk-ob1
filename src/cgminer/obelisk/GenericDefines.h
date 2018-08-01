/**
 * \file GenericDefines.h
 *
 * \brief Header for GenericDefines.c
 * \date STARTED: 2017/09/06
 *
 */
#ifndef _GENDEFINES_H_INCLUDED
#define _GENDEFINES_H_INCLUDED

/**
 * @file GenericDefines.h
 *
 * @brief Some generic and useful defines
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

/***   INCLUDE FILES               ***/

/***   USEFUL DEFINITIONS          ***/
#define bSUCCESS    false
#define bFAIL       true
#define bERROR      true
#define MAX_POS_INT16              (0x7fff)            // 32767 or 2**15-1
#define MAX_POS_INT32              (0x7fffffff)        // 2**31-1
#define MATH_PI                    (3.14159265359)
#define PI_TIMES_TWO               (2.0 * MATH_PI)
#define PI_BY_HALF                 (0.5 * MATH_PI)
#define RADIANS_PER_DEGREE         (MATH_PI / 180.0)
#define DEGREES_PER_RADIAN         (1.0 / RADIANS_PER_DEGREE)
#define SQUARE_OF(x)               ( (x) * (x) )
#define MIN_OF(a,b)    (((a)<(b))?(a):(b))
#define MAX_OF(a,b)    (((a)<(b))?(b):(a))
#define ELEMENTS_OF(x) ((int) (sizeof(x)/sizeof(x[0])))
#define KELVIN_OFFSET               (273.15)

//#define MSG_TKN_INFO "-I-"

#endif /* ifndef _GENDEFINES_H_INCLUDED */

