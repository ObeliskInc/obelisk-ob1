/**
 * \file ADS1015.c
 *
 * \brief ADS1015 firmware device driver; application specific
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
 * \section Purpose
 * Mid level device drivers for the ADS1015 using TWI/I2C interface. Implemented for custom
 * configuration of the SC1 hashing board rather than as a generic device driver.
 *
 * On the SC1 design the ADS1015 device is used to monitor ASIC string related voltages and
 * incoming power supply voltage.  This uses a state machine to read the device data and then
 * write the next configuration word to start conversion for the next sample.  The channel order
 * is stored in a list and can be varied by programming the list.
 *
 * The device is physically located on the hashing board.  This supports four measurement channels,
 * which in this application are ground referenced and single-ended. All inputs use the same PGA
 * settings for +/-4V range.
 *
 * \section Description
 * ADS1015 is a 4-channel, 12-bit, delta-sigma type ADC with internal reference, oscillator,
 * PGA and I2C interface.  Conversion rates of up to 3.3KSPS are possible (single channel).
 * Data is read as 12-bit value but left-justified with 0's as a 16-bit (2 byte) readback.
 * The value read is a signed, bipolar value representing the differential voltage between
 * channel pairs. This application uses ground-referred, single-ended measurements. Therefore,
 * only about half the 12-bit range is used.
 *
 * This application does not use the interrupt output of the IC.  The sample rate is controlled by
 * software methods. This device does not have an auto-sequencer for the channels. Conversions will
 * use the single-shot/power-down conversion mode. Power-down is automatically entered after each
 * conversion is completed and the data is then stored in the device conversion register for future readback.
 * The firmware will operate by periodically reading the prior converted value then switching the
 * mux to the next channel and starting the next conversion.  To do this, it should be called
 * periodically at regular intervals.  The interval periods and sequencing will define the sample rate.
 *
 * As the measurements will be on slowly changing (DC power) signals, a software based sample rate
 * of 100SPS or less per channel (X4 channels = 400SPS device) will be sufficient.  The firmware
 * driver can add a low pass filter and maintain an accessor function so that other functions can
 * read the recent data values for diagnostic and control loop purposes.
 *
 * Since in this application the measurements are for DC (low frequency) power, use of a downsampling
 * filter is not a strict requirement for functions that consume the sampled data.
 *
 * The device has two main registers: conversion and configuration.  A handler function can be used to
 * operate a state machine that cycles through the channels, starts the conversions, reads the data, etc.
 *
 * I2C message format; bit order is msb to lsb
 * For I2C master WRITE: at minimum 4 bytes are required
 * byte 1: control byte; value is: 10010xx R/W#; where
 *         10010xx is fixed 7-bit device code; xx is determined by ADDR pin connection at reset
 *         R/W# single bit indicates type of transfer
 * byte 2: register pointer; i.e. config, conversion, comparators
 * byte 3: (optional) data; MSB of 16-bit value to write
 * byte 4: (optional) data; LSB of 16-bit value to write
 *
 * For I2C master READ: at minimum 3 bytes are required; this does NOT set the register pointer.
 * The previously set register pointer is used to read the desired data. This device supports the
 * repeated START type of transfer for reads.
 * byte 1: control byte; value is: 10010xx R/W#; where
 *         10010xx is fixed 7-bit device code; xx is determined by ADDR pin connection at reset
 *         R/W# single bit indicates type of transfer
 * byte 2: data MSB of 16-bit value being read
 * byte 3: data LSB of 16-bit value being read
 *
 * Code is written for the Atmel SAMG55 (ARM-M4); but can be ported to the SAMA5D27 SOM or other MCUs.
 *
 * ADC channel configurations:
 * Full scale range = +/- 4.096V (3.3V max single ended)
 * Output data rate = selectable; using fastest conversion rate but restarting with software
 * conversion mode: single-shot CONFIG.MODE=1
 *
 * The four channels are used in this application as follows below. Channels 0, 2 and 3 have a 4:1 voltage
 * reduction using a resistance divider.  Channel 1 does not have a divider. More information provided in
 * the code and defines below.
 * CH0: V15IO is the top-most ASIC I/O voltage; this is generated from an OPAMP based on core voltage
 *      of the top most ASIC. It provides a measure of whether the OPAMP is functioning correctly.
 *      Typical voltage expected is about 0.5V more than the V15 (CH2 AIN2). This is a diagnostic reading only.
 * CH1: Lowest ASIC V1 core voltage; provides what the lowest ASIC in the string is operating at.  It is
 *      assumed all ASICs will have similar (but not the same) core voltage as the string voltage divides
 *      evenly across the ASICs.  The expected voltage is 0.5V to 0.7V. This is a diagnostic reading only.
 *      Initial plan is to use the same +/-4V range as the other channels but it may be possible to use a
 *      different PGA setting to improve resolution.
 * CH2: V15 is the top-mode ASIC core voltage; this is generated from the DC-DC converter that powers the
 *      ASIC string.  This is the most important of the measured data as this is used to feedback the
 *      control loop that sets the string voltage.  It is used to account for drift and low-tolerances on
 *      devices that set the string voltage. Expected typical range is about 7.5 to 9.5V.
 * CH3: VIN is the input supply voltage; this is generated from the external power supply. Typical range
 *      is 12V (11.5 to 12.5V).  This is a diagnostic signal.
 *
 * \section Module History
 * - 20180608 Rev 0.1 Started; using AS7 (build 7.0.1417)
 *
 */

