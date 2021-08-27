#include "BICListener.h"

// BIC Usings
using namespace cortec::implantapi;

// GRPC Usings
using BICgRPC::TemperatureUpdate;
using BICgRPC::HumidityUpdate;
using BICgRPC::NeuralUpdate;
using BICgRPC::PowerUpdate;
using BICgRPC::ConnectionUpdate;
using BICgRPC::ErrorUpdate;
using BICgRPC::NeuralSample;

namespace BICGRPCHelperNamespace
{
    /// <summary>
    /// Event Handler for Brain Interchange stimulation state change events
    /// </summary>
    /// <param name="isStimulating">Boolean indicates if stimulation is active or not</param>
    void BICListener::onStimulationStateChanged(const bool isStimulating)
    {
        // Write Event Information to Console
        std::cout << "\tDEBUG: Stimulation state changed: " << isStimulating << std::endl;

        // Grab a local-scoped lock before updating multi-threaded accessable state variables
        std::lock_guard<std::mutex> lock(m_mutex);

        // Update the value, local-scoped locked is released at end of function
        m_isStimulating = isStimulating;
    }

    /// <summary>
    /// Accessor for latest stimulation state value received from onStimulationStateHChanged event handler
    /// </summary>
    /// <returns>Boolean stimulation state</returns>
    bool BICListener::isStimulating()
    {
        // Grab a local-scoped lock before updating multi-threaded accessable state variables
        std::lock_guard<std::mutex> lock(m_mutex);

        // Return the value, local-scoped locked is released at end of function
        return m_isStimulating;
    }

    /// <summary>
    /// Event Handler for Brain Interchange measurement/sensing state change events
    /// </summary>
    /// <param name="isMeasuring">Boolean indicates if measurement/sensing is active or not</param>
    void BICListener::onMeasurementStateChanged(const bool isMeasuring)
    {
        // Write Event Information to Console
        std::cout << "\tDEBUG: Measurement state changed: " << isMeasuring << std::endl;

        // Grab a local-scoped lock before updating multi-threaded accessable state variables
        std::lock_guard<std::mutex> lock(m_mutex);

        // Update the value, local-scoped locked is released at end of function
        m_isMeasuring = isMeasuring;
    }

    /// <summary>
    /// Accessor for latest measurement/sensing state value received from onMeasurementStateChanged event handler
    /// </summary>
    /// <returns>Boolean measurement/sensing state</returns>
    bool BICListener::isMeasuring()
    {
        // Grab a local-scoped lock before updating multi-threaded accessable state variables
        std::lock_guard<std::mutex> lock(m_mutex);

        // Return the value, local-scoped locked is released at end of functione
        return m_isMeasuring;
    }

    /// <summary>
    /// Event handler for Brain Interchange connection state change events that include connection update information 
    /// </summary>
    /// <param name="info">Connection update information</param>
    void BICListener::onConnectionStateChanged(const connection_info_t& info)
    {
        // First, determine if connection update is between PC to external or if it is to external to INS.
        if (info.count(ConnectionType::PC_TO_EXT) > 0)
        {
            // Event refers to PC/External USB connection
            const bool isConnected = info.at(ConnectionType::PC_TO_EXT) == ConnectionState::CONNECTED;

            // Write out new state to the console
            std::cout << "*** Connection state from PC to external unit changed: "
                << (isConnected ? "connected" : "disconnected") << std::endl;

            // If gRPC connection streaming is enabled, write out the update
            if (ConnectionWriter != NULL)
            {
                ConnectionUpdate connectionMessage;
                connectionMessage.set_connectiontype("USB");
                connectionMessage.set_isconnected(isConnected);
                ConnectionWriter->Write(connectionMessage);
            }
        }
        if (info.count(ConnectionType::EXT_TO_IMPLANT) > 0)
        {
            // Event refers to External/INS inductive connection
            const bool isConnected = info.at(ConnectionType::EXT_TO_IMPLANT) == ConnectionState::CONNECTED;

            // Write out new state to the console
            std::cout << "*** Connection state from external unit to implant changed: "
                << (isConnected ? "connected" : "disconnected") << std::endl;

            // If gRPC connection streaming is enabled, write out the update
            if (ConnectionWriter != NULL)
            {
                ConnectionUpdate connectionMessage;
                connectionMessage.set_connectiontype("Inductive");
                connectionMessage.set_isconnected(isConnected);
                ConnectionWriter->Write(connectionMessage);
            }
        }
    }

    /// <summary>
    /// Event handler for Brain Interchange connection state change events that only include boolean connection update 
    /// </summary>
    /// <param name="isConnected">Boolean indicating if it is connected or not</param>
    void BICListener::onConnectionStateChanged(const bool isConnected)
    {
        // If gRPC connection streaming is enabled, write out the update
        if (ConnectionWriter != NULL)
        {
            ConnectionUpdate connectionMessage;
            connectionMessage.set_connectiontype("Overall");
            connectionMessage.set_isconnected(isConnected);
            ConnectionWriter->Write(connectionMessage);
        }
    }

