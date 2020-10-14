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
#ifndef IMPLANTAPI_IIMPLANT_LISTENER_H
#define IMPLANTAPI_IIMPLANT_LISTENER_H
#include <cppapi/ConnectionInfo.h>
#include <cppapi/Sample.h>
#include <vector>


namespace cortec { namespace implantapi {
  
    /**
    * @brief Interface for receiving measurement data, implant state changes or implant health state changes from an
    * IImplant.
    */
    class IImplantListener
    {
    public:
        virtual ~IImplantListener() {}

        /**
          * Callback receiving stimulation state changes.
          * @param[in] isStimulating true if stimulation is in progress, false otherwise
          */
        virtual void onStimulationStateChanged(const bool isStimulating) = 0;

        /**
          * Callback receiving measurement state changes.
          * @param[in] isMeasuring true if measurement loop is running, false otherwise
          */
        virtual void onMeasurementStateChanged(const bool isMeasuring) = 0;

        /**
        * Callback receiving connection state changes.
        * @param[in] info contains connection state for each connection.
        */
        virtual void onConnectionStateChanged(const connection_info_t& info) = 0;

        /**
        * Callback receiving measurement data,
        *
        * @param samples vector of samples. First samples is the oldest sample, second sample is the second oldest and 
        *        so on... The vector's pointer ownership is passed to the listener and needs to be deleted after 
        *        processing.
        */
        virtual void onData(const std::vector<CSample>* samples) = 0;

        /**
        * Callback triggered when new supply voltage value is received from the implant.
        *
        * @param[in] voltageMicroV Voltage of implant in microvolt.
        */
        virtual void onImplantVoltageChanged(const double voltageMicroV) = 0;

        /**
        * Callback triggered when new primary coil current value is received from the external unit.
        * The primary coil refers to the coil inside the head peace of the external unit.
        *
        * @param[in] currentMilliA coil current in milli amps
        */
        virtual void onPrimaryCoilCurrentChanged(const double currentMilliA) = 0;

        /**
        * Callback triggered when new current control value is received from the external unit.
        * The power of the implant is controlled by the external unit. The control value
        * provides a measure of how good the coupling between the two coils is and how much
        * more power can be provided if necessary.
        *
        * @param[in] controlValue The value is between 0.0 and 100.0 percent, where 0.0 translates
        *            to no power and 100.0 translates to maximum power applied.
        */
        virtual void onImplantControlValueChanged(const double controlValue) = 0;

        /**
        * Callback triggered when new temperature value is received from the implant.
        *
        * @param[in] temperature Temperature of implant in degree Celsius.
        */
        virtual void onTemperatureChanged(const double temperature) = 0;

        /**
        * Callback when new humidity value is received from the implant.
        *
        * @param[in] humidity Humidity of implant in microvolt.
        */
        virtual void onHumidityChanged(const double humidity) = 0;

        /**
        * Callback triggered when errors inside driver library occure during measurement and stimulation.
        * Examples:
        * - another implant is attached at runtime
        * - another external unit is attached at runtime
        *
        * @param err the error message
        */
        virtual void onError(const std::exception& err) = 0;

        /**
        * Callback when the onData calls are processed too slow.
        */
        virtual void onDataProcessingTooSlow() = 0;
    };
}}


#endif //IMPLANTAPI_IIMPLANT_LISTENER_H