/***    INCLUDE FILES       ***/
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "Usermain.h"
#include "TWI_Support.h"
#include "Ob1Hashboard.h"
#include "ads1015.h"
#include "MiscSupport.h"
#include "Console.h"
#include "MiscSupport.h"

/***    LOCAL DEFINITIONS     ***/
#define MAX_NUM_OF_ADS1015 MAX_NUMBER_OF_HASH_BOARDS // one per board in this design

#define DEV_INIT_DEV_MESSAGE "ADS1015 Init: "

// Defines for actual TWI/I2C transport
#define ADS1015_READ_BUFF_SIZE 4 // max bytes size we expect to have to transfer to the device
#define ADS1015_WRITE_BUFF_SIZE ADS1015_READ_BUFF_SIZE

#define MUX_BITS_POS 12 // shifts bits to ADC mux channel bits [13:12]

// To write to the device we can write only the ptr reg or for the configuration we can also follow with the data
// To read, we first write the point to the reg and then do a read as separate or as repeated START.
#define PTR_REGISTER_WRITE_COUNT 1 // reg offset only; data is not written
#define CONVERSION_REGISTER_SIZE 2 // 2 bytes, 16 bits of data, MSB reads out first
#define CONFIGURATION_REGISTER_SIZE 2 // 2 bytes, 16 bits of data, MSB reads out first
#define CONFIGURATION_REGISTER_WRITE_COUNT (CONFIGURATION_REGISTER_SIZE + PTR_REGISTER_WRITE_COUNT) // reg offset plus the data

#define ADC_NUM_BITS 16 // Converted 12-bit values are left justified with 0's; so they appear as 16-bits

// ADS1015 OS bit is 0 for conversion in progress and 1 if not converting; in case we want to poll this
#define IS_CONV_IN_PROGRESS(x) (0 == ((x)&OP_STATUS))

// For the single-ended, ground referenced, generally positive inputs we would expect only positive or
// possibly slightly negative values.
// Define some invalid values we should never see so we can set data buffers to an invalid value...
#define ADS1015_INVALID_DATA (0xC3A5) // something we should never see from the ADS1015 data on our design
#define ADS1015_INVALID_CONFIG (0) // something we should never see from the ADS1015 config on our design
// #define UNUSED_TAG_CONFIG_DATA            ((int)E_NUM_ADS1015_CHANS)  // tag for data if configuration status

// Convert from ADC counts to actual voltages at the AIN inputs to the ADC (compensate for resistive dividers).
// ADS1015 has a PGA that we will use for +/-4.096V input range (8.192 total);
// but AINn inputs can't exceed supply rail which will be 3.3Vdc.
// Input = AINp - AINn and AINn is ground since we will be single-ended and ground-referred.
// Actual precision will be about 11 bits since it can't use the negative range (i.e. the msb is not used).
// But since the bits are left-justified into a 16-bit word, it looks like a 16-bit value.
//
// For a 16-bit value and 8.192V range, lsb weighting is 125uV.  If it is shifted right by 3 places
// it will provide a convenient 1mV per lsb weighting as an integer value.  This shifts off 0's from
// the lsbs so we don't lose any information.
// However, because the ratio of resistive divider on the inputs is 4.02 it is close enough to 4 to simply
// multiply the value by 4 which is a 2-bit left shift.  The end result is to shift the raw data right by
// 1-bit to convert raw ADC input into the scaled mV value being monitored.
// Macro converts raw data ADS1015 lsb counts to mV for
#define CONV_RAW_AIN_TO_MILLIVOLT_CH023(x) ((x) >> 1) // Ch0, 2 and 3 have the 4:1 resistive divider
#define CONV_RAW_AIN_TO_MILLIVOLT_CH1(x) ((x) >> 3) // Ch1 does not have 4:1 resistive divider

