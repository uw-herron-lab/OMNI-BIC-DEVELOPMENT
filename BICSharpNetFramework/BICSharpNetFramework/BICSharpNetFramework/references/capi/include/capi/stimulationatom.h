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
#ifndef CAPI_STIMULATIONATOM_H
#define CAPI_STIMULATIONATOM_H

#include <capi/capi.h>

/**
* @file stimulationatom.h
* @brief Atomic element of a stimulation function.
*
* Each atom has a duration which is defined in micros and a type (atom_type_t) that determines its 
* form. Atoms can be appended to a stimulation function to define the function's signal form over time.
*
* @see IStimulationFunction.
*/

/**
* Enumeration with all available stimulation atom types.
*/
typedef enum
{
    AT_NOTYPE,      ///< Type representing no type
    AT_RECTANGULAR, ///< Rectangular shaped atoms, @see CRectangularStimulationAtom 
    /// Rectangular shaped atoms with multiple amplitudes, @see CRectangular4AmplitudeStimulationAtom
    AT_RECTANGULAR_4_AMPLITUDE, 
    AT_PAUSE,       ///< Atom that represents a pause, i.e. no stimulation, @see CPauseStimulationAtom
    AT_UNSUPPORTED, ///< default value
    AT_COUNT        ///< number of atom types
} atom_type_t;

typedef struct _HIStimulationAtom* HIStimulationAtom; ///< opaque type for passing IStimulationAtom
typedef struct _HIStimulationAtomIterator* HIStimulationAtomIterator; ///< opaque type for passing IStimulationAtom iter


#ifdef __cplusplus
extern "C" {
#endif

/**
* Get the duration of a stimulation atom.
*
* @param [in] hStimulationAtom Handle to stimulation atom.
* @param [out] duration        Duration in micros of the stimulation atom.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationatom_getDuration(
    const HIStimulationAtom  hStimulationAtom,
    uint64_t* const duration);

/**
* Get the type of a stimulation atom.
*
* @param [in] hStimulationAtom Handle to stimulation atom.
* @param [out] atomType        Type of the stimulation atom.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationatom_getType(
    const HIStimulationAtom  hStimulationAtom,
    atom_type_t* const atomType);

/**
* Create a deep copy of the atom. 
*
* Note: The caller is responsible for deletion either by calling stimulationatom_destroy or by adding it to a function 
*       (ownership passed to function).
*
* @param [in] hStimulationAtom       Handle to stimulation atom.
* @param [out] hStimulationAtomClone Handle to identical copy of the atom.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationatom_clone(
    const HIStimulationAtom  hStimulationAtom,
    HIStimulationAtom* const hStimulationAtomClone);

/**
* Compare two stimulation atoms.
*
* @param [in] hStimulationAtom      Handle to stimulation atom.
* @param [in] hOtherStimulationAtom Handle to other stimulation atom.
* @param [out] result               True if the other atom defines the same stimulation signal.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationatom_isEqual(
    const HIStimulationAtom  hStimulationAtom,
    const HIStimulationAtom  hOtherStimulationAtom,
    bool* const result);

/**
* Destroy stimulation atom handle.
*
* Note: Use this method only if the atom has not been added to a stimulation function,
*       since the function will already take care of its destruction.
*
* @param[in] hStimulationAtom Address of handle to stimulation function to be destroyed.
*            The handle is nulled after destruction.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationatom_destroy(HIStimulationAtom * const hStimulationAtom);

#ifdef __cplusplus
}
#endif

#endif // CAPI_STIMULATIONATOM_H