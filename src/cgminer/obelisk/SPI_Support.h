/**
 * \file SPI_Support.h
 *
 * \brief SPI port firmware related declares.
 *
 * \section Legal
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
 */

#ifndef _SPI_SUPPORT_H_INCLUDED
#define _SPI_SUPPORT_H_INCLUDED

//#include <compiler.h>
//#include <utils.h>
#include "Ob1Hashboard.h"

/***  ASSOCIATED STRUCTURES/ENUMS      ***/
typedef enum  {
    E_SPI_XFER8       = 0,  // read/write spi transfer; 8 bits at a time
    E_SPI_XFER        = 1,  // read/write spi transfer; more than 8 bits per word (not used here)
    E_SPI_XFER_READ   = 2,  // read only spi transfer; (not used here)
    E_SPI_XFER_WRITE  = 3   // write only spi transfer; 8 bits at a time
} E_SPI_XFER_TYPE;

/***   GLOBAL DATA DECLARATIONS     ***/
// Task pending globals; for ISR callbacks, etc
#define SPI_READ_RATE_MHZ  1000000  // 1 MHz read/write transfers
#define SPI_WRITE_RATE_MHZ 1000000  // 1 MHz write only transfers; may change later to 5MHz for writes

/***   GLOBAL FUNCTION PROTOTYPES   ***/
extern void SPI5_Startup(void);

//TODO: extern int32_t spi_m_async_transfer16(struct spi_m_async_descriptor *spi, uint8_t const *txbuf, uint8_t *const rxbuf, const uint16_t length);
extern int32_t spi_m_async_transfer16(void);

//TODO: extern void FlushMSyncSpiRxBuf(struct _spi_m_async_dev *dev);
extern void FlushMSyncSpiRxBuf(void);

//TODO: extern void SetSPIClockRate(struct spi_m_async_descriptor *spi, uint32_t uiRateMHz);
extern void SetSPIClockRate(void);

extern bool bIsSPI5_CBOccurred(bool bClear);

extern bool bSPI5StartDataXfer(E_SPI_XFER_TYPE eSPIXferType, uint8_t const *pucaTxBuf, uint8_t *const pucaRxBuf, const uint16_t uiLength);
//extern bool bSPI5StartDataXfer(void);

extern int iIsHBSpiBusy(bool bWait);

extern void SPITimeOutError(void);

//extern void HBSetSpiMux(void);
void HBSetSpiMux(E_SC1_SPISEL_T eSPIMUX);

extern void HBSetSpiSelectsFalse(uint8_t uiBoard);
extern void HBSetSpiSelects(uint8_t uiBoard, bool bState);

extern void DeassertSPISelects(void);

extern bool bHBGetSpiSelects(uint8_t uiBoard);

#endif /* ifndef _SPI_SUPPORT_H_INCLUDED */