/***    LOCAL STRUCTURES/ENUMS      ***/

/***    LOCAL FUNCTION PROTOTYPES   ***/
static int iADS1015Reset(uint8_t uiBoard);
static int iADS1015ReadSample(uint8_t uiBoard, int16_t* piSample);

static int iConfigureChannel(uint8_t uiBoard, bool bAdvance);
static int iADS1015ReadReg(uint8_t uiBoard, uint8_t uiReg);
static int16_t iADS1015SampleTomV(int16_t iSample, uint8_t uiChannel);

/***    LOCAL DATA DECLARATIONS     ***/
static uint8_t ucaTWIOutBuf[ADS1015_WRITE_BUFF_SIZE]; // buffer for sending out
static uint8_t ucaTWIInBuf[ADS1015_READ_BUFF_SIZE]; // buffer for reading in
static S_ADS1015_DATA_T saADCData[MAX_NUM_OF_ADS1015]; // Most recent conversion data

// ADS1015 does not have an automatic channel sequencer so we make one as a list.  We can fill the list with whatever
// order of channels we want.  The same list sequence is used for all boards but each board has its own index.
#define CHANNEL_SEQUENCE_NUM E_NUM_ADS1015_CHANS // sequence list size; using one per channel here
static uint8_t uiaChanSeq[CHANNEL_SEQUENCE_NUM]; // channel sequencer list; gets init at start
static uint8_t uiaChanSeqIndex[MAX_NUM_OF_ADS1015]; // each board has its own list index

static S_TWI_TRANSFER_T sADS1015TwiXferBuf;

/*********************************/
/***                             */
/***   THE CODE BEGINS HERE    ***/
/***                             */
/*********************************/

/** *************************************************************
 *  \brief Support function to report the present system voltages to the console
 *  Serves as an example of how to read the values
 *
 *  \param uint8_t uiBoard sets which board to configure
 *  \return none
 */
void ReportVMon(uint8_t uiBoard)
{
    S_ADS1015_DATA_T* psVMonData;
    int16_t iValuemV, iValueRaw;
    int iResult;

    iResult = iGetVMonDataPtr(uiBoard, &psVMonData); // get where the data is stored
    if (ERR_NONE == iResult) {
        iResult = iADS1015ReadVoltages(uiBoard);
    }
    if (ERR_NONE == iResult) {
        iValueRaw = psVMonData->iaDataRawInt[VMON_VIN];
        iValuemV = psVMonData->iaDataAsmV[VMON_VIN];
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "-V- HB%d: VIN= %dmV (%d) \r\n", uiBoard + 1, iValuemV, iValueRaw);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        iValueRaw = psVMonData->iaDataRawInt[VMON_V15];
        iValuemV = psVMonData->iaDataAsmV[VMON_V15];
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "          V15= %dmV  (%d) \r\n", iValuemV, iValueRaw);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        iValueRaw = psVMonData->iaDataRawInt[VMON_V15IO];
        iValuemV = psVMonData->iaDataAsmV[VMON_V15IO];
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "          VI5IO= %dmV (%d) \r\n", iValuemV, iValueRaw);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
        iValueRaw = psVMonData->iaDataRawInt[VMON_V1];
        iValuemV = psVMonData->iaDataAsmV[VMON_V1];
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "          V1= %dmV  (%d) \r\n", iValuemV, iValueRaw);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);

    } else {
        (void)snprintf(caStringVar, CONSOLE_LINE_SIZE, "-V- HB %d: Error reading voltage monitor\r\n", uiBoard + 1);
        CONSOLE_OUTPUT_IMMEDIATE(caStringVar);
    }

} // ReportSysVoltages

