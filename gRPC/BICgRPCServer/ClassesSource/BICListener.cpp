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
    /// Enable or disable connection streaming to a gRPC client. 
    /// Function instructs BIC to start streaming, resulting in data being received by various connection event handler functions.  
    /// Events puts data in a queue for independent transmission to client by thread utilizing "grpcConnectionStreamThread" function.
    /// </summary>
    /// <param name="enableSensing">True if streaming is being requested to be enabled, false otherwise</param>
    /// <param name="aWriter">gRPC client writing inteface.</param>
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
    /// Private function that reads connection data from queue and sends to gRPC client. Intended to be run as a thread by enableConnectionSensing.
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

    //*************************************************** Neural Data Streaming and Closed-Loop Stim Functions ***************************************************
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
                    // Lock the neurobuffer
                    this->m_neuroBufferLock.lock();

                    // Add the front item to the queue for the current packet
                    bufferedNeuroUpdate->mutable_samples()->AddAllocated(neuralSampleQueue.front());

                    // Clean up the current sample from the list
                    neuralSampleQueue.pop();

                    // Unlock the neurobuffer
                    this->m_neuroBufferLock.unlock();
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

                    // Delete all elements in the buffer after transmission, freeing up memory.
                    delete bufferedNeuroUpdate;
                    bufferedNeuroUpdate = new BICgRPC::NeuralUpdate();
                }
            }
        }

        // Clean up the buffer
        delete bufferedNeuroUpdate;
    }

    double BICListener::filterIIR(double currSamp, double b[], double a[])
    {
        double filtTemp;

        // check if n-1 is negative, then n-2 would also be negative
        filtTemp = b[0] * currSamp + b[1] * prevData[0] + b[2] * prevData[1] - a[1] * filtData[0] - a[2] * filtData[1];
        
        // remove the last sample and insert the most recent sample to the front of the vector
        filtData.insert(filtData.begin(), filtTemp);
        filtData.pop_back();
        
        // prevData[0] holds the most recent sample while prevData[1] keeps older sample
        prevData.insert(prevData.begin(), currSamp);
        prevData.pop_back();

        return filtTemp;
    }

    bool BICListener::isZeroCrossing(std::vector<double> filtData)
    {
        bool sendStim = false;
        // check if most recent filtered sample is negative
        if (filtData[0] < 0)
        {
            // then check if older filtered sample was positive
            if (filtData[1] > 0)
            {
                sendStim = true;
            }
        }
        return sendStim;
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
                // Create a new sample data buffer
                int32_t sampleCounter = samples->at(i).getMeasurementCounter();
                int16_t sampleNum = samples->at(i).getNumberOfMeasurements();
                
                // variables for IIR filtering
                int datLen = 100;
                int channel = 0;
                filtData.resize(datLen, 0);
                // hardcoding filter coefficients for now
                double b[3] = { 0.0305, 0, -0.0305 }; 
                double a[3] = { 1, -1.9247, 0.9391 };

                double* theData = samples->at(i).getMeasurements();
                NeuralSample* newSample = new NeuralSample();
                newSample->set_numberofmeasurements(sampleNum);
                newSample->set_supplyvoltage(samples->at(i).getSupplyVoltage());
                newSample->set_isconnected(samples->at(i).isConnected());
                newSample->set_stimulationnumber(samples->at(i).getStimulationId());
                newSample->set_stimulationactive(samples->at(i).isStimulationActive());
                newSample->set_samplecounter(sampleCounter);
                newSample->set_isinterpolated(false);
                newSample->set_filtchannel(channel);

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

                            for (uint32_t interpolatedPointNum = 1; interpolatedPointNum <= diff; interpolatedPointNum++)
                            {
                                // Create a new sample data buffer
                                NeuralSample* newInterpolatedSample = new NeuralSample();

                                // Add in the fields from the latest BIC packet
                                newInterpolatedSample->set_numberofmeasurements(sampleNum);
                                newInterpolatedSample->set_supplyvoltage(newSample->supplyvoltage());
                                newInterpolatedSample->set_isconnected(newSample->isconnected());
                                newInterpolatedSample->set_stimulationnumber(newSample->stimulationnumber());
                                newInterpolatedSample->set_stimulationactive(newSample->stimulationactive());
                                newInterpolatedSample->set_samplecounter(lastNeuroCount + interpolatedPointNum);
                                newInterpolatedSample->set_isinterpolated(true);
                                newInterpolatedSample->set_filtchannel(channel);

                                // Copy in the time domain data in
                                for (int interChannelPoint = 0; interChannelPoint < sampleNum; interChannelPoint++)
                                {
                                    double interpolatedSample = latestData[interChannelPoint] + (interpolationSlopes[interChannelPoint] * interpolatedPointNum);
                                    newInterpolatedSample->add_measurements(interpolatedSample);
                                    if (interChannelPoint == channel)
                                    {
                                        // if at a negative zero crossing, send stimulation
                                        if (isCLStimEn && isZeroCrossing(filtData))
                                        {
                                            // start thread to execute stim command
                                            stimTrigger->notify_all();
                                        }

                                        // when interpolating the sample for a specific channel, also apply an IIR filter for that sample
                                        double filtSamp = filterIIR(interpolatedSample, b, a);
                                        newInterpolatedSample->set_filtsample(filtSamp); // set the filtered sample in the neural sample 
                                    }
                                }

                                // Add it to the buffer if there is room
                                if (neuralSampleQueue.size() < 1000)
                                {
                                    // Lock the neurobuffer, add data, unlock
                                    this->m_neuroBufferLock.lock();
                                    neuralSampleQueue.push(newInterpolatedSample);
                                    this->m_neuroBufferLock.unlock();
                                }
                                else
                                {
                                    delete newInterpolatedSample;
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

                // Update Interpolation Info for future use
                lastNeuroCount = sampleCounter;

                // Copy in the data to both the newSample and the latest data buffer (for future interpolation). Delete it when done
                for (int j = 0; j < sampleNum; j++)
                {
                    newSample->add_measurements(theData[j]);
                    latestData[j] = theData[j];
                    if (j == channel)
                    {
                        // if at a negative zero crossing, send stimulation
                        if (isCLStimEn && isZeroCrossing(filtData))
                        {
                            // start thread to execute stim command
                            stimTrigger->notify_all();
                        }

                        // filter data for particular channel and set it in newSample
                        double filtSamp = filterIIR(theData[j], b, a);
                        newSample->set_filtsample(filtSamp);
                        
                    }
                }
                delete theData;

                // Add latest received data packet to the buffer if there is room
                // If sending filtered data to stream, would update queue (filtNeuralSampleQueue) here..?
                if (neuralSampleQueue.size() < 1000)
                {
                    // Lock the neurobuffer, add data, unlock
                    this->m_neuroBufferLock.lock();
                    neuralSampleQueue.push(newSample);
                    this->m_neuroBufferLock.unlock();

                    // Notify the streaming function that new data exists
                    neuralDataNotify->notify_all();
                }
                else
                {
                    delete newSample;
                    std::cout << "GRPC Neural Queue Size Overflow, streaming data skipped" << std::endl;
                }
            }
        }

        // No matter what, delete samples
        delete samples;
    }

    /// <summary>
    /// Thread to be started up when phase-triggered stimulation functionality is enabled.
    /// Delivers a specific stimulation pattern whe
    /// </summary>
    void BICListener::phaseTriggeredSendStimThread()
    {
        // Create a mutex to make the conditional 'wait' functionality
        std::mutex stimLock;
        std::unique_lock<std::mutex> stimTriggerWait(stimLock);

        // Create stimulation command "factory"
        std::unique_ptr<IStimulationCommandFactory> theStimFactory(createStimulationCommandFactory());
        IStimulationCommand* stimulationCommand = theStimFactory->createStimulationCommand();
        IStimulationFunction* stimulationFunction = theStimFactory->createStimulationFunction();

        // Create stimulation waveform
            // <TODO>

        // Wait for a zero crossing
        stimTrigger->wait(stimTriggerWait);

        // Loop while streaming is active
        while (isCLStimEn)
        {
            // Send a stim command
            theImplantedDevice->startStimulation(stimulationCommand);

            // Wait for a zero crossing
            stimTrigger->wait(stimTriggerWait);
        }
    }

    /// <summary>
    /// Adds a pointer to the implanted device to the BICListener, required for phase-locked stim functionality
    /// </summary>
    /// <param name="anImplantedDevice">the active impalnted device</param>
    void BICListener::addImplantPointer(cortec::implantapi::IImplant* anImplantedDevice)
    {
        theImplantedDevice = anImplantedDevice;
    }
    
    /// <summary>
    /// Function that enables phase-locked stimulation functionality on a output channel based on an input channel's sensed neural activity
    /// </summary>
    /// <param name="enableClosedLoop">A boolean indicating if phasic stim should be enabled or disabled</param>
    /// <param name="phaseSensingChannel">The channel to sense phase on</param>
    /// <param name="phaseStimChannel">The channel to stimulate after negative zero crossings of phase sensing channel</param>
    void BICListener::enablePhasicStim(bool enableClosedLoop, int phaseSensingChannel, int phaseStimChannel)
    {
        if (enableClosedLoop && !isCLStimEn)
        {
            // Enable Closed Loop since it is not enabled and request is to enable
            // Instantiate the conditional variable for thread notification
            stimTrigger = new std::condition_variable();

            // Update state tracking variable
            isCLStimEn = true;

            // Start thread up
            betaStimThread = new std::thread(&BICListener::phaseTriggeredSendStimThread, this);
        }
        else if (!enableClosedLoop && isCLStimEn)
        {
            // Update state tracking variable
            isCLStimEn = false;
            stimTrigger->notify_all();

            // Disable Closed Loop since it is enabled and request is to disable
            betaStimThread->join();
            betaStimThread->~thread();
            delete betaStimThread;
            betaStimThread = NULL;
            
            // Clean up the conditional variable
            stimTrigger->~condition_variable();
            delete stimTrigger;
            stimTrigger = NULL;
        }
    }

    //*************************************************** Power Data Streaming Functions ***************************************************
    /// <summary>
    /// Enable or disable power streaming to a gRPC client. 
    /// Function instructs BIC to start streaming, resulting in data being received by various power event handler functions.  
    /// Events puts data in a queue for independent transmission to client by thread utilizing "grpcPowerStreamThread" function.
    /// </summary>
    /// <param name="enableSensing">True if streaming is being requested to be enabled, false otherwise</param>
    /// <param name="aWriter">gRPC client writing inteface.</param>
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

    /// <summary>
    /// Private function that reads power data from queue and sends to gRPC client. Intended to be run as a thread by enablePowerSensing.
    /// </summary>
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
    /// Enable or disable temperature streaming to a gRPC client. 
    /// Function instructs BIC to start streaming, resulting in data being received by "onTemperatureChanged" event handler function.  
    /// onTemperatureChanged puts data in a queue for independent transmission to client by thread utilizing "grpcTemperatureStreamThread" function.
    /// </summary>
    /// <param name="enableSensing">True if streaming is being requested to be enabled, false otherwise</param>
    /// <param name="aWriter">gRPC client writing inteface.</param>
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

    /// <summary>
    /// Private function that reads temperature data from queue and sends to gRPC client. Intended to be run as a thread by enableTemperatureSensing.
    /// </summary>
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
    /// Enable or disable humidity streaming to a gRPC client. 
    /// Function instructs BIC to start streaming, resulting in data being received by "onHumidityChanged" event handler function.  
    /// onHumidityChanged puts data in a queue for independent transmission to client by thread utilizing "grpcHumidityStreamThread" function.
    /// </summary>
    /// <param name="enableSensing">True if streaming is being requested to be enabled, false otherwise</param>
    /// <param name="aWriter">gRPC client writing inteface.</param>
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
    
    /// <summary>
    /// Private function that reads humidity data from queue and sends to gRPC client. Intended to be run as a thread by enableHumiditySensing.
    /// </summary>
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
    /// Enable or disable error streaming to a gRPC client. 
    /// Function instructs BIC to start streaming, resulting in data being received by "onError" event handler function.  
    /// onError puts data in a queue for independent transmission to client by thread utilizing "grpcErrorStreamThread" function.
    /// </summary>
    /// <param name="enableSensing">True if streaming is being requested to be enabled, false otherwise</param>
    /// <param name="aWriter">gRPC client writing inteface.</param>
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

    /// <summary>
    /// Private function that reads error data from queue and sends to gRPC client. Intended to be run as a thread by enableErrorSensing.
    /// </summary>
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