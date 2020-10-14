/**********************************************************************
* Copyright 2015-2019, CorTec GmbH
* All rights reserved.
*
* Redistribution, modification, adaptation or translation is not permitted.
*
* CorTec shall be liable a) for any damage caused by a willful, fraudulent or grossly 
* negligent act, or resulting in injury to life, body or health, or covered by the 
* Product Liability Act, b) for any reasonably foreseeable damage resulting from 
* its breach of a fundamental contractual obligation up to the amount of the 
* licensing fees agreed under this Agreement. 
* All other claims shall be excluded. 
* CorTec excludes any liability for any damage caused by Licensee's 
* use of the Software for regular medical treatment of patients.
**********************************************************************/
#ifndef CAPI_IMPLANTINFO_H
#define CAPI_IMPLANTINFO_H

#include <capi/capi.h>
#include <capi/channelinfo.h>

/**
* @file implantinfo.h
* @brief Information on the implant including channel info.
*/

typedef struct _HImplantInfo* HImplantInfo; ///< opaque type for passing CImplantInfo*

#ifdef __cplusplus
extern "C" {
#endif

/**
* Get the firmware version of the implant.
*
* @param[in] hImplantInfo           Handle to implant info (must be initialized).
* @param[out] firmwareVersionStringPtr Pointer to buffer, recommended buffer size is 128.
* @param[in] bufferLength           size of the buffer
* @param[out] stringLengthPtr       length of the returned string 
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantinfo_getFirmwareVersion(
    const HImplantInfo hImplantInfo,
    capi_char_t* const firmwareVersionStringPtr, const size_t bufferLength, size_t* const stringLengthPtr);

/**
* Get the type of the implant.
*
* @param[in] hImplantInfo          Handle to implant info (must be initialized).
* @param[out] deviceTypeStringPtr  Pointer to buffer, recommended buffer size is 128.
* @param[in] bufferLength          size of the buffer
* @param[out] stringLengthPtr      length of the returned string 
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantinfo_getDeviceType(
    const HImplantInfo hImplantInfo,
    capi_char_t* const deviceTypeStringPtr, const size_t bufferLength, size_t* const stringLengthPtr);

/**
* Get the unique implant identifier of the implant.
*
* @param[in] hImplantInfo    Handle to implant info (must be initialized).
* @param[out] deviceIdStringPtr  Pointer to buffer, recommended buffer size is 128.
* @param[in] bufferLength        size of the buffer
* @param[out] stringLengthPtr    length of the returned string 
*
* @return indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantinfo_getDeviceId(
    const HImplantInfo  hImplantInfo,
    capi_char_t* const deviceIdStringPtr, const size_t bufferLength, size_t* const stringLengthPtr);
 
/**
* Get information of the capabilities of each channel.
*
* @param[in] hImplantInfo Handle to implant info (must be initialized).
* @param[out] infoVector  Will contain information of the capabilities of each channel as a vector.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantinfo_getChannelInfo(
    const HImplantInfo  hImplantInfo,
    channelinfovector_t* const infoVector);

/**
* Get the total number of channels.
*
* @param[in] hImplantInfo  Handle to implant info (must be initialized).
* @param[out] channelCount Will contain the total number of channels.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantinfo_getChannelCount(
    const HImplantInfo  hImplantInfo,
    size_t* const channelCount);
 
/**
* Get the number of channels with measurement capabilities.
*
* @param[in] hImplantInfo             Handle to implant info (must be initialized).
* @param[out] measurementChannelCount Will contain the number of channels with measurement capabilities.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantinfo_getMeasurementChannelCount(
    const HImplantInfo  hImplantInfo,
    size_t* const measurementChannelCount);

/**
* Get the number of channels with stimulation capabilities.
*
* @param[in] hImplantInfo             Handle to implant info (must be initialized).
* @param[out] stimulationChannelCount Will contain the number of channels with stimulation capabilities.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantinfo_getStimulationChannelCount(
    const HImplantInfo  hImplantInfo,
    size_t* const stimulationChannelCount);

/**
* Get the measurement sampling rate.
*
* @param[in] hImplantInfo  Handle to implant info (must be initialized).
* @param[out] samplingRate Will contain the measurement sampling rate.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantinfo_getSamplingRate(
    const HImplantInfo  hImplantInfo,
    uint32_t* const samplingRate);

/**
* Destroy implant info handle.
*
* @param[in] hImplantInfo Address of handle to implant info. Handle is set to null after destruction.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantinfo_destroy(HImplantInfo* const hImplantInfo);
 
#ifdef __cplusplus
}
#endif

#endif // CAPI_IMPLANTINFO_H