/** *************************************************************
 * \brief Accessor function to get a pointer to the most recent data from the ADS1015.
 *  User should call the iADS1015ReadVoltages() function first (or periodically) to force the
 *  update of the data.  The data location does not change so the ptr can be gotten once and then
 *  the data can be accessed as needed.
 * \param uiBoard defines which board to target; 0 to MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \param ppVMonDataStruct is ptr to ptr to S_ADS1015_DATA_T where the most recent ADS1015 data is stored.
 * \return int iError
 * \retval ERR_NONE if success
 * \retval ERR_INVALID_DATA if hash board is not detected or other problem
 */
int iGetVMonDataPtr(uint8_t uiBoard, S_ADS1015_DATA_T** ppVMonDataStruct)
{
    int iResult = ERR_NONE;

    if (ERR_NONE != iIsBoardValid(uiBoard, false)) {
        iResult = ERR_INVALID_DATA;
    } else {
        *ppVMonDataStruct = &saADCData[uiBoard];
    } // if (ERR_NONE != iIsBoardValid(uiBoard,false))

    return (iResult);
} // iGetVMonDataPtr()

/** *************************************************************
 * \brief Convert the raw sample number into an integer of millivolt value
 * \param iSample is ADS1015 sample data
 * \param iChannel is the channel number (so it knows how to scale it)
 * \return iDatamV is integer value in mV
 */
static int16_t iADS1015SampleTomV(int16_t iSample, uint8_t uiChannel)
{
    int16_t iDatamV;

    // Chan order is:
    // AIN0   CH0  V15IO
    // AIN1   CH1  V1
    // AIN2   CH2  V15
    // AIN3   CH3  VIN
    switch (uiChannel) {
    default:
        iDatamV = -1;
        break;
    case VMON_V15IO:
    case VMON_VIN:
    case VMON_V15:
        iDatamV = CONV_RAW_AIN_TO_MILLIVOLT_CH023(iSample);
        break;
    case VMON_V1: // AIN1/Ch2 is V1 and is not scaled by a resistor divider
        iDatamV = CONV_RAW_AIN_TO_MILLIVOLT_CH1(iSample);
        break;
    } // end switch

    return (iDatamV);
} // iADS1015SampleTomV()

/** *************************************************************
 * \brief Reads the converted samples from the ADS1015 and configures to convert the next
 * sample.
 * \param uiBoard defines which board to target; 0 to MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \return int error codes from err_codes.h
 * \retval ERR_NONE if success
 * \retval ERR_INVALID_DATA if hash board is not detected or other problem
 * \retval other error codes from err_codes.h
 */
int iADS1015ReadVoltages(uint8_t uiBoard)
{
    int iResult = ERR_NONE;
    int16_t iSample, imillivolt;
    uint8_t uixChannel;

    if (ERR_NONE != iIsBoardValid(uiBoard, false)) {
        iResult = ERR_INVALID_DATA;
    } else {
        uiaChanSeqIndex[uiBoard] = 0; // reset the channel index to first of the list
        uixChannel = 0; // local channel/loop counter
        while (ERR_NONE == iResult) {
            if (0 == uixChannel) {
                // Despite the data sheet information saying you can do a conversion and then read the result; it appears
                // we must read the first result twice because the conversion register has an old value that comes out.
                iResult = iADS1015ReadSample(uiBoard, &iSample); // throw-away read of stale data
            } // if (0 == uiChannel)
            if (ERR_NONE == iResult) {
                iResult = iConfigureChannel(uiBoard, true); // do the conversion with channel advance
                if (ERR_NONE == iResult) {
                    usleep(1000); // delay to allow conversion before we read the data; (only should need 1ms at 3300KSPS)
                    iResult = iADS1015ReadSample(uiBoard, &iSample); // Read the most recent converted sample
                    if (ERR_NONE == iResult) {
                        saADCData[uiBoard].iaDataRawInt[uixChannel] = iSample;
                        imillivolt = iADS1015SampleTomV(iSample, uiaChanSeq[uixChannel]); // convert sample value into an integer voltage (in mV)
                        saADCData[uiBoard].iaDataAsmV[uixChannel] = imillivolt;
                    } // if (ERR_NONE == iResult)
                }
            } // if (ERR_NONE == iResult)
            if (CHANNEL_SEQUENCE_NUM <= ++uixChannel) {
                break; // while
            }
        } // while (ERR_NONE == iResult)

    } // if (ERR_NONE != iIsBoardValid(uiBoard,false))

    return (iResult);
} // iADS1015ReadVoltages()

