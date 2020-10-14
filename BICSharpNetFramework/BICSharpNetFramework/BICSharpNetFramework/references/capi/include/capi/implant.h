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
#ifndef CAPI_IMPLANT_H
#define CAPI_IMPLANT_H

#include <capi/capi.h>
#include <capi/implantinfo.h>
#include <capi/stimulationcommand.h>

/**
* @file implant.h
* @brief Interface for an implant.
* 
* Generic implant interface for all kinds of implants. This interface acts as a facade to
* the hardware abstraction layer. Stimulation is triggered as an stimulation command, which is checked for compliance
* with the corresponding implant stimulation constraints and processed if it satisfies the requirements.
*
* All module functions return STATUS_RUNTIME_ERROR if the execution of the commands fails (e.g. missing connection).
*/

                               
typedef struct _HImplant* HImplant;                  ///< opaque type for passing IImplant*
typedef struct _HImplantListener* HImplantListener;        ///< opaque type for passing IImplantListener*

#define SAMPLE_NO_STIMULATION      0 ///< sample_t.stimulationId value for "no stimulation started with this sample"

/**
* @brief Enumeration for different connections. 
*
* Note that not all implants do have all connection types.
*/
typedef enum _connection_type
{
    CON_TYPE_EXT_TO_IMPLANT = 0,    ///< Connection between external unit and implant.
    CON_TYPE_PC_TO_EXT = 1,         ///< Connection between PC and external unit.
    CON_TYPE_UNKNOWN = 2            ///< Connection of unknown type.
} connection_type_t;

/**
* @brief Enumeration for the state of a connection, e.g., connected or disconnected.
*/
typedef enum _connection_state
{    
    CON_STATE_DISCONNECTED = 0, ///< Represents the state 'disconnected'.
    CON_STATE_CONNECTED = 1,    ///< Represents the state 'connected'.
    CON_STATE_UNKNOWN = 2       ///< Represents the state 'unknown'.
} connection_state_t;

/**
* @brief Measurement data read by the implant at one point in time, i.e. for one sample.
*/
typedef struct _sample
{
    /** Number of channels used for measuring. Array measurements must be of this size. */
    uint16_t numberOfMeasurements;  
    
    /** 
    * Array of measurements. This array must be of size numberOfMeasurements. 
    */
    double* measurements; 

    /** Supply voltage in mV. */
    uint32_t supplyVoltage; 

    /**  True if the implant is connected (all channels available). */
    bool isConnected; 

    /**
    * Id of the stimulation which starts with this sample. If no stimulation started with this sample, then the
    * stimulationNumber is IMPLANT_NO_STIMULATION.
    */
    uint16_t stimulationId; 

    /** True if the stimulation is started. */
    bool isStimulationActive;

    /** 
    * Counter that is increased for each measurement sample starting with 0.
    * The value range is [0, 4294967295] (i.e. 2^32 - 1). If the maximum value
    * is exceeded the counter will be reset automatically.
    */
    uint32_t measurementCounter;
} sample_t;

/**
*  @brief Callback functions which are called by the implant to notify the user on certain events.
*
* For example, new measurement data, implant state changes or implant health state changes from an implant.
* It is possible to initialize single handles as NULL before creating the listener handle in order to not receive
* any events of a certain type. If the callbacks are not NULL they must point to callable functions!
* Please note that ignoring the onError event is not advisible for medical devices, since error conditions could be
* lost.
*/
typedef struct _implantlistener
{
    /**
    * Callback receiving stimulation state changes.
    *
    * @param[in] isStimulating True if stimulation is in progress, false otherwise
    */ 
    void(*onStimulationStateChanged)(const bool isStimulating);

    /**
    * Callback receiving measurement state changes.
    *
    * @param[in] isMeasuring True if measurement loop is running, false otherwise
    */
    void(*onMeasurementStateChanged)(const bool isMeasuring);

    /**
    * Callback receiving connection state changes.
    *
    * @param[in] connectionType  Information on which connection state changed.
    * @param[in] connectionState The new connection state.
    */  
    void(*onConnectionStateChanged) (
        const connection_type_t connectionType, 
        const connection_state_t connectionState);

    /**
    * Callback receiving measurement data.
    *
    * Important: Once the callback returns the pointed sample is no longer valid!
    *
    * @param[in] sample Measurement data.
    */
    void(*onData)                   (const sample_t* const sample);

    /**
    * Callback if a new supply voltage information was received from the implant.
    *
    * @param[in] voltageMicroV Voltage of implant in microvolt.
    */
    void (*onImplantVoltageChanged)     (const double voltageMicroV);

    /**
    * Callback if a new primary coil current value was received from the implant.
    *
    * @param[in] currentMilliA Current of implant in milliampere.
    */
    void (*onPrimaryCoilCurrentChanged) (const double currentMilliA);

    /**
    * Callback if implant control value change was received from the external unit.
    *
    * @param[in] controlValue Control value in [0..100], where 0 is minimum and 
                 100 is maximum.
    */
    void (*onImplantControlValueChanged)(const double controlValue);
    
    /**
    * Callback if a new temperature information was received from the implant.
    *
    * @param[in] temperature Temperature of implant in degree Celsius.
    */
    void(*onTemperatureChanged)     (const double temperature);

    /**
    * Callback if a new humidity information was received from the implant.
    *
    * @param[in] humidity Relative humidity of implant in percent.
    */
    void(*onHumidityChanged)        (const double humidity);

    /**
    * Callback for errors in driver library during measurement and stimulation.
    * Examples:
    * - Humidity exceeds safety limit and leads to critical shutdown of the implant
    * - Stimulation current exceeds safety limits potentially due to electrical malfunctions
    *
    * Important: Once the callback returns, the errorDescription is no longer valid!
    *
    * @param errorDescription The error message
    */
    void(*onError)                  (const char* errorDescription);

    /**
    * Callback when onData calls are processed too slow.
    */
    void(*onDataProcessingTooSlow)   ();

} implantlistener_t;


