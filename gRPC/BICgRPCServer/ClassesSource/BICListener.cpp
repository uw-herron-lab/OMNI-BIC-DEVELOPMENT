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
                // Check if we've lost packets, for funsies
                if (lastNeuroCount + 1 != samples->at(i).getMeasurementCounter())
                {
                    if (lastNeuroCount == samples->at(i).getMeasurementCounter())
                    {
                        // Repeated Packet Count! 
                        // Write error message to server console
                        std::cout << ">>> Repeated packet counter value! <<<" << std::endl;
                    }
                    else
                    {
                        // Missed packet!
                        uint32_t newSampleCounterValue = samples->at(i).getMeasurementCounter();
                        double diff = newSampleCounterValue - (lastNeuroCount + 1);
                        if (diff < 0)
                        {
                            // Wrap around case
                            diff = (double)samples->at(i).getMeasurementCounter() + ((double)UINT32_MAX - lastNeuroCount + 1);
                        }
                        // Update counter
                        lastNeuroCount = samples->at(i).getMeasurementCounter();

                        // Write error message to server console
                        std::cout << "** Missed Neural Datapoints: " << diff << "! **" << std::endl;

                        // If neural data buffer is not empty, flush it so we can keep buffers continuous
                        if (NeuralWriter != NULL && bufferedNeuroUpdate.samples().size() > 0)
                        {
                            // TODO: Is performance here an issue? Maybe use pingpong buffers and write in a different thread?
                            NeuralWriter->Write(bufferedNeuroUpdate);
                            bufferedNeuroUpdate.Clear();
                        }
                    }
                }
                else
                {
                    // Update counter
                    lastNeuroCount = samples->at(i).getMeasurementCounter();
                }

                // If we're streaming, create message
                if (NeuralWriter != NULL)
                {
                    NeuralSample* newSample = bufferedNeuroUpdate.add_samples();
                    int numMeasures = samples->at(i).getNumberOfMeasurements();
                    newSample->set_numberofmeasurements(numMeasures);
                    newSample->set_supplyvoltage(samples->at(i).getSupplyVoltage());
                    newSample->set_isconnected(samples->at(i).isConnected());
                    newSample->set_stimulationnumber(samples->at(i).getStimulationId());
                    newSample->set_stimulationactive(samples->at(i).isStimulationActive());
                    newSample->set_samplecounter(samples->at(i).getMeasurementCounter());
                    for (int j = 0; j < numMeasures; j++)
                    {
                        newSample->add_measurements(samples->at(i).getMeasurements()[j]);
                    }
                }
            }

            // If streaming, send data
            // TODO: Use wait mutex to check before nulls?
            if (NeuralWriter != NULL && bufferedNeuroUpdate.samples().size() >= neuroDataBufferThreshold)
            {
                NeuralWriter->Write(bufferedNeuroUpdate);
                bufferedNeuroUpdate.Clear();
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