/*
    E_ASIC_V15IO         = E_FIRST_ADS1015_CHAN,  // VDDIO at top of string
    E_ASIC_V1            = 1,                // 1 ASIC 1 core voltage
    E_ASIC_V15           = 2,                // 2 Top of string voltage (DC-DC output)
    E_PS12V              = 3,                // 3 Input power supply voltage
*/

// Values are returned in volts
double getASIC_V15IO(uint8_t uiBoard)
{
    return ((double)saADCData[uiBoard].iaDataAsmV[E_ASIC_V15IO]) / 1000.0;
}

double getASIC_V1(uint8_t uiBoard)
{
    return ((double)saADCData[uiBoard].iaDataAsmV[E_ASIC_V1]) / 1000.0;
}

double getASIC_V15(uint8_t uiBoard)
{
    return ((double)saADCData[uiBoard].iaDataAsmV[E_ASIC_V15]) / 1000.0;
}

double getPS12V(uint8_t uiBoard)
{
    return ((double)saADCData[uiBoard].iaDataAsmV[E_PS12V]) / 1000.0;
}

/** *************************************************************
 * \brief Reads the most recent sample from the ADS1015 conversion register.  The read data
 *  as bytes is converted to a 16 bit unsigned int and represents raw data from the device.
 * \param uiBoard defines which board to read; 1 to MAX_HASH_BOARDS is valid range
 * \param piSample ptr to int16_t to return value
 * \return int error codes from err_codes.h
 * \retval ERR_NONE if success
 * \retval ERR_INVALID_DATA if hash board is not detected or other problem
 * \retval other error codes from err_codes.h
 */
static int iADS1015ReadSample(uint8_t uiBoard, int16_t* piSample)
{
    int iResult = ERR_NONE;

    if (ERR_NONE != iIsBoardValid(uiBoard, false)) {
        iResult = ERR_INVALID_DATA;
    } else {
        // Read the prior converted sample
        iResult = iADS1015ReadReg(uiBoard, ADS1015_REG_CONVERSION);
        if (ERR_NONE == iResult) {
            *piSample = (int16_t)(uiArrayToUint16(ucaTWIInBuf, true)); // convert to 16 bit number with endian swap
        }
    } // if (ERR_NONE == iIsBoardValid(uiBoard,false))

    if (ERR_NONE != iResult) {
        *piSample = ADS1015_INVALID_DATA;
    }
    return (iResult);
} // iADS1015ReadSample()

/** *************************************************************
 * \brief Initialize the ADS1015 driver and related.
 *
 * \param uiBoard defines which board to target; 0 to MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \return int error codes from err_codes.h
 */
int iADS1015Initialize(uint8_t uiBoard)
{
    static bool bSequenceListSet = false; // do once
    int iResult;
    int ixI;

    if ((0 <= uiBoard) && (MAX_NUMBER_OF_HASH_BOARDS > uiBoard)) {
        // There is one sequence list for all devices; but each has its own pointer to the list.
        // This only needs to be initialized once.  This list can be overwritten later if the channel
        // order is to be changed.
        if (false == bSequenceListSet) { // set default channel sequences on first call
            // default list is to sequence through the channels in order; this can be changed elsewhere, later on.
            for (ixI = 0; ixI < CHANNEL_SEQUENCE_NUM; ixI++) {
                uiaChanSeq[ixI] = (uint8_t)ixI;
            } // for (ixI ...
            uiaChanSeqIndex[uiBoard] = 0;
            bSequenceListSet = true;
        } // if (false == bSequenceListSet)

        iResult = iADS1015Reset(uiBoard);
        if (ERR_NONE == iResult) {
            iResult = iADS1015ReadVoltages(uiBoard); // prime the device ; get first samples
        }
    }

    return (iResult);
} // ADS1015Initialize()

/** *************************************************************
 * \brief Reset the ADS1015 using the general call address defined in spec.
 *
 * \param uiBoard defines which board to target; 0 to MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \return int error codes from err_codes.h
 * SideEffects: this is a general call reset so it may reset other devices on the I2C bus.
 */
