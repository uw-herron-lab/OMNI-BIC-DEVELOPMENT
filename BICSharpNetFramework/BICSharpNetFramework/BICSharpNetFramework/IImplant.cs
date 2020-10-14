using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace BICSharpNetFramework
{
    class IImplant
    {            
        /**
          * Register listener object, which is notified on arrival of new data and errors. Needs to be called
          * once before starting the measurement loop. On consecutive calls only the latest registered listener
          * will be notified. If listener == nullptr, listener will be deregistered.         
          */
        public void registerListener(IImplantListener* listener) = 0;

        /**
        * @return informations about the implant. The ownership of CImplantInfo is passed to the caller.
        */
        public CImplantInfo getImplantInfo();

        /**
        * Starts measurement of data. Note that only the measurement result of the channels that are not used as reference
        * electrodes is valid. The measurement result for the reference electrodes is the reference value.
        *
        * @param refChannels List of channel indices that are used as composite reference electrode. Can be empty.
        *
        * \if INTERN_IMPLANTAPI
        *   @throws CRuntimeException if the measurement is started already
        * \else
        *   @throws std::exception if the measurement is started already
        * \endif
        */
        public void startMeasurement(const std::set<uint32_t>& refChannels = {}) = 0;

        /**
        * Stops measurement of data. 
        */
        public void stopMeasurement() = 0;

        /**
        * Starts impedance measurement of one channel. This is a blocking call. Impedance measurement is only possible
        * while no measurement or stimulation is running.
        *
        * @param[in] channel number of the channel, whose impedance should be measured. Number starts with 0.
        * @return impedance in Ohm.
        */
        virtual double getImpedance(const uint32_t channel) = 0;

        /**
        * Measures the temperature within implant capsule. This is a blocking call. It will
        * stop running measurements.
        *
        * @return temperature in degree Celsius.
        */
        virtual double getTemperature() = 0;
        
        /**
        * Measures the humidity within implant capsule. This is a blocking call. It will
        * stop running measurements.
        *
        * @return humidity in %rh.
        */
        virtual double getHumidity() = 0;

        /**
        * Starts a stimulation. This is a non-blocking call. 
        * A stimulation can be stopped by sending a stimulation command with i1 = i2 = 0mA or 0V respectively.
        *
        * @param[in] cmd the stimulation command that needs to be executed. The ownership is passed to the IImplant 
        *                instance.
        * \if INTERN_IMPLANTAPI
        *   @throws CInvalidArgumentException if cmd is nullptr
        *   @throws CRuntimeException if stimulation command cannot be sent to implant, e.g. because it is currently  
        *                             disconnected.
        * \else
        *   @throws std::exception if cmd is nullptr
        *   @throws std::exception if stimulation command cannot be sent to implant, e.g. because it is currently 
        *                          disconnected.
        * \endif
        */
        virtual void startStimulation(IStimulationCommand* cmd) = 0;

		/**
		* Stops stimulation.
        *
        * \if INTERN_IMPLANTAPI
        *   @throws CRuntimeException if stimulation command cannot be sent to implant, e.g. because it is currently
        *                             disconnected.
        * \else
        *   @throws std::exception if stimulation command cannot be sent to implant, e.g. because it is currently
        *                          disconnected.
        * \endif
		*/
		virtual void stopStimulation() = 0;

        /**
        * Enable or disable power transfer to the implant (enabled by default). Note that powering on the
        * implant takes some milliseconds before it is responsive.
        * @param[in] enabled if true the power transfer to the implant is initiated else it is stopped
        * \if INTERN_IMPLANTAPI
        *   @throws CTimeoutException if timeout
        *   @throws CRuntimeException if invalid response.
        * \else
        *   @throws std::exception if timeout
        *   @throws std::exception if invalid response.
        * \endif
        */
        virtual void setImplantPower(const bool enabled) = 0;

        /**
        * Tells the implant to propagate the current measurement and stimulation states through the registered
        * listener.
        *
        * This method can be used to synchronize GUIs with the implant's state (i.e., is it currently measuring or
        * stimulating or both?).
        *
        * Usage:
        * 1. register listener
        * 2. call pushState()
        * 3. update GUI based on callback from listener (onMeasurementStateChanged()/onStimulationStateChanged())
        *
        * \if INTERN_IMPLANTAPI
        *   @throws CTimeoutException if timeout
        *   @throws CRuntimeException if invalid response.
        * \else
        *   @throws std::exception if timeout
        *   @throws std::exception if invalid response.
        * \endif
        */
        virtual void pushState() = 0;
    }
}
