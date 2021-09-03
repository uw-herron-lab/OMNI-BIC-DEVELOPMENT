#include "BICListener.h"
#include <thread>

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

    //*************************************************** Connection Streaming Functions ***************************************************
    /// <summary>
    /// 
    /// </summary>
    /// <param name="enableSensing"></param>
    /// <param name="aWriter"></param>
    void BICListener::enableConnectionStreaming(bool enableSensing, grpc::ServerWriter<BICgRPC::ConnectionUpdate>* aWriter)
    {
        // Accessing streaming state objects, grab the mutex for protection against multi-threaded race conditions
        std::lock_guard<std::mutex> lock(m_mutex);

        // Determine action to be taken. Only take action if requested action matches potential actions based on current state.
        if (enableSensing && connectionStreamingState == false)
        {
            // Prepare to stream connection data
            connectionStreamingState = true;
            connectionWriter = aWriter;
            connectionDataNotify = new std::condition_variable();
            connectionProcessingThread = new std::thread(&BICListener::grpcConnectionStreamThread, this);
        }
        else if (!enableSensing && connectionStreamingState == true)
        {
            // Shut down streaming. First notify and wait for streaming handling thread to stop.
            connectionStreamingState = false;
            connectionDataNotify->notify_all();
            connectionProcessingThread->join();

            // Delete generated resources to free memory
            connectionProcessingThread->~thread();
            delete connectionProcessingThread;
            connectionProcessingThread = NULL;

            connectionDataNotify->~condition_variable();
            delete connectionDataNotify;
            connectionDataNotify = NULL;
        }
    }

    /// <summary>
    /// Private function that reads neural data from queue and sends to gRPC client. Intended to be run as a thread by enableNeuralSensing.
    /// </summary>
    void BICListener::grpcConnectionStreamThread()
    {
        // Prepare the waiting for data variables
        std::mutex connectionDataLock;
        std::unique_lock<std::mutex> connectionDataWait(connectionDataLock);

        // Loop while streaming is active
        while (connectionStreamingState)
        {
            if (connectionSampleQueue.empty())
            {
                connectionDataNotify->wait(connectionDataWait);
            }
            else
            {
                // Attempt to write the packet to the gRPC stream writer
                try {
                    // WARNING - THIS WILL BLOCK IF WRITER BUFFER IS FULL DUE TO SLOW READING BY CLIENT
                    connectionWriter->Write(*connectionSampleQueue.front());

                    // Clean up the current sample from the list
                    connectionSampleQueue.pop();
                }
                catch (std::exception& anyException)
                {
                    std::cout << "GRPC Write Failed. Reason: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "GRPC Write Buffer Failed. No reason." << std::endl;
                }
            }
        }
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
            if (connectionStreamingState)
            {
                ConnectionUpdate* connectionMessage = new ConnectionUpdate();
                connectionMessage->set_connectiontype("USB");
                connectionMessage->set_isconnected(isConnected);

                // Add it to the buffer if there is room
                if (connectionSampleQueue.size() < 100)
                {
                    connectionSampleQueue.push(connectionMessage);
                    connectionDataNotify->notify_all();
                }
                else
                {
                    delete connectionMessage;
                    std::cout << "GRPC Connection Queue Size Overflow, streaming data skipped" << std::endl;
                }
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
            if (connectionStreamingState)
            {
                ConnectionUpdate* connectionMessage = new ConnectionUpdate();
                connectionMessage->set_connectiontype("Inductive");
                connectionMessage->set_isconnected(isConnected);

                // Add it to the buffer if there is room
                if (connectionSampleQueue.size() < 100)
                {
                    connectionSampleQueue.push(connectionMessage);
                    connectionDataNotify->notify_all();
                }
                else
                {
                    delete connectionMessage;
                    std::cout << "GRPC Connection Queue Size Overflow, streaming data skipped" << std::endl;
                }
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
        if (connectionStreamingState)
        {
            ConnectionUpdate* connectionMessage = new ConnectionUpdate();
            connectionMessage->set_connectiontype("Overall");
            connectionMessage->set_isconnected(isConnected);

            // Add it to the buffer if there is room
            if (connectionSampleQueue.size() < 100)
            {
                connectionSampleQueue.push(connectionMessage);
                connectionDataNotify->notify_all();
            }
            else
            {
                delete connectionMessage;
                std::cout << "GRPC Connection Queue Size Overflow, streaming data skipped" << std::endl;
            }
        }
    }

    //*************************************************** Neural Data Streaming Functions ***************************************************
    /// <summary>
    /// Enable or disable neural streaming to a gRPC client. 
    /// Function instructs BIC to start streaming, resulting in data being received by "onData" event handler function.  
    /// onData puts data in a queue for independent transmission to client by thread utilizing "grpcNeuralStreamThread" function.
    /// </summary>
    /// <param name="enableSensing">True if streaming is being requested to be enabled, false otherwise</param>
    /// <param name="dataBufferSize">Size of buffered data packets to be returned to gRPC client</param>
    /// <param name="interplationThreshold">The maximum number of data points to interpolate between lost data points</param>
    /// <param name="aWriter">gRPC client writing inteface.</param>
    void BICListener::enableNeuralStreaming(bool enableSensing, uint32_t dataBufferSize, uint32_t interplationThreshold, grpc::ServerWriter<BICgRPC::NeuralUpdate>* aWriter)
    {
        // Accessing streaming state objects, grab the mutex for protection against multi-threaded race conditions
        std::lock_guard<std::mutex> lock(m_mutex);

        // Determine action to be taken. Only take action if requested action matches potential actions based on current state.
        if (enableSensing && neuralStreamingState == false)
        {
            // Prepare to stream neural data
            neuralStreamingState = true;
            neuroDataBufferThreshold = dataBufferSize;
            neuroInterplationThreshold = interplationThreshold;
            neuralWriter = aWriter;
            neuralDataNotify = new std::condition_variable();
            neuralProcessingThread = new std::thread (&BICListener::grpcNeuralStreamThread, this);
        }
        else if (!enableSensing && neuralStreamingState == true)
        {
            // Shut down streaming. First notify and wait for streaming handling thread to stop.
            neuralStreamingState = false;
            neuralDataNotify->notify_all();
            neuralProcessingThread->join();

            // Delete generated resources to free memory
            neuralProcessingThread->~thread();
            delete neuralProcessingThread;
            neuralProcessingThread = NULL;

            neuralDataNotify->~condition_variable();
            delete neuralDataNotify;
            neuralDataNotify = NULL;
        }
    }

    /// <summary>
    /// Private function that reads neural data from queue and sends to gRPC client. Intended to be run as a thread by enableNeuralSensing.
    /// </summary>
    void BICListener::grpcNeuralStreamThread()
    {
        // Create buffer objects for gRPC straming and interpolation
        BICgRPC::NeuralUpdate* bufferedNeuroUpdate = new BICgRPC::NeuralUpdate();
        std::mutex neuralDataLock;
        std::unique_lock<std::mutex> neuroDataWait(neuralDataLock);

        // Loop while streaming is active
        while (neuralStreamingState)
        {
            if (neuralSampleQueue.empty())
            {
                neuralDataNotify->wait(neuroDataWait);
            }
            else
            {
                try {
                    // Add the front item to the queue for the current packet
                    bufferedNeuroUpdate->mutable_samples()->AddAllocated(neuralSampleQueue.front());
                    
                    // Clean up the current sample from the list
                    neuralSampleQueue.pop();
                }
                catch (std::exception& anyException)
                {
                    std::cout << "GRPC Read Buffer Failed. Reason: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "GRPC Read Buffer Failed. No reason." << std::endl;
                }

                // If enough data has been queued, send data
                if (bufferedNeuroUpdate->samples().size() >= neuroDataBufferThreshold)
                {
                    // Attempt to write the packet to the gRPC stream writer
                    try {
                        // WARNING - THIS WILL BLOCK IF NEURALWRITER BUFFER IS FULL DUE TO SLOW READING BY CLIENT
                        neuralWriter->Write(*bufferedNeuroUpdate);
                    }
                    catch (std::exception& anyException)
                    {
                        std::cout << "GRPC Write Failed. Reason: " << anyException.what() << std::endl;
                    }
                    catch (...)
                    {
                        std::cout << "GRPC Write Buffer Failed. No reason." << std::endl;
                    }

                    // Delete all elements in the buffer after trasmission, freeing up memory.
                    bufferedNeuroUpdate->mutable_samples()->DeleteSubrange(0, bufferedNeuroUpdate->samples().size());
                }
            }
        }
    }

    /// <summary>
    /// Event handler for Brain Interchange neural data received. 
    /// Not intended to be called from gRPC microservice.
    /// </summary>
    /// <param name="samples">BIC sensed LFP samples</param>
    void BICListener::onData(const std::vector<CSample>* samples)
    {
        // Ensure samples is not empty before working with it.
        if (!samples->empty())
        {
            // Loop through retrieved samples
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
                        if (diff <= neuroInterplationThreshold)
                        {
                            // Continue the error
                            std::cout << "Interpolating " << diff << " points..." << std::endl;

                            // Interpolate and mark data as interpolated in NeuralSample message
                            double interpolationSlopes[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

                            for (int interpolatedPointNum = 0; interpolatedPointNum < diff; interpolatedPointNum++)
                            {
                                // Create a new sample data buffer
                                NeuralSample* newInterpolatedSample = new NeuralSample();

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

                                // Add it to the buffer if there is room
                                if (neuralSampleQueue.size() < 1000)
                                {
                                    neuralSampleQueue.push(newInterpolatedSample);
                                    neuralDataNotify->notify_all();
                                }
                                else
                                {
                                    newInterpolatedSample->Clear();
                                    std::cout << "GRPC Neural Queue Size Overflow, streaming data skipped" << std::endl;
                                }
                            }
                        }
                        else
                        {
                            // Continue the error
                            std::cout << "Exceeded Interpolation limit. Data loss indicated by dropout in sample count" << std::endl;
                        }
                    }
                }

                // If we're streaming, create message
                // Create a new sample data buffer
                NeuralSample* newSample = new NeuralSample();

                // Add in the fields from the latest BIC packet
                newSample->set_numberofmeasurements(sampleNum);
                newSample->set_supplyvoltage(samples->at(i).getSupplyVoltage());
                newSample->set_isconnected(samples->at(i).isConnected());
                newSample->set_stimulationnumber(samples->at(i).getStimulationId());
                newSample->set_stimulationactive(samples->at(i).isStimulationActive());
                newSample->set_samplecounter(sampleCounter);
                newSample->set_isinterpolated(false);

                // Copy in the data to both the newSample and the latest data buffer (for interpolation). Delete it when done
                for (int j = 0; j < sampleNum; j++)
                {
                    newSample->add_measurements(theData[j]);
                    latestData[j] = theData[j];
                }
                delete theData;

                // Add it to the buffer if there is room
                if (neuralSampleQueue.size() < 1000)
                {
                    neuralSampleQueue.push(newSample);
                    neuralDataNotify->notify_all();
                }
                else
                {
                    newSample->Clear();
                    std::cout << "GRPC Neural Queue Size Overflow, streaming data skipped" << std::endl;
                }

                // Update counter
                lastNeuroCount = sampleCounter;
            }
        }

        // No matter what, delete samples
        delete samples;
    }

    //*************************************************** Power Data Streaming Functions ***************************************************
    /// <summary>
    /// 
    /// </summary>
    /// <param name="enableSensing"></param>
    /// <param name="aWriter"></param>
    void BICListener::enablePowerStreaming(bool enableSensing, grpc::ServerWriter<BICgRPC::PowerUpdate>* aWriter)
    {
        // Accessing streaming state objects, grab the mutex for protection against multi-threaded race conditions
        std::lock_guard<std::mutex> lock(m_mutex);

        // Determine action to be taken. Only take action if requested action matches potential actions based on current state.
        if (enableSensing && powerStreamingState == false)
        {
            // Prepare to stream power data
            powerStreamingState = true;
            powerWriter = aWriter;
            powerDataNotify = new std::condition_variable();
            powerProcessingThread = new std::thread(&BICListener::grpcPowerStreamThread, this);
        }
        else if (!enableSensing && powerStreamingState == true)
        {
            // Shut down streaming. First notify and wait for streaming handling thread to stop.
            powerStreamingState = false;
            powerDataNotify->notify_all();
            powerProcessingThread->join();

            // Delete generated resources to free memory
            powerProcessingThread->~thread();
            delete powerProcessingThread;
            powerProcessingThread = NULL;

            powerDataNotify->~condition_variable();
            delete powerDataNotify;
            powerDataNotify = NULL;
        }
    }

    void BICListener::grpcPowerStreamThread()
    {
        // Prepare the waiting for data variables
        std::mutex powerDataLock;
        std::unique_lock<std::mutex> powerDataWait(powerDataLock);

        // Loop while streaming is active
        while (powerStreamingState)
        {
            if (powerSampleQueue.empty())
            {
                powerDataNotify->wait(powerDataWait);
            }
            else
            {
                // Attempt to write the packet to the gRPC stream writer
                try {
                    // WARNING - THIS WILL BLOCK IF WRITER BUFFER IS FULL DUE TO SLOW READING BY CLIENT
                    powerWriter->Write(*powerSampleQueue.front());

                    // Clean up the current sample from the list
                    powerSampleQueue.pop();
                }
                catch (std::exception& anyException)
                {
                    std::cout << "GRPC Write Failed. Reason: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "GRPC Write Buffer Failed. No reason." << std::endl;
                }
            }
        }
    }

    /// <summary>
    /// Event handler for Brain Interchange implant received voltage update
    /// </summary>
    /// <param name="voltageMicroV">New implant voltage level</param>
    void BICListener::onImplantVoltageChanged(const double voltageMicroV)
    {
        // If gRPC power streaming is enabled, write out the update
        if (powerStreamingState)
        {
            PowerUpdate* powerMessage = new PowerUpdate();
            powerMessage->set_parameter("Voltage");
            powerMessage->set_value(voltageMicroV);
            powerMessage->set_units("microvolts");

            // Add it to the buffer if there is room
            if (powerSampleQueue.size() < 100)
            {
                powerSampleQueue.push(powerMessage);
                powerDataNotify->notify_all();
            }
            else
            {
                delete powerMessage;
                std::cout << "GRPC Power Queue Size Overflow, streaming data skipped" << std::endl;
            }
        }
    }

    /// <summary>
    /// Event handler for Brain Interchange external unit's coil current update
    /// </summary>
    /// <param name="currentMilliA">New coil current in milliamps</param>
    void BICListener::onPrimaryCoilCurrentChanged(const double currentMilliA)
    {
        // If gRPC power streaming is enabled, write out the update
        if (powerStreamingState)
        {
            PowerUpdate* powerMessage = new PowerUpdate();
            powerMessage->set_parameter("CoilCurrent");
            powerMessage->set_value(currentMilliA);
            powerMessage->set_units("milliamperes");

            // Add it to the buffer if there is room
            if (powerSampleQueue.size() < 100)
            {
                powerSampleQueue.push(powerMessage);
                powerDataNotify->notify_all();
            }
            else
            {
                delete powerMessage;
                std::cout << "GRPC Power Queue Size Overflow, streaming data skipped" << std::endl;
            }
        }
    }

    /// <summary>
    /// Brain Interchange event handler for power control value events
    /// </summary>
    /// <param name="controlValue">New power system control percentage</param>
    void BICListener::onImplantControlValueChanged(const double controlValue)
    {
        // If gRPC power streaming is enabled, write out the update
        if (powerStreamingState)
        {
            PowerUpdate* powerMessage = new PowerUpdate();
            powerMessage->set_parameter("Control");
            powerMessage->set_value(controlValue);
            powerMessage->set_units("%");

            // Add it to the buffer if there is room
            if (powerSampleQueue.size() < 100)
            {
                powerSampleQueue.push(powerMessage);
                powerDataNotify->notify_all();
            }
            else
            {
                delete powerMessage;
                std::cout << "GRPC Power Queue Size Overflow, streaming data skipped" << std::endl;
            }
        }
    }

    //*************************************************** Temperature Streaming Functions ***************************************************
    /// <summary>
    /// 
    /// </summary>
    /// <param name="enableSensing"></param>
    /// <param name="aWriter"></param>
    void BICListener::enableTemperatureStreaming(bool enableSensing, grpc::ServerWriter<BICgRPC::TemperatureUpdate>* aWriter)
    {
        // Accessing streaming state objects, grab the mutex for protection against multi-threaded race conditions
        std::lock_guard<std::mutex> lock(m_mutex);

        // Determine action to be taken. Only take action if requested action matches potential actions based on current state.
        if (enableSensing && temperatureStreamingState == false)
        {
            // Prepare to stream neural data
            temperatureStreamingState = true;
            temperatureWriter = aWriter;
            temperatureDataNotify = new std::condition_variable();
            temperatureProcessingThread = new std::thread(&BICListener::grpcTemperatureStreamThread, this);
        }
        else if (!enableSensing && temperatureStreamingState == true)
        {
            // Shut down streaming. First notify and wait for streaming handling thread to stop.
            temperatureStreamingState = false;
            temperatureDataNotify->notify_all();
            temperatureProcessingThread->join();

            // Delete generated resources to free memory
            temperatureProcessingThread->~thread();
            delete temperatureProcessingThread;
            temperatureProcessingThread = NULL;

            temperatureDataNotify->~condition_variable();
            delete temperatureDataNotify;
            temperatureDataNotify = NULL;
        }
    }

    void BICListener::grpcTemperatureStreamThread()
    {
        // Prepare the waiting for data variables
        std::mutex temperatureDataLock;
        std::unique_lock<std::mutex> temperatureDataWait(temperatureDataLock);

        // Loop while streaming is active
        while (temperatureStreamingState)
        {
            if (temperatureSampleQueue.empty())
            {
                temperatureDataNotify->wait(temperatureDataWait);
            }
            else
            {
                // Attempt to write the packet to the gRPC stream writer
                try {
                    // WARNING - THIS WILL BLOCK IF WRITER BUFFER IS FULL DUE TO SLOW READING BY CLIENT
                    temperatureWriter->Write(*temperatureSampleQueue.front());

                    // Clean up the current sample from the list
                    temperatureSampleQueue.pop();
                }
                catch (std::exception& anyException)
                {
                    std::cout << "GRPC Write Failed. Reason: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "GRPC Write Buffer Failed. No reason." << std::endl;
                }
            }
        }
    }

    /// <summary>
    /// Brain Interchange event handler for implanted temperature events
    /// </summary>
    /// <param name="temperature">New implanted device's temperature</param>
    void BICListener::onTemperatureChanged(const double temperature)
    {
        // If gRPC temperature streaming is enabled, write out the update
        if (temperatureStreamingState)
        {
            TemperatureUpdate* temperatureMessage = new TemperatureUpdate();
            temperatureMessage->set_temperature(temperature);
            temperatureMessage->set_units("celsius");

            // Add it to the buffer if there is room
            if (temperatureSampleQueue.size() < 100)
            {
                temperatureSampleQueue.push(temperatureMessage);
                temperatureDataNotify->notify_all();
            }
            else
            {
                delete temperatureMessage;
                std::cout << "GRPC Temperature Queue Size Overflow, streaming data skipped" << std::endl;
            }
        }
    }

    //*************************************************** Humidity Streaming Functions ***************************************************
    /// <summary>
    /// 
    /// </summary>
    /// <param name="enableSensing"></param>
    /// <param name="aWriter"></param>
    void BICListener::enableHumidityeStreaming(bool enableSensing, grpc::ServerWriter<BICgRPC::HumidityUpdate>* aWriter)
    {
        // Accessing streaming state objects, grab the mutex for protection against multi-threaded race conditions
        std::lock_guard<std::mutex> lock(m_mutex);

        // Determine action to be taken. Only take action if requested action matches potential actions based on current state.
        if (enableSensing && humidityStreamingState == false)
        {
            // Prepare to stream humidity data
            humidityStreamingState = true;
            humidityWriter = aWriter;
            humidityDataNotify = new std::condition_variable();
            humidityProcessingThread = new std::thread(&BICListener::grpcHumidityStreamThread, this);
        }
        else if (!enableSensing && humidityStreamingState == true)
        {
            // Shut down streaming. First notify and wait for streaming handling thread to stop.
            humidityStreamingState = false;
            humidityDataNotify->notify_all();
            humidityProcessingThread->join();

            // Delete generated resources to free memory
            humidityProcessingThread->~thread();
            delete humidityProcessingThread;
            humidityProcessingThread = NULL;

            humidityDataNotify->~condition_variable();
            delete humidityDataNotify;
            humidityDataNotify = NULL;
        }
    }
    
    void BICListener::grpcHumidityStreamThread()
    {
        // Prepare the waiting for data variables
        std::mutex humidityDataLock;
        std::unique_lock<std::mutex> humidityDataWait(humidityDataLock);

        // Loop while streaming is active
        while (humidityStreamingState)
        {
            if (humiditySampleQueue.empty())
            {
                humidityDataNotify->wait(humidityDataWait);
            }
            else
            {
                // Attempt to write the packet to the gRPC stream writer
                try {
                    // WARNING - THIS WILL BLOCK IF WRITER BUFFER IS FULL DUE TO SLOW READING BY CLIENT
                    humidityWriter->Write(*humiditySampleQueue.front());

                    // Clean up the current sample from the list
                    humiditySampleQueue.pop();
                }
                catch (std::exception& anyException)
                {
                    std::cout << "GRPC Write Failed. Reason: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "GRPC Write Buffer Failed. No reason." << std::endl;
                }
            }
        }
    }

    /// <summary>
    /// Brain Interchange event handler for implanted humidity events
    /// </summary>
    /// <param name="humidity">New implanted device's humidity level</param>
    void BICListener::onHumidityChanged(const double humidity)
    {
        // If gRPC humidity streaming is enabled, write out the update
        if (humidityStreamingState)
        {
            HumidityUpdate* humidityMessage = new HumidityUpdate();
            humidityMessage->set_humidity(humidity);
            humidityMessage->set_units("rh");

            // Add it to the buffer if there is room
            if (humiditySampleQueue.size() < 100)
            {
                humiditySampleQueue.push(humidityMessage);
                humidityDataNotify->notify_all();
            }
            else
            {
                delete humidityMessage;
                std::cout << "GRPC Humidity Queue Size Overflow, streaming data skipped" << std::endl;
            }
        }
    }

    //*************************************************** Error Streaming Functions ***************************************************
    /// <summary>
    /// 
    /// </summary>
    /// <param name="enableSensing"></param>
    /// <param name="aWriter"></param>
    void BICListener::enableErrorStreaming(bool enableSensing, grpc::ServerWriter<BICgRPC::ErrorUpdate>* aWriter)
    {
        // Accessing streaming state objects, grab the mutex for protection against multi-threaded race conditions
        std::lock_guard<std::mutex> lock(m_mutex);

        // Determine action to be taken. Only take action if requested action matches potential actions based on current state.
        if (enableSensing && errorStreamingState == false)
        {
            // Prepare to stream error data
            errorStreamingState = true;
            errorWriter = aWriter;
            errorDataNotify = new std::condition_variable();
            errorProcessingThread = new std::thread(&BICListener::grpcErrorStreamThread, this);
        }
        else if (!enableSensing && errorStreamingState == true)
        {
            // Shut down streaming. First notify and wait for streaming handling thread to stop.
            errorStreamingState = false;
            errorDataNotify->notify_all();
            errorProcessingThread->join();

            // Delete generated resources to free memory
            errorProcessingThread->~thread();
            delete errorProcessingThread;
            errorProcessingThread = NULL;

            errorDataNotify->~condition_variable();
            delete errorDataNotify;
            errorDataNotify = NULL;
        }
    }

    void BICListener::grpcErrorStreamThread()
    {
        // Prepare the waiting for data variables
        std::mutex errorDataLock;
        std::unique_lock<std::mutex> errorDataWait(errorDataLock);

        // Loop while streaming is active
        while (errorStreamingState)
        {
            if (errorSampleQueue.empty())
            {
                errorDataNotify->wait(errorDataWait);
            }
            else
            {
                // Attempt to write the packet to the gRPC stream writer
                try {
                    // WARNING - THIS WILL BLOCK IF WRITER BUFFER IS FULL DUE TO SLOW READING BY CLIENT
                    errorWriter->Write(*errorSampleQueue.front());

                    // Clean up the current sample from the list
                    errorSampleQueue.pop();
                }
                catch (std::exception& anyException)
                {
                    std::cout << "GRPC Write Failed. Reason: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "GRPC Write Buffer Failed. No reason." << std::endl;
                }
            }
        }
    }

    /// <summary>
    /// Brain Interchange event handler for DLL error events
    /// </summary>
    /// <param name="err">Error information from BIC DLL</param>
    void BICListener::onError(const std::exception& err)
    {
        // If gRPC error streaming is enabled, write out the update
        if (errorStreamingState)
        {
            ErrorUpdate* errorMessage = new ErrorUpdate();
            errorMessage->set_message(err.what());

            // Add it to the buffer if there is room
            if (errorSampleQueue.size() < 100)
            {
                errorSampleQueue.push(errorMessage);
                errorDataNotify->notify_all();
            }
            else
            {
                delete errorMessage;
                std::cout << "GRPC Error Queue Size Overflow, streaming data skipped" << std::endl;
            }
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
        if (errorStreamingState)
        {
            ErrorUpdate* errorMessage = new ErrorUpdate();
            errorMessage->set_message("Warning: Data processing too slow");

            // Add it to the buffer if there is room
            if (errorSampleQueue.size() < 100)
            {
                errorSampleQueue.push(errorMessage);
                errorDataNotify->notify_all();
            }
            else
            {
                delete errorMessage;
                std::cout << "GRPC Error Queue Size Overflow, streaming data skipped" << std::endl;
            }
        }
    }
}