static int iADS1015Reset(uint8_t uiBoard)
{
#define ADS1015_GEN_CALL_RESET_COUNT 1
    int iResult;

    sADS1015TwiXferBuf.ePort = uiBoard;
    sADS1015TwiXferBuf.eXferMode = E_TWI_WRITE;
    sADS1015TwiXferBuf.uiAddr = ADS1015_GENERAL_CALL_ADDR;
    sADS1015TwiXferBuf.uiCount = ADS1015_GEN_CALL_RESET_COUNT;
    ucaTWIOutBuf[0] = ADS1015_RESET;
    sADS1015TwiXferBuf.puiaData = ucaTWIOutBuf;

    iResult = iTWIInitXFer(&sADS1015TwiXferBuf);
    delay_ms(1); // without this delay the I2C bus doesn't work properly; #TODO figure out why
    //	usleep(1000);

    return (iResult);
    //	return(ERR_NONE);
} // iADS1015Reset()

/** *************************************************************
 * \brief Read the device.  Sets the register pointer to the desired register and then
 * uses a repeated start to read back the data.
 * Typically this will be the conversion register but it might be the configuration register.
 *
 * \param uiBoard defines which board to target; 0 to MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \param uiReg which reg; ADS1015_REG_CONVERSION, ADS1015_REG_CONFIG, etc
 * \return int error codes from err_codes.h
 * Assumes the registers are the same size as CONVERSION_REGISTER_SIZE
 */
static int iADS1015ReadReg(uint8_t uiBoard, uint8_t uiReg)
{
    int iResult;

    sADS1015TwiXferBuf.ePort = uiBoard;
    sADS1015TwiXferBuf.eXferMode = E_TWI_READ_REG;
    sADS1015TwiXferBuf.uiAddr = ADS1015_TWI_SA7B;
    sADS1015TwiXferBuf.uiReg = uiReg;
    sADS1015TwiXferBuf.uiCount = CONVERSION_REGISTER_SIZE;
    sADS1015TwiXferBuf.puiaData = ucaTWIInBuf;
    iResult = iTWIInitXFer(&sADS1015TwiXferBuf);

    usleep(1000);

    return (iResult);

} // iADS1015ReadReg()

/** *************************************************************
 * \brief Configure the ADS1015 to start the next conversion.  Fills the transfer
 * container struct and then calls the TWI transport driver function to execute the
 * transfer.  This is done after the data conversion register has been read and prepares
 * the device to sample the next channel.
 *
 * Directs the TWI driver to send multiple byte command that:
 * 1) Sends the register pointer to the config reg
 * 2) Send MSB of the config data
 * 3) Send LSB of the config data
 * Configuration is hard coded and assumes the same configuration for all channels.
 * Optionally advances the channel from the present one to the next in modulo order.
 *
 * \param uiBoard defines which board to target; 0 to MAX_NUMBER_OF_HASH_BOARDS-1 is valid range
 * \param bAdvance if true, moves to next channel for the next time called
 * \return int error codes from err_codes.h
 */

static int iConfigureChannel(uint8_t uiBoard, bool bAdvance)
{
#define ADS1015_CONFIG (BEGIN_CONVERSION | SINGLE_ENDED | PGA_4V096 | MODE_SINGLE_SHOT | DATA_RATE_3300 | COMP_DISABLE)

    uint16_t uiConfigWord;

    // Configures the device to take a measurement on the channel in the list
    uiConfigWord = ADS1015_CONFIG | ((uiaChanSeq[uiaChanSeqIndex[uiBoard]] << MUX_BITS_POS) & CONFIG_MUX_BITS);
    if ((true == bAdvance) && (CHANNEL_SEQUENCE_NUM <= ++uiaChanSeqIndex[uiBoard])) {
        uiaChanSeqIndex[uiBoard] = 0;
    }

    sADS1015TwiXferBuf.ePort = uiBoard;
    sADS1015TwiXferBuf.eXferMode = E_TWI_WRITE;
    sADS1015TwiXferBuf.uiAddr = ADS1015_TWI_SA7B;
    sADS1015TwiXferBuf.uiCount = CONFIGURATION_REGISTER_WRITE_COUNT;
    ucaTWIOutBuf[0] = (uint8_t)ADS1015_REG_CONFIG;
    Uint16ToArray(uiConfigWord, &ucaTWIOutBuf[1], true); // convert to array with endian swap to network order
    sADS1015TwiXferBuf.puiaData = ucaTWIOutBuf;

    return (iTWIInitXFer(&sADS1015TwiXferBuf));

} // iConfigureChannel()