#ifdef __cplusplus
extern "C" {
#endif

/**
* Create listener handle which can be used to receive implant events. 
* 
* Note: The ownership of the listener is passed to the callee and must be destroyed once it is not longer needed.
*       (cp. implant_destroyListener)
*
* @param[in] listener          implantlistener_t struct with initialized callback function pointers.
* @param[out] hImplantListener Instantiated listener handle (return value must be checked!)
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_createListener(
    const implantlistener_t* const listener, 
    HImplantListener* const hImplantListener);

/**
* Destroy implant listener handles.
*
* Note: Never destroy a listener handle that still is registered to implant (cp. implant_unregisterListener)
* 
* @param[in] hImplantListener Listener handle to be destroyed. The handle is nulled after destruction.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_destroyListener(HImplantListener* const hImplantListener);

/**
* Register listener object, which is notified on arrival of new data and errors. 
*
* The function needs to be called once before starting the measurement loop in order to receive measurement values. 
* On consecutive calls only the latest registered listener will be notified.
*
* @param[in] hImplant         Initialized handle to an implant.
* @param[in] hImplantListener Instantiated listener handle to be registered on the implant
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_registerListener(HImplant hImplant, HImplantListener  hImplantListener);


/**
* Unregister listener object from an implant handle.
*
* @param[in] hImplant Initialized handle to an implant.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_unregisterListener(HImplant const hImplant);

/**
* Get information about the implant.
* 
* Note: The ownership is passed to the caller and should be destroyed with implantinfo_destroy if it is not longer
*       required.
*
* @param[in] hImplant      Initialized handle to an implant.
* @param[out] hImplantInfo Initialized handle to implant info (@see implantinfo.h). 
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_getImplantInfo(HImplant const hImplant,  HImplantInfo* const hImplantInfo);

/**
* Starts measurement of data. Note that only the measurement result of the channels that are not used as reference
* electrodes is valid. The measurement result for the reference electrodes is the reference value.
*
* @param[in] hImplant    Initialized handle to an implant.
* @param[in] refChannels List of channel indices that are used as composite reference electrode. Can be empty.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_startMeasurement(HImplant const hImplant, const capi_uint32_set_t refChannels);

/**
* Stops measurement of data.
*
* @param[in] hImplant Initialized handle to an implant.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_stopMeasurement(HImplant const hImplant);

/**
* Starts impedance measurement of one channel. This is a blocking call. Impedance measurement is only possible
* while no measurement or stimulation is running.
*
* @param[in] hImplant Initialized handle to an implant.
* @param[in] channel  Number of the channel, whose impedance should be measured. Number starts with 0.
* @param[out] value   Impedance in ohm.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_getImpedance(
    HImplant const hImplant,
    const uint32_t channel, 
    double* const value);

/**
* Measures the temperature within implant capsule. This is a blocking call. It will
* stop running measurements.
*
* @param[in] hImplant Initialized handle to an implant.
* @param[out] value   Temperature in degree Celsius.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_getTemperature(HImplant const hImplant, double* const value);

/**
* Measures the humidity within implant capsule. This is a blocking call. It will stop running measurements. 
*
* @param[in] hImplant Initialized handle to an implant.
* @param[out] value   Humidity in %rh.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_getHumidity(HImplant const hImplant, double* const value);

/**
* Starts a stimulation. This is a non-blocking call (i.e. the call will schedule the stimulation but not wait
* until it is finished). The command pointer ownership is passed to the implant handle.
*
* Note: The implant takes ownership of the command and destroys it automatically at the end of its lifecycle.
*
* @param[in] hImplant            Initialized handle to an implant.
* @param[in] hStimulationCommand Initialized and configured stimulation command handle (@see stimulationcommandfactory.h)
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_startStimulation(
    HImplant const hImplant,
    HIStimulationCommand  hStimulationCommand);

/**
* Stops stimulation.
*
* @param[in] hImplant Initialized handle to an implant.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_stopStimulation(HImplant const hImplant);

/**
* Enable or disable power transfer to the implant (enabled by default). Note that powering on the
* implant takes some milliseconds before it is responsive.
*
* @param[in] hImplant Initialized handle to an implant.
* @param[in] enabled  If true the power transfer to the implant is initiated else it is stopped.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_setImplantPower(HImplant const hImplant, const bool enabled);

/**
* Tells the implant to propagate the current measurement and stimulation states through the registered
* listener. 
*
* This method can be used to synchronize GUIs with the implant's state. 
*
* Usage:
* 1. register listener
* 2. call pushState()
* 3. update GUI based on callback from listener (onMeasurementStateChanged()/onStimulationStateChanged())
*
* @param[in] hImplant Initialized handle to an implant.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_pushState(HImplant const hImplant);

/**
* Destroy implant handles.
*
* @param[in] hImplant Handle to the implant instance, which shall be destroyed.
*
* @return Indicator for successful code execution (STATUS_OK on success)
*/
CAPI_DLL capi_status_t implant_destroy(HImplant* const hImplant);


#ifdef __cplusplus
}
#endif

#endif // CAPI_IMPLANT_H