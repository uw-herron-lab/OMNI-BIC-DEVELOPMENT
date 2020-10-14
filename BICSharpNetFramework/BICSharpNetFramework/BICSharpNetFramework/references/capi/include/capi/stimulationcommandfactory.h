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
#ifndef CAPI_STIMULATIONCOMMANDFACTORY_H
#define CAPI_STIMULATIONCOMMANDFACTORY_H

#include <capi/capi.h>
#include <capi/stimulationcommand.h>
#include <capi/stimulationfunction.h>
#include <capi/stimulationatom.h>

/**
* @file stimulationcommandfactory.h
* @brief Factory interface for creation of stimulation commands, functions and atoms.
*
* Typical usage:
* 1. Create empty stimulation command.
* 2. Create empty stimulation function. Set repetition parameter.
* 3. Create stimulation atoms and append them to function.
* 4. Append stimulation function to command.
* 5. Repeat steps 2. - 4. until all functions are created.
*
* @see stimulationcommand.h
* @see stimulationfunction.h
* @see stimulationatom.h
*/

typedef struct _HIStimulationFactory* HIStimulationFactory; ///< opaque type for passing IStimulationCommandFactory

#ifdef __cplusplus
extern "C" {
#endif

/**
* Get the handle to a stimulation command factory.
*
* @param[out] hStimCommandFactory Handle to stimulation command factory.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationcommandfactory_getFactoryHandle(HIStimulationFactory* hStimCommandFactory);

/**
* Creates an empty stimulation command.
*
* Note: The stimulation command must be destroyed via stimulationcommand_destroy (@see stimulationcommand.h)
*       once it is not longer needed.
*
* @param[in] hStimCommandFactory Handle to stimulation command factory (must be initialized).
* @param[out] stimulationCommand Empty stimulation command. Caller is responsible for deletion of returned pointer.
*
* @return Indicator for successful code execution (TATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationcommandfactory_createStimulationCommand(
    HIStimulationFactory hStimCommandFactory, 
    HIStimulationCommand* stimulationCommand);

/**
* Creates an empty stimulation function.
*
* Note: The caller must take care for destruction of the stimulation function once it is not longer needed. E.g. via 
*       stimulationfunction_destroy (@see stimulationfunction.h) or by passing the function to a stimulation command, 
*       which takes care of the destruction of its functions.
*
* @param[in] hStimCommandFactory  Handle to stimulation command factory (must be initialized).
* @param[out] stimulationFunction Empty stimulation function with repetition count 1. Caller is
* responsible for deletion of returned pointer.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationcommandfactory_createStimulationFunction(
    HIStimulationFactory hStimCommandFactory, 
    HIStimulationFunction* stimulationFunction);

/**
* Creates a stimulation atom. Each atom represents a constant amplitude for a fixed time duration.
*
* Note: 1. The caller must take care for destruction of the stimulation atom once it is not longer needed. E.g. via 
*          stimulationatom_destroy (@see stimulationatom.h) or by passing the function to a stimulation function, 
*          which takes care of the destruction of its atoms.
*       2. Atoms of different types cannot be appended to the same stimulation function
*
* Consider an implant that does voltage stimulation. Then createRectStimulationAtom(12, 2000000) would return
* a stimulation atom that stimulates for 2 seconds with an amplitude of 12 Volts. However, if this atom would
* be sent to an implant that does current stimulation, this atom would elicit a stimulation with 12
* Milliampere for 2 seconds.
*
* @param[in] hStimCommandFactory Handle to stimulation command factory (must be initialized).
* @param[out] stimulationAtom    Rectangular stimulation atom.
* @param[in] value               Stimulation amplitude, either in V or mA.
* @param[in] duration            Duration in micros of the stimulation.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationcommandfactory_createRectStimulationAtom(
    HIStimulationFactory hStimCommandFactory, 
    HIStimulationAtom* stimulationAtom, 
    const double value, 
    const uint64_t duration);

/**
* Creates a stimulation atom with four different amplitudes. Each atom represents four constant
* amplitudes for a fixed time duration.
*
* Note: 1. The caller must take care for its destruction of the stimulation atom once it is not longer needed. E.g. via 
*          stimulationatom_destroy (@see stimulationatom.h) or by passing the function to a stimulation function, 
*          which takes care of the destruction of its atoms.
*       2. Atoms of different types cannot be appended to the same stimultion function
*
* @param[in] hStimCommandFactory Handle to stimulation command factory (must be initialized).
* @param[out] stimulationAtom    4Rectangular stimulation atom.
* @param[in] amplitude0          First stimulation amplitude, either in V or mA.
* @param[in] amplitude1          Second stimulation amplitude, either in V or mA.
* @param[in] amplitude2          Third stimulation amplitude, either in V or mA.
* @param[in] amplitude3          Fourth stimulation amplitude, either in V or mA.
* @param[in] duration            Duration in micros of the stimulation.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationcommandfactory_create4RectStimulationAtom(
    HIStimulationFactory hStimCommandFactory, 
    HIStimulationAtom* stimulationAtom,
    const double amplitude0, 
    const double amplitude1, 
    const double amplitude2, 
    const double amplitude3, 
    const uint64_t duration);

/**
* Creates a stimulation pause atom with a given duration (no amplitudes)
*
* Note: 1. The caller must take care for its destruction of the stimulation atom once it is not longer needed. E.g. via 
*          stimulationatom_destroy (@see stimulationatom.h) or by passing the function to a stimulation function, 
*          which takes care of the destruction of its atoms.
*
* @param[in] hStimCommandFactory Handle to stimulation command factory (must be initialized).
* @param[out] stimulationAtom    Stimulation pause atom.
* @param[in] duration            Duration in micros of the stimulation.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t stimulationcommandfactory_createStimulationPauseAtom(
    HIStimulationFactory hStimCommandFactory, 
    HIStimulationAtom* stimulationAtom,
    const uint64_t duration);

#ifdef __cplusplus
}
#endif

#endif // CAPI_STIMULATIONCOMMANDFACTORY_H