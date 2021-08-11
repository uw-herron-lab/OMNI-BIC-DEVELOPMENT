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
    void BICListener::onStimulationStateChanged(const bool isStimulating)
    {
        // TODO - anything? bug report? Should only trigger when there is a change right?
        std::cout << "\tDEBUG: Stimulation state changed: " << isStimulating << std::endl;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_isStimulating = isStimulating;
    }

    bool BICListener::isStimulating()
    {
        // TODO - anything?
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isStimulating;
    }

    void BICListener::onMeasurementStateChanged(const bool isMeasuring)
    {
        // TODO - anything? bug report? Should only trigger when there is a change right?
        std::cout << "\tDEBUG: Measurement state changed: " << isMeasuring << std::endl;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_isMeasuring = isMeasuring;
    }

    bool BICListener::isMeasuring()
    {
        // TODO - anything?
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isMeasuring;
    }

    void BICListener::onConnectionStateChanged(const connection_info_t& info)
    {
        if (info.count(ConnectionType::PC_TO_EXT) > 0)
        {
            const bool isConnected = info.at(ConnectionType::PC_TO_EXT) == ConnectionState::CONNECTED;
            std::cout << "*** Connection state from PC to external unit changed: "
                << (isConnected ? "connected" : "disconnected") << std::endl;

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
            const bool isConnected = info.at(ConnectionType::EXT_TO_IMPLANT) == ConnectionState::CONNECTED;
            std::cout << "*** Connection state from external unit to implant changed: "
                << (isConnected ? "connected" : "disconnected") << std::endl;

            if (ConnectionWriter != NULL)
            {
                ConnectionUpdate connectionMessage;
                connectionMessage.set_connectiontype("Inductive");
                connectionMessage.set_isconnected(isConnected);
                ConnectionWriter->Write(connectionMessage);
            }
        }
    }

    void BICListener::onConnectionStateChanged(const bool isConnected)
    {
        if (ConnectionWriter != NULL)
        {
            ConnectionUpdate connectionMessage;
            connectionMessage.set_connectiontype("Overall");
            connectionMessage.set_isconnected(isConnected);
            ConnectionWriter->Write(connectionMessage);
        }
    }

    void BICListener::onData(const std::vector<CSample>* samples)
    {
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

    void BICListener::onImplantVoltageChanged(const double voltageMicroV)
    {
        // TODO check units
        if (PowerWriter != NULL)
        {
            PowerUpdate powerMessage;
            powerMessage.set_parameter("Voltage");
            powerMessage.set_value(voltageMicroV);
            powerMessage.set_units("microvolts");
            PowerWriter->Write(powerMessage);
        }
    }

    void BICListener::onPrimaryCoilCurrentChanged(const double currentMilliA)
    {
        if (PowerWriter != NULL)
        {
            PowerUpdate powerMessage;
            powerMessage.set_parameter("CoilCurrent");
            powerMessage.set_value(currentMilliA);
            powerMessage.set_units("milliamperes");
            PowerWriter->Write(powerMessage);
        }
    }

    void BICListener::onImplantControlValueChanged(const double controlValue)
    {
        if (PowerWriter != NULL)
        {
            PowerUpdate powerMessage;
            powerMessage.set_parameter("Control");
            powerMessage.set_value(controlValue);
            powerMessage.set_units("%");
            PowerWriter->Write(powerMessage);
        }
    }

    void BICListener::onTemperatureChanged(const double temperature)
    {
        if (TemperatureWriter != NULL)
        {
            TemperatureUpdate tempMessage;
            tempMessage.set_temperature(temperature);
            tempMessage.set_units("celsius");
            TemperatureWriter->Write(tempMessage);
        }
    }

    void BICListener::onHumidityChanged(const double humidity)
    {
        if (HumidityWriter != NULL)
        {
            HumidityUpdate humidMessage;
            humidMessage.set_humidity(humidity);
            humidMessage.set_units("rh");
            HumidityWriter->Write(humidMessage);
        }
    }

    void BICListener::onError(const std::exception& err)
    {
        if (ErrorWriter != NULL)
        {
            ErrorUpdate errorMessage;
            errorMessage.set_message(err.what());
            ErrorWriter->Write(errorMessage);
        }
    }

    void BICListener::onDataProcessingTooSlow()
    {
        std::cout << "*** Warning: Data processing too slow" << std::endl;
        if (ErrorWriter != NULL)
        {
            ErrorUpdate errorMessage;
            errorMessage.set_message("Warning: Data processing too slow");
            ErrorWriter->Write(errorMessage);
        }
    }
}