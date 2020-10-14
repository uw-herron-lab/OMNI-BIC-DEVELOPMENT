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
#ifndef CAPI_CAPI_H
#define CAPI_CAPI_H

// dll export import helper macro
#ifdef BUILD_CAPI_DLL
#define CAPI_DLL    __declspec(dllexport)
#else
#define CAPI_DLL    __declspec(dllimport)
#endif

#include <stdint.h>
#include <stdbool.h>

/**
* @brief Enumeration for return status of all C-api commands.
*/
typedef enum _capi_status
{
    STATUS_OK,
    STATUS_NULL_POINTER_ERROR,  ///< a null pointer was passed in the arguments
    STATUS_RUNTIME_ERROR,       ///< an exception occured, call capi_getErrorMessage for more information
    STATUS_STRING_TOO_LONG,     ///< the passed string object cannot hold all data
    STATUS_VECTOR_TOO_LONG,     ///< the passed vector object cannot hold all data
    STATUS_SET_TOO_BIG,         ///< the passed set object cannot hold all the data
    STATUS_COUNT
} capi_status_t;

/**
* @brief type used for string data.
*/
typedef char capi_char_t;

/**
* @brief Structure for passing a set of uint32_t elements.
*/
typedef struct _capi_uint32Set
{
    size_t size;        ///< size of element array
    uint32_t* elements; ///< address of array for elements
} capi_uint32_set_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
* Get further information on last error. Only meaningful if the last return code was STATUS_RUNTIME_ERROR.
*
* @param[out] messagePtr           Pointer to buffer, recommended buffer size is 128.
* @param[in] bufferLength          size of the buffer
* @param[out] stringLengthPtr      length of the returned string 
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t capi_getErrorMessage(
    capi_char_t* const messagePtr, const size_t bufferLength, size_t* const stringLengthPtr);

/**
* Get version string of the C-api.
*
* @param[out] versionPtr           Pointer to buffer, recommended buffer size is 128.
* @param[in] bufferLength          size of the buffer
* @param[out] stringLengthPtr      length of the returned string 
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t capi_getLibraryVersion(
    capi_char_t* const versionPtr, const size_t bufferLength, size_t* const stringLengthPtr);


#ifdef __cplusplus
}
#endif

#endif // CAPI_CAPI_H