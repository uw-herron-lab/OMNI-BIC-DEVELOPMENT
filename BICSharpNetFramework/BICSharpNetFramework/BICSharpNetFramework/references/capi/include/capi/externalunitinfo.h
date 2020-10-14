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
#ifndef CAPI_EXTERNALUNITINFO_H
#define CAPI_EXTERNALUNITINFO_H

#include <capi/capi.h>

/**
* @file externalunitinfo.h
* @brief Information about the external unit.
*/

typedef struct _HExternalUnitInfo* HExternalUnitInfo; ///< opaque type for passing CExternalUnitInfo*

/**
  * @brief Vector of external unit information, one for every connected and available external unit.
  */
typedef struct _ExternalUnitInfoVector
{
    size_t count;               ///< number of elements inside the vector array
    HExternalUnitInfo* vector; ///< array of external unit info handles
} externalunitinfovector_t;


#ifdef __cplusplus
extern "C" {
#endif

/**
* Get the type of the connected implant.
*
* @param[in] hExternalUnitInfo Handle to external unit info (must be initialized).
* @param[out] typeStringPtr    Pointer to buffer, recommended buffer size is 128.
* @param[in] bufferLength      size of the buffer
* @param[out] stringLengthPtr  length of the returned string 
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t externalunitinfo_getImplantType(
    const HExternalUnitInfo hExternalUnitInfo,
    capi_char_t* const typeStringPtr, const size_t bufferLength, size_t* const stringLengthPtr);

/**
* Get device identifier of the external unit.
*
* @param[in] hExternalUnitInfo Handle to external unit info (must be initialized).
* @param[out] deviceIdStringPtr Pointer to buffer, recommended buffer size is 128.
* @param[in] bufferLength       size of the buffer
* @param[out] stringLengthPtr   length of the returned string 
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t externalunitinfo_getDeviceId(
    const HExternalUnitInfo hExternalUnitInfo,
    capi_char_t* const deviceIdStringPtr, const size_t bufferLength, size_t* const stringLengthPtr);

/**
* Get firmware version of the external unit.
*
* @param[in] hExternalUnitInfo          Handle to external unit info (must be initialized).
* @param[out] firmwareVersionStringPtr  Pointer to buffer, recommended buffer size is 128.
* @param[in] bufferLength               size of the buffer
* @param[out] stringLengthPtr           length of the returned string 
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t externalunitinfo_getFirmwareVersion(
    const HExternalUnitInfo hExternalUnitInfo,
    capi_char_t* const firmwareVersionStringPtr, const size_t bufferLength, size_t* const stringLengthPtr);

/**
* Destroy external unit info handles.
*
* @param[in] externalUnitInfoVector Address of vector of external unit infos. Handle is set to null after destruction.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t externalunitinfos_destroy(externalunitinfovector_t* const externalUnitInfoVector);

#ifdef __cplusplus
}
#endif

#endif // CAPI_EXTERNALUNITINFO_H