    /// <summary>
    /// Event handler for Brain Interchange neural data received
    /// </summary>
    /// <param name="samples">BIC sensed LFP samples</param>
    void BICListener::onData(const std::vector<CSample>* samples)
    {
        // Ensure samples is not empty before working with it.
        if (!samples->empty())
        {
            // Create message container for streaming, may not be used
            for (int i = 0; i < samples->size(); i++)
            {
                // Copy in the time domain data in
                int32_t sampleCounter = samples->at(i).getMeasurementCounter();
                int16_t sampleNum = samples->at(i).getNumberOfMeasurements();
                double* theData = samples->at(i).getMeasurements();

                // Check if we've lost packets, if so interpolate
                if (lastNeuroCount + 1 != sampleCounter)
                {
                    if (lastNeuroCount == sampleCounter)
                    {
                        // Repeated Packet Count! 
                        // Write error message to server console
                        std::cout << ">>> Repeated packet counter value! <<<" << std::endl;
                    }
                    else
                    {
                        // Missed packet!
                        uint32_t diff = sampleCounter - (lastNeuroCount + 1);
                        if (diff < 0)
                        {
                            // Wrap around case
                            diff = (uint32_t)sampleCounter + (UINT32_MAX - lastNeuroCount + 1);
                        }

                        // Write error message to server console
                        std::cout << "** Missed Neural Datapoints: " << diff << "! **";

                        // Ensure interpolation is a reasonable amount
                        if (diff <= interplationThreshold)
                        {
                            // Continue the error
                            std::cout << "Interpolating " << diff << " points..." << std::endl;

                            // Interpolate and mark data as interpolated in NeuralSample message
                            double interpolationSlopes[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

                            for (int interpolatedPointNum = 0; interpolatedPointNum < diff; interpolatedPointNum++)
                            {
                                // Create a new sample data buffer in the arena
                                NeuralSample* newInterpolatedSample = google::protobuf::Arena::CreateMessage<NeuralSample>(&arena);

                                // Add in the fields from the latest BIC packet
                                newInterpolatedSample->set_numberofmeasurements(sampleCounter);
                                newInterpolatedSample->set_supplyvoltage(samples->at(i).getSupplyVoltage());
                                newInterpolatedSample->set_isconnected(samples->at(i).isConnected());
                                newInterpolatedSample->set_stimulationnumber(samples->at(i).getStimulationId());
                                newInterpolatedSample->set_stimulationactive(samples->at(i).isStimulationActive());
                                newInterpolatedSample->set_samplecounter(lastNeuroCount + interpolatedPointNum);
                                newInterpolatedSample->set_isinterpolated(true);

                                // Copy in the time domain data in
                                for (int interChannelPoint = 0; interChannelPoint < sampleNum; interChannelPoint++)
                                {
                                    double interpolatedSample = latestData[interChannelPoint] + (interpolationSlopes[interChannelPoint] * interpolatedPointNum);
                                    newInterpolatedSample->add_measurements(interpolatedSample);
                                }

                                // If we're streaming, create message
                                if (NeuralWriter != NULL)
                                {
                                    bufferedNeuroUpdate->mutable_samples()->AddAllocated(newInterpolatedSample);
                                }
                            }
                        }
                        else
                        {
                            // Continue the error
                            std::cout << "Exceeded Interpolation limit." << std::endl;

                            // Exceeded interpolation threshold, irregular loss. Send the past buffered dataset out so it's continuous
                            if (NeuralWriter != NULL && bufferedNeuroUpdate->samples().size() > 0)
                            {
                                // Attempt to write the packet to the gRPC stream writer
                                try {
                                    // TODO - THIS WILL BLOCK IF NEURALWRITER BUFFER IS FULL DUE TO SLOW READING BY CLIENT
                                    // NEED TO DISSOCIATE CORTEC READS FROM GRPC STREAMING
                                    NeuralWriter->Write(*bufferedNeuroUpdate);
                                }
                                catch (std::exception& anyException)
                                {
                                    std::cout << "GRPC Write Failed. Reason: " << anyException.what() << std::endl;
                                }

                                // Reset the gRPC memory management arena system
                                arena.Reset();
                                bufferedNeuroUpdate = google::protobuf::Arena::CreateMessage<BICgRPC::NeuralUpdate>(&arena);
                            }
                        }
                    }
                }

                // If we're streaming, create message
                if (NeuralWriter != NULL)
                {
                    // Create a new sample data buffer in the arena
                    NeuralSample* newSample = google::protobuf::Arena::CreateMessage<NeuralSample>(&arena);

                    // Add in the fields from the latest BIC packet
                    newSample->set_numberofmeasurements(sampleNum);
                    newSample->set_supplyvoltage(samples->at(i).getSupplyVoltage());
                    newSample->set_isconnected(samples->at(i).isConnected());
                    newSample->set_stimulationnumber(samples->at(i).getStimulationId());
                    newSample->set_stimulationactive(samples->at(i).isStimulationActive());
                    newSample->set_samplecounter(sampleCounter);
                    newSample->set_isinterpolated(false);

                    // Copy in the data
                    for (int j = 0; j < sampleNum; j++)
                    {
                        newSample->add_measurements(theData[j]);
                        latestData[j] = theData[j];
                    }

                    // Add it to the buffer
                    bufferedNeuroUpdate->mutable_samples()->AddAllocated(newSample);
                }

                // Update counter
                delete theData;
                lastNeuroCount = sampleCounter;
            }

            // If streaming, send data
            if (NeuralWriter != NULL && bufferedNeuroUpdate->samples().size() >= neuroDataBufferThreshold)
            {
                // Attempt to write the packet to the gRPC stream writer
                try {
                    // TODO - THIS WILL BLOCK IF NEURALWRITER BUFFER IS FULL DUE TO SLOW READING BY CLIENT
                    // NEED TO DISSOCIATE CORTEC READS FROM GRPC STREAMING
                    NeuralWriter->Write(*bufferedNeuroUpdate);
                }
                catch (std::exception& anyException)
                {
                    std::cout << "GRPC Write Failed. Reason: " << anyException.what() << std::endl;
                }

                // Reset the gRPC memory management arena system
                arena.Reset();
                bufferedNeuroUpdate = google::protobuf::Arena::CreateMessage<BICgRPC::NeuralUpdate>(&arena);
            }
        }

        delete samples;
    }

    /// <summary>
    /// Event handler for Brain Interchange implant received voltage update
    /// </summary>
    /// <param name="voltageMicroV">New implant voltage level</param>
    void BICListener::onImplantVoltageChanged(const double voltageMicroV)
    {
        // TODO check units

        // If gRPC power streaming is enabled, write out the update
        if (PowerWriter != NULL)
        {
            PowerUpdate powerMessage;
            powerMessage.set_parameter("Voltage");
            powerMessage.set_value(voltageMicroV);
            powerMessage.set_units("microvolts");
            PowerWriter->Write(powerMessage);
        }
    }

    /// <summary>
    /// Event handler for Brain Interchange external unit's coil current update
    /// </summary>
    /// <param name="currentMilliA">New coil current in milliamps</param>
    void BICListener::onPrimaryCoilCurrentChanged(const double currentMilliA)
    {
        // If gRPC power streaming is enabled, write out the update
        if (PowerWriter != NULL)
        {
            PowerUpdate powerMessage;
            powerMessage.set_parameter("CoilCurrent");
            powerMessage.set_value(currentMilliA);
            powerMessage.set_units("milliamperes");
            PowerWriter->Write(powerMessage);
        }
    }

    /// <summary>
    /// Brain Interchange event handler for power control value events
    /// </summary>
    /// <param name="controlValue">New power system control percentage</param>
    void BICListener::onImplantControlValueChanged(const double controlValue)
    {
        // If gRPC power streaming is enabled, write out the update
        if (PowerWriter != NULL)
        {
            PowerUpdate powerMessage;
            powerMessage.set_parameter("Control");
            powerMessage.set_value(controlValue);
            powerMessage.set_units("%");
            PowerWriter->Write(powerMessage);
        }
    }

    /// <summary>
    /// Brain Interchange event handler for implanted temperature events
    /// </summary>
    /// <param name="temperature">New implanted device's temperature</param>
    void BICListener::onTemperatureChanged(const double temperature)
    {
        // If gRPC temperature streaming is enabled, write out the update
        if (TemperatureWriter != NULL)
        {
            TemperatureUpdate tempMessage;
            tempMessage.set_temperature(temperature);
            tempMessage.set_units("celsius");
            TemperatureWriter->Write(tempMessage);
        }
    }

    /// <summary>
    /// Brain Interchange event handler for implanted humidity events
    /// </summary>
    /// <param name="humidity">New implanted device's humidity level</param>
    void BICListener::onHumidityChanged(const double humidity)
    {
        // If gRPC humidity streaming is enabled, write out the update
        if (HumidityWriter != NULL)
        {
            HumidityUpdate humidMessage;
            humidMessage.set_humidity(humidity);
            humidMessage.set_units("rh");
            HumidityWriter->Write(humidMessage);
        }
    }

    /// <summary>
    /// Brain Interchange event handler for DLL error events
    /// </summary>
    /// <param name="err">Error information from BIC DLL</param>
    void BICListener::onError(const std::exception& err)
    {
        // If gRPC error streaming is enabled, write out the update
        if (ErrorWriter != NULL)
        {
            ErrorUpdate errorMessage;
            errorMessage.set_message(err.what());
            ErrorWriter->Write(errorMessage);
        }
    }

    /// <summary>
    /// Brain Interchange event handler for processing rate errors. This event is called if the neural data is not being consumed quickly enough by the gRPC microservice.
    /// </summary>
    void BICListener::onDataProcessingTooSlow()
    {
        // Important event, write it out to the console
        std::cout << "*** Warning: Data processing too slow" << std::endl;
        
        // If gRPC error streaming is enabled, write out the update
        if (ErrorWriter != NULL)
        {
            ErrorUpdate errorMessage;
            errorMessage.set_message("Warning: Data processing too slow");
            ErrorWriter->Write(errorMessage);
        }
    }
}