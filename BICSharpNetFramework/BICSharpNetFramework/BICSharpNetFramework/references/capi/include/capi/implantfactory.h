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
#ifndef CAPI_IMPLANTFACTORY_H
#define CAPI_IMPLANTFACTORY_H

#include <capi/capi.h>
#include <capi/implant.h>
#include <capi/externalunitinfo.h>
#include <capi/implantinfo.h>

/**
* @file implantfactory.h
* @brief Device discovery and factory interface creating implant handles to control implant devices
*
* Main responsiblities of this component:
* - discovering all external units and implants connected to the system which are recognized by the factory
* - creating an instance of a suitable hardware abstraction layer implementation for a specified
*   external unit and implant combination.
*/

typedef struct _HImplantFactory* HImplantFactory; ///< opaque type for passing IImplantFactory*


#ifdef __cplusplus
extern "C" {
#endif

/**
* Initialize library. Must be called before calling any other library function.
* If enable logging is turned on, all events (e.g. measurement channel data, implant status, etc.) received from the 
* implant are logged into a binary log file.
*
* @param[in] enableLogging Set to true to enable logging, false otherwise
* @param[in] fileNamePtr   Path of the log file. The c string array must be zero terminated. 
*                 Any existing file with this name will be replaced (param may be nullptr if enableLogging is false)
* @param[in] len           string length of filename
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantfactory_init(const bool enableLogging, 
    const capi_char_t* const fileNamePtr, const size_t len);

/**
* Get the handle to an implant factory.
* 
* @param[out] hImplantFactory Handle to implant factory.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantfactory_getFactoryHandle(HImplantFactory* const hImplantFactory);

/**
* Method for discovering the available external units connected to the system.
*
* Note: The ownership of exteral unit infos is passed to the callee and must be destroyed once they are
* no longer needed. (cp. externalunitinfo_destroy @see externalunitinfo.h)
* 
* @param[in] hImplantFactory         Handle to implant factory (must be initialized).
* @param[out] externalUnitInfoVector Will contain the list of all connected external units,
*                                    empty list if no external units are currently connected.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantfactory_getExternalUnitInfos(
    HImplantFactory hImplantFactory, 
    externalunitinfovector_t* const externalUnitInfoVector);

/**
* Method for discovering the connected implant of an external unit.
* 
* Note: The ownership of implant info is passed to the callee and must be destroyed once it is not longer needed.
*       (cp. implantinfo_destroy @see implantinfo.h)
*
* @param[in] hImplantFactory  Handle to implant factory (must be initialized).
* @param[in] externalUnitInfo External unit information of the device of interest.
* @param[out] hImplantInfo    Handle to implant info with details on the connected implant.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantfactory_getImplantInfo(
    HImplantFactory  hImplantFactory, 
    const HExternalUnitInfo externalUnitInfo, 
    HImplantInfo* const hImplantInfo);

/**
* Determine if the factory is responsible for a certain external unit.
*
* @param[in] hImplantFactory       Handle to implant factory (must be initialized).
* @param[in] externalUnitInfo      Information describing the external unit.
* @param[out] isResponsibleFactory Set to true, if the factory can handle the type of the external unit, false otherwise.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantfactory_isResponsibleFactory(
    HImplantFactory  hImplantFactory, 
    const HExternalUnitInfo externalUnitInfo, 
    bool* const isResponsibleFactory);

/**
* Creates an initialized implant handle for the device combination specified by implantId and externalUnitId.
*
* Note: 1. The implant handle must be destroyed via implant_destroy (@see implant.h)
*          once it is not longer needed.
*       2. Once an implant has been created, the external unit id will no longer be listed in the list returned by
*          implantfactory_getExternalUnitInfos unless the implant handle is destroyed.
*
* @param[in] hImplantFactory  Handle to implant factory (must be initialized).
* @param[in] externalUnitInfo External unit information, which the device is connected to.
* @param[in] implantInfo      Implant information of the implant that needs to be instantiated.
* @param[out] hImplant        An instantiated and initialized implant.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implantfactory_create(
    HImplantFactory  hImplantFactory, 
    const HExternalUnitInfo externalUnitInfo, 
    const HImplantInfo  implantInfo, 
    HImplant* hImplant);


#ifdef __cplusplus
}
#endif

#endif // CAPI_IMPLANTFACTORY_H