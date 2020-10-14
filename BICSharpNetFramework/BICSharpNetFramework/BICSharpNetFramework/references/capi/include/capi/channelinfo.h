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
#ifndef CAPI_CHANNELINFO_H
#define CAPI_CHANNELINFO_H

#include <capi/capi.h>

/**
* @file channelinfo.h
* @brief Information on a single channel of an electrode.
*/
struct _HChannelInfo;
typedef const struct _HChannelInfo* HChannelInfo;    ///< opaque type for passing CChannelInfo*
/**
* @brief Enumeration for unit types.
*/
typedef enum _unit_type
{
    UT_NO_UNIT = 0,
    UT_CURRENT,
    UT_VOLTAGE,
    UT_COUNT // number of units
} unit_type_t;

/**
  * @brief Vector of channel information.
  *        
  * Each entry provides information on the specific electrode channel.
  */
typedef struct _ChannelInfoVector
{
    size_t count;           ///< number of channel information elements inside the vector array
    HChannelInfo* vector;  ///< array of channel info handles 
} channelinfovector_t;


#ifdef __cplusplus
extern "C" {
#endif

/**
* Check if the electrode is capable of measure electrical signals.
*
* @param[in] hChannelInfo Handle to channel info (must be initialized).
* @param[out] canMeasure  Will be set to true if the electrode is capable of measure electrical signals.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t channelinfo_canMeasure(const HChannelInfo hChannelInfo, bool* const canMeasure);

/**
* Get the minimum value in voltage [V] the electrode can measure.
*
* @param[in] hChannelInfo Handle to channel info (must be initialized).
* @param[out] value The value measured by the electrode.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t channelinfo_getMeasureValueMin(const HChannelInfo hChannelInfo, double* const value);

/**
* Get the maximum value in voltage [V] the electrode can measure.
*
* @param[in] hChannelInfo Handle to channel info (must be initialized).
* @param[out] value       The value measured by the electrode.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t channelinfo_getMeasureValueMax(const HChannelInfo hChannelInfo, double* const value);

/**
* Check if the electrode is capable of stimulation.
*
* @param[in] hChannelInfo  Handle to channel info (must be initialized).
* @param[out] canStimulate Will be set to true if the electrode is capable of stimulation.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t channelinfo_canStimulate(const HChannelInfo hChannelInfo, bool* const canStimulate);

/**
* Get the unit type of the stimulation value.
*
* @param[in] hChannelInfo     Handle to channel info (must be initialized).
* @param[out] stimulationUnit Will be set to the unit type of the stimulation value.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t channelinfo_getStimulationUnit(
    const HChannelInfo hChannelInfo,
    unit_type_t* const stimulationUnit);

/**
* Get the minimum value in UnitType the electrode can emit for stimulation.
*
* @param[in] hChannelInfo Handle to channel info (must be initialized).
* @param[out] value       Will be set to the minimum value in UnitType the electrode can emit for stimulation.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t channelinfo_getStimValueMin(const HChannelInfo hChannelInfo, double* const value);

/**
* Get the maximum value in UnitType the electrode can emit for stimulation.
*
* @param[in] hChannelInfo Handle to channel info (must be initialized).
* @param[out] value       Will be set to the maximum value in UnitType the electrode can emit for stimulation.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t channelinfo_getStimValueMax(const HChannelInfo hChannelInfo, double* const value);

/**
* Check if the electrode is capable of measuring it's impedance.
*
* @param[in] hChannelInfo         Handle to channel info (must be initialized).
* @param[out] canMeasureImpedance Will be set to true if the electrode is capable of measuring it's impedance.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t channelinfo_canMeasureImpedance(
    const HChannelInfo hChannelInfo,
    bool* const canMeasureImpedance);

#ifdef __cplusplus
}
#endif

#endif // CAPI_CHANNELINFO_H