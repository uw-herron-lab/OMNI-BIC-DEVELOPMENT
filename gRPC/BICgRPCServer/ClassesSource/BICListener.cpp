#include "BICListener.h"
#include <thread>
#include <iostream>
#include <chrono>
#include <ctime>
#include <fstream>

// #define to enable onData console events
//#define DEBUG_CONSOLE_ENABLE;

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
    //*************************************************** Device State Event Handlers ***************************************************
    /// <summary>
    /// Event Handler for Brain Interchange stimulation state change events
    /// </summary>
    /// <param name="isStimulating">Boolean indicates if stimulation is active or not</param>
    void BICListener::onStimulationStateChanged(const bool isStimulating)
    {
        // Write Event Information to Console
        std::cout << "\tSTATE CHANGE: Stimulation state changed: " << isStimulating << std::endl;

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
        std::cout << "\tSTATE CHANGE: Measurement state changed: " << isMeasuring << std::endl;

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
    /// Event handler that indicates when a stimulation function has finished executing
    /// </summary>
    /// <param name="numFinishedFunctions">The number of functions that have finished</param>
    void BICListener::onStimulationFunctionFinished(const uint64_t numFinishedFunctions)
    {
        // Write Event Information to Console
        std::cout << "\tSTATE CHANGE: Stimulation Function Completed: " << numFinishedFunctions << " Finished Functions." << std::endl;
    }

    /// <summary>
    /// Event handler that indicates when the RF connection between implant and external unit updates
    /// </summary>
    /// <param name="antennaQualitydBm">Antenna connection quality measure measured in dBm</param>
    /// <param name="validFramesReceived">Integer indicating the number of valid frames received</param>
    /// <param name="invalidHandshake">Integer indicating invalid handshakes</param>
    /// <param name="radioCrcErrors">Integer indicating CRC errors</param>
    /// <param name="otherRxErrors">Integer indicating other RX errors</param>
    /// <param name="rxQueueOverflows">Integer indicating number of RX queue overflows</param>
    /// <param name="txQueueOverflows">Integer indicating number of TX queue overflows</param>
    void BICListener::onRfQualityUpdate(const int8_t antennaQualitydBm,
        const uint16_t validFramesReceived, const uint16_t invalidHandshake,
        const uint16_t radioCrcErrors, const uint16_t otherRxErrors,
        const uint32_t rxQueueOverflows, const uint32_t txQueueOverflows)
    {
        // Write Event Information to Console
        std::cout << "\tSTATE CHANGE: Rf Quality Update: " << antennaQualitydBm << "dBm" << std::endl;
    }

    /// <summary>
    /// Event handler that indicates when the RF channel is updated
    /// </summary>
    /// <param name="rfChannel"></param>
    void BICListener::onChannelUpdate(const uint8_t rfChannel)
    {
        // Write Event Information to Console
        std::cout << "\tSTATE CHANGE: Rf Channel Update: " << rfChannel << std::endl;
    }

    /// <summary>
    /// Event handler to report the ID of the last stimulation function executed
    /// </summary>
    /// <param name="id"></param>
    void BICListener::onLastStimulationFunctionId(const uint16_t id)
    {
        if (m_isStimulating)
        {
            // Write Event Information to Console
        std::cout << "\tSTATE INFO: Stimulation Function ID: " << id << " ." << std::endl;
        }
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

    //*************************************************** Neural Data Streaming ***************************************************
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

    /// <summary>
    /// Event handler for Brain Interchange neural data received. 
    /// Not intended to be called from gRPC microservice.
    /// </summary>
    /// <param name="samples">BIC sensed LFP samples</param>
    void BICListener::onData(const std::vector<CSample>* samples)
    {
        std::chrono::system_clock::time_point packetReceived = std::chrono::system_clock::now();
        // Ensure samples is not empty before working with it.
        if (!samples->empty())
        {
            // Loop through retrieved samples
            for (int i = 0; i < samples->size(); i++)
            {
                // Create a new sample data buffer
                int32_t sampleCounter = samples->at(i).getMeasurementCounter();
                int16_t sampleNum = samples->at(i).getNumberOfMeasurements();
                double* theData = samples->at(i).getMeasurements();
                uint64_t sampleTime = packetReceived.time_since_epoch().count();
                NeuralSample* newSample = new NeuralSample();
                newSample->set_numberofmeasurements(sampleNum);
                newSample->set_supplyvoltage(samples->at(i).getSupplyVoltage());
                newSample->set_isconnected(samples->at(i).isConnected());
                newSample->set_stimulationnumber(samples->at(i).getStimulationId());
                newSample->set_stimulationactive(samples->at(i).isStimulationActive());
                newSample->set_samplecounter(sampleCounter);
                newSample->set_isinterpolated(false);
                newSample->set_filtchannel(distributedInputChannel);
                newSample->set_timestamp(sampleTime);
                newSample->set_triggerphase(stimTriggerPhase);
                newSample->set_isinputtrighigh(samples->at(i).isMeasurementTriggerHigh());


                // Check if we've lost packets, if so interpolate
                if (lastNeuroCount + 1 != sampleCounter)
                {
                    if (lastNeuroCount == sampleCounter)
                    {
                        // Repeated Packet Count! 
                        // Write error message to server console
                        std::cout << "WARNING: Repeated packet counter value in sensing packets!" << std::endl;
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

#ifdef DEBUG_CONSOLE_ENABLE
                        // Write error message to server console
                        std::cout << "DEBUG: Missed Neural Datapoints: " << diff << "! **";
#endif

                        // Ensure interpolation is a reasonable amount
                        if (diff <= neuroInterplationThreshold)
                        {
                            // Continue the error
#ifdef DEBUG_CONSOLE_ENABLE
                            std::cout << "DEBUG: Interpolating " << diff << " points..." << std::endl;
#endif

                            // Determine the interpolation slopes
                            double interpolationSlopes[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
                            for (int index = 0; index < 32; index++)
                            {
                                interpolationSlopes[index] = (theData[index] - latestData[index]) / (diff + 1);
                            }

                            // Interpolate and mark data as interpolated in NeuralSample message
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
                                newInterpolatedSample->set_filtchannel(distributedInputChannel);
                                newInterpolatedSample->set_timestamp(latestTimeStamp);
                                newInterpolatedSample->set_triggerphase(stimTriggerPhase);
                                newInterpolatedSample->set_isinputtrighigh(newSample->isinputtrighigh());
                                
                                // Copy in the time domain data in
                                for (int interChannelPoint = 0; interChannelPoint < sampleNum; interChannelPoint++)
                                {
                                    double interpolatedSample = latestData[interChannelPoint] + (interpolationSlopes[interChannelPoint] * interpolatedPointNum);
                                    newInterpolatedSample->add_measurements(interpolatedSample);
                                    if (interChannelPoint == distributedInputChannel)
                                    {

                                        // Call calcPhase to estimate current sample's phase
                                        newInterpolatedSample->set_phase(calcPhase(bpFiltData, lastNeuroCount + interpolatedPointNum, &sigFreqData, &phaseData));

                                        // Call Processing Helper, take output and send to client
                                        newInterpolatedSample->set_filtsample(processingHelper(interpolatedSample, &rawPrevData, &stimOnset, &hampelPrevData, &dcFiltPrevData, sampGain));
                                        newInterpolatedSample->set_prefiltsample(dcFiltPrevData[0]);
                                        newInterpolatedSample->set_hampelfiltsample(hampelPrevData[0]);
                                        newInterpolatedSample->set_isvalidtarget(isValidTarget);

                                        // Update stimulation history window
                                        if (newInterpolatedSample->stimulationactive() == true && prevStimActive == false)
                                        {
                                            stimOnset.insert(stimOnset.begin(), 1);
                                            updateTriggerPhase(newInterpolatedSample->phase());
                                            prevStimActive = true;
                                            stimSampStamp.insert(stimSampStamp.begin(), lastNeuroCount + interpolatedPointNum);
                                            stimSampStamp.pop_back();
                                        }
                                        else
                                        {
                                            stimOnset.insert(stimOnset.begin(), 0);
                                        }

                                        // Mitigate self-triggering
                                        if (isSelfTrig == false)
                                        {
                                            detectSelfTriggering(stimSampStamp, 1.25 * 1 / (sigFreqData[0]) * 1000);
                                        }
                                        else
                                        {
                                            if (lastNeuroCount + interpolatedPointNum - stimSampStamp[0] > 150) 
                                            {
                                                isSelfTrig = false;
                                            }
                                        }
                                        if (newInterpolatedSample->stimulationactive() == false && prevStimActive == true)
                                        {
                                            // update state variable on stimActive state
                                            prevStimActive = false;
                                        }
                                        stimOnset.pop_back();
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
                                    std::cout << "WARNING: GRPC Neural Queue Size Overflow, streaming data skipped" << std::endl;
                                }
                            }
                        }
                        else
                        {
                            // Continue the error
                            std::cout << "WARNING: Exceeded Interpolation limit. Data loss indicated by dropout in sample count" << std::endl;
                        }
                    }
                }

                // Update Interpolation Info for future use
                lastNeuroCount = sampleCounter;

                // Update time value for interpolated sample
                latestTimeStamp = sampleTime;

                // Copy in the data to both the newSample and the latest data buffer (for future interpolation). Delete it when done
                for (int j = 0; j < sampleNum; j++)
                {
                    newSample->add_measurements(theData[j]);
                    latestData[j] = theData[j];
                    if (j == distributedInputChannel)
                    {
                        // Estimate current sample's phase
                        newSample->set_phase(calcPhase(bpFiltData, sampleCounter, &sigFreqData, &phaseData));

                        // Call Processing Helper, take output and send to client
                        newSample->set_filtsample(processingHelper(theData[j], &rawPrevData, &stimOnset, &hampelPrevData, &dcFiltPrevData, sampGain));
                        newSample->set_prefiltsample(dcFiltPrevData[0]);
                        newSample->set_hampelfiltsample(hampelPrevData[0]);
                        newSample->set_isvalidtarget(isValidTarget);

                        // Update stimulation history window
                        if (newSample->stimulationactive() == true && prevStimActive == false)
                        {
                            stimOnset.insert(stimOnset.begin(), 1);
                            updateTriggerPhase(newSample->phase());
                            prevStimActive = true;
                            stimSampStamp.insert(stimSampStamp.begin(), sampleCounter);
                            stimSampStamp.pop_back();
                        }
                        else
                        {
                            stimOnset.insert(stimOnset.begin(), 0);
                        }

                        // Mitigate self-triggering
                        if (isSelfTrig == false)
                        {
                            detectSelfTriggering(stimSampStamp, 1.25 * 1 / (sigFreqData[0]) * 1000);
                        }
                        else
                        {
                            if (sampleCounter - stimSampStamp[0] > 150) 
                            {
                                isSelfTrig = false;
                            }

                        }

                        if (newSample->stimulationactive() == false && prevStimActive == true)
                        {
                            prevStimActive = false;
                        }
                        stimOnset.pop_back();
                    }
                }

                delete theData;

                // Add latest received data packet to the buffer if there is room
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
                    std::cout << "WARNING: GRPC Neural Queue Size Overflow, streaming data skipped" << std::endl;
                }
            }
        }
        // No matter what, delete samples
        delete samples;
    }

    //*************************************************** Microservice Triggered Stimulation Functions ***************************************************
    
    /// <summary>
    /// Function that enables phase-locked stimulation functionality on an output channel based on an input channel's sensed neural activity
    /// </summary>
    /// <param name="enableDistributed">A boolean indicating if phasic stim should be enabled or disabled</param>
    /// <param name="sensingChannel">The channel to sense neural activity on</param>
    /// <param name="filtCoeff_B">B-array with constants for IIR filter</param>
    /// <param name="filtCoeff_A">A-array with constants for IIR filter</param>
    /// <param name="triggeredFunctionIndex"></param>
    /// <param name="stimThreshold">Amplitude threshold condition to send stimulation</param>
    /// <param name="triggerPhase">Triggering phase condition to send stimulation</param>
    /// <param name="nStimHistory">Size of window to sample-and-hold stimulation artifact</param>
    /// <param name="nSelfTrigLimit">Upper limit of consecutive stimulation pulses to trigger a lockout period</param>
    void BICListener::enableDistributedStim(bool enableDistributed, int sensingChannel, std::vector<double> filtCoeff_B, std::vector<double> filtCoeff_A, uint32_t triggeredFunctionIndex, double stimThreshold, double triggerPhase, double targetPhase)
    {
        distributedInputChannel = sensingChannel;
        betaBandPassIIR_B = filtCoeff_B;
        betaBandPassIIR_A = filtCoeff_A;
        distributedStimThreshold = stimThreshold;
        stimTriggerPhase = triggerPhase;
        stimTargetPhase = targetPhase;

        if (enableDistributed && !isTriggeringStimulation() && !isStimulating())
        {
            // Enable Closed Loop since it is not enabled and request is to enable
            // Instantiate the conditional variable for thread notification
            stimTrigger = new std::condition_variable();

            // Update state tracking variable
            isCLStimEn = true;

            // Start thread up
            distributedStimThread = new std::thread(&BICListener::triggeredSendStimThread, this);
        }
        else if (!enableDistributed && isCLStimEn)
        {
            // Update state tracking variable
            isCLStimEn = false;
            stimTrigger->notify_all();

            // Disable Closed Loop since it is enabled and request is to disable
            distributedStimThread->join();
            distributedStimThread->~thread();
            delete distributedStimThread;
            distributedStimThread = NULL;

            // Ensure stimulation is stopped
            theImplantedDevice->stopStimulation();

            // Clean up the conditional variable
            stimTrigger->~condition_variable();
            delete stimTrigger;
            stimTrigger = NULL;
        }
    }
    
    /// <summary>
    /// Thread to be started up when phase-triggered stimulation functionality is enabled.
    /// Delivers a specific stimulation pattern whe
    /// </summary>
    void BICListener::triggeredSendStimThread()
    {
        // Create a mutex to make the conditional 'wait' functionality
        std::mutex stimLock;
        std::unique_lock<std::mutex> stimTriggerWait(stimLock);

        // create instance of stimTimes to keep track of before and after stim timestamps
        StimTimes startStimulationTimes;
        // enable stim time logging
        enableStimTimeLogging(true);

        // initialize before and after timestamps
        std::chrono::system_clock::time_point before;
        std::chrono::system_clock::time_point after;

        // Wait for a zero crossing
        stimTrigger->wait(stimTriggerWait);

        // Loop while streaming is active
        while (isCLStimEn)
        {
            // Get time before start stimulation command (UTC)
            before = std::chrono::system_clock::now();
            startStimulationTimes.beforeStimTimeStamp = before.time_since_epoch().count();

            try
            {
                // Execute the stimulation command
                theImplantedDevice->startStimulation();

                // Get time after start stimulation command (UTC)
                after = std::chrono::system_clock::now();
                startStimulationTimes.afterStimTimeStamp = after.time_since_epoch().count();

                // If no exception encountered, mark it 0
                startStimulationTimes.recordedException = "0";
#ifdef DEBUG_CONSOLE_ENABLE
                std::cout << "DEBUG: finished stim in " << elapsed_sec.count() << "s\n";
#endif

                // Add latest received data packet to the buffer if there is room
                if (stimTimeSampleQueue.size() < 1000)
                {
                    // Lock the stim time buffer, add data, unlock
                    this->m_stimTimeBufferLock.lock();
                    stimTimeSampleQueue.push(startStimulationTimes); // add struct with timestamps and exception to queue
                    this->m_stimTimeBufferLock.unlock();

                    // Notify the streaming function that new data exists
                    stimTimeDataNotify->notify_all();
                }
                else
                {
                    std::cout << "WARNING: Before Stim Time Log Queue Size Overflow, streaming data skipped" << std::endl;
                }
            }
            catch (std::exception& anyException)
            {
                std::cout << "ERROR: Stimulation exception encountered: " << anyException.what() << std::endl;
                
                // If exception occurred, after is the timestamp when the exception occurred
                after = std::chrono::system_clock::now();
                startStimulationTimes.afterStimTimeStamp = after.time_since_epoch().count();
                // Also keep track of exception encountered
                startStimulationTimes.recordedException = anyException.what();

                if (stimTimeSampleQueue.size() < 1000)
                {
                    // Lock the stim time buffer, add data, unlock
                    this->m_stimTimeBufferLock.lock();
                    stimTimeSampleQueue.push(startStimulationTimes); // add struct with timestamps and exception to queue
                    this->m_stimTimeBufferLock.unlock();

                    // Notify the streaming function that new data exists
                    stimTimeDataNotify->notify_all();
                }
                else
                {
                    std::cout << "WARNING: Before Stim Time Log Queue Size Overflow, streaming data skipped" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "ERROR: Stimulation exception encountered. No reason" << std::endl;
            }

            // Wait for a zero crossing
            stimTrigger->wait(stimTriggerWait);
        }
    }

    /// <summary>
    /// Adds a pointer to the implanted device interface to the BICListener, required for phase-locked stim functionality
    /// </summary>
    /// <param name="anImplantedDevice">the active impalnted device</param>
    void BICListener::addImplantPointer(cortec::implantapi::IImplant* anImplantedDevice)
    {
        theImplantedDevice = anImplantedDevice;
    }

    /// <summary>
    /// Handles the time-domain processing to be performed on each new datapoint
    /// </summary>
    /// <param name="newData">Latest datapoint to be processed for potential triggering of stimulation</param>
    /// <param name="dataHistory">History of raw datapoints</param>
    /// <param name="stimHistory">History of stimulation onsets</param>
    /// <param name="hampelDataHistory">History of samples after going through Hampel filter</param>
    /// <param name="dcFiltHistory">History of samples after going through DC Block filter</param>
    /// <param name="filterGain">Gain of the IIR bandpass filter</param>
    /// <returns></returns>
    double BICListener::processingHelper(double newData, std::vector<double>* dataHistory, std::vector<double>* stimHistory, std::vector<double>* hampelDataHistory, std::vector<double>* dcFiltHistory, double filterGain)
    {   
        double stimCount = 0;
        double dcFiltSamp;
        double hampelSamp;
        double MAD;
        double medianVal;
        
        std::vector<double> modifier(dataHistory->size());
        std::vector<double> sorted(dataHistory->size());

        // store most recent raw sample
        dataHistory->insert(dataHistory->begin(), newData);
        dataHistory->pop_back();

        // Artifact rejection
        for (int i = 0; i < stimHistory->size(); i++)
        {
            stimCount += stimHistory->at(i);
        }

        // Blank artifact or send data through DC block filter
        if (stimCount > 0)
        {
            dcFiltSamp = hampelDataHistory->at(0);
        }
        else
        {
            dcFiltSamp = 0.945 * dcFiltHistory->at(0) + dataHistory->at(0) - dataHistory->at(1);
        }
        dcFiltHistory->insert(dcFiltHistory->begin(), dcFiltSamp);
        dcFiltHistory->pop_back();

        // Hampel filter for outlier detection
        // Identify median of a given window
        sorted = *dcFiltHistory;
        sort(sorted.begin(), sorted.end());
        medianVal = sorted[((sorted.size() - 1) / 2) + 1];

        // Calculate median absolute deviation (MAD)
        for (int i = 0; i < dcFiltHistory->size(); i++)
        {
            modifier[i] = abs(dcFiltHistory->at(i) - medianVal);
        }        

        sort(modifier.begin(), modifier.end());
        MAD = 1.4826 * modifier[((modifier.size() - 1) / 2) + 1];

        // Determine if a sample is an outlier and needs to be replaced by calculated median value
        if (abs(dcFiltSamp - medianVal) <= 3 * MAD) 
        {
            hampelSamp = dcFiltSamp;
        }
        else
        {
            hampelSamp = medianVal;
        }

        // Band pass filter for beta activity
        double filtSamp = filterIIR(hampelSamp, &bpFiltData, hampelDataHistory, &betaBandPassIIR_B, &betaBandPassIIR_A, 1);

        // if at a particular phase, above an arbitrary threshold, and closed loop stim is enabled, send stimulation
        if (!isSelfTrig && isCLStimEn && detectTriggerPhase(phaseData, stimTriggerPhase) && bpFiltData[0] > distributedStimThreshold)
        {
            // log that a valid target has been found
            isValidTarget = true;

            // start thread to execute stim command
            stimTrigger->notify_all();
        }
        else
        {
            isValidTarget = false;
        }

        // Return the filtered sample for visualization purposes
        return filtSamp;
    }


    /// <summary>
    /// Private function that applies an IIR filter to incoming neural data
    /// </summary>
    /// <param name="currSamp">Latest neural sample</param>
    /// <param name="prevFiltOut">History of filtered data samples</param>
    /// <param name="prevInput">History of raw data samples</param>
    /// <param name="b">B-array for IIR constants</param>
    /// <param name="a">A-array for IIR constants</param>
    /// <param name="gainVal">Gain for IIR bandpass filter</param>
    /// <returns>Current filtered output sample</returns>
    double BICListener::filterIIR(double currSamp, std::vector<double>* prevFiltOut, std::vector<double>* prevInput, std::vector<double>* b, std::vector<double>* a, double gainVal)
    {
       double filtTemp;

       // 2nd order IIR filter
       filtTemp = gainVal * b->at(0) * currSamp + gainVal * b->at(1) * prevInput->at(0) + gainVal * b->at(2) * prevInput->at(1) + gainVal * b->at(3) * prevInput->at(2) + gainVal * b->at(4) * prevInput->at(3)
            - a->at(1) * prevFiltOut->at(0) - a->at(2) * prevFiltOut->at(1) - a->at(3) * prevFiltOut->at(2) - a->at(4) * prevFiltOut->at(3);

        // remove the last sample and insert the most recent sample to the front of the vector
        prevFiltOut->insert(prevFiltOut->begin(), filtTemp);
        prevFiltOut->pop_back();

        // Store most recent sample at the beginning of history window
        prevInput->insert(prevInput->begin(), currSamp);
        prevInput->pop_back();

        return filtTemp;
    }

    /// <summary>
    /// Helper function for identifying if the latest point is a positive zero crossing
    /// </summary>
    /// <param name="dataArray">Vector of sensing data to assess for zero crossing</param>
    /// <returns>Boolean indicating if the latest point is a positive zero crossing</returns>
    bool BICListener::isZeroCrossing(std::vector<double> dataArray)
    {
        bool isZeroCrossing = false;

        // Check neighboring samples to determine for a zero-crossing
        if (dataArray[0] > 0)
        {
            if (dataArray[1] < 0)
            {
                isZeroCrossing = true;
            }
        }
        return isZeroCrossing;
    }
    
    /// <summary>
    /// Helper function to identify if a certain phase has passed
    /// </summary>
    /// <param name="prevPhase">Vector of previous phase data</param>
    /// <param name="triggerPhase">Current triggering phase value</param>
    /// <returns>Boolean indicating if the phase for triggering stim has passed</returns>
    bool BICListener::detectTriggerPhase(std::vector<double> prevPhase, double triggerPhase)
    {
        bool wasTriggerPhase = false;

        if (prevPhase[0] > triggerPhase && prevPhase[2] < triggerPhase)
        {
            wasTriggerPhase = true;
        }
        return wasTriggerPhase;
    }

    /// <summary>
    /// Function for calculating the phase of a sample
    /// </summary>
    /// <param name="dataArray">Vector of filtered sensing data</param>
    /// <param name="currTimeStamp">Timestamp of the current sample</param>
    /// <param name="prevSigFreq">Vector of previously calculated frequencies</param>
    /// <param name="prevPhase">Vector of previously calculated phases</param>
    /// <returns>Calculated phase of the current sample</returns>
    double BICListener::calcPhase(std::vector<double> dataArray, uint64_t currSamp, std::vector<double>* prevSigFreq, std::vector<double>* prevPhase)
    {
        double avgSigFreq = 0;
        double sigFreq = 0;
        double currPhase = 0;
        uint64_t sampDiff = 0;

        // Identify sample difference 
        sampDiff = currSamp - zeroSamp;

        // Check if there is a negative zero crossing
        // Estimate oscillation frequency
        if (isZeroCrossing(dataArray))
        {
            // If so, calculate the  frequency
            sigFreq = 1 / (sampDiff * 0.001);

            // Check that the calculated frequency is within reasonable bounds
            if (sigFreq > 10 && sigFreq < 30) 
            {
                prevSigFreq->insert(prevSigFreq->begin(), sigFreq);
                prevSigFreq->pop_back();
            }
            zeroSamp = currSamp;

            // Phase is 0 since it's a zero crossing
            currPhase = 0;
        }

        // Calculate the phase value using previously recorded signal frequencies
        else
        {
            for (int i = 0; i < prevSigFreq->size(); i++)
            {
                avgSigFreq += prevSigFreq->at(i);
            }
            avgSigFreq /= prevSigFreq->size();

            currPhase = 0.001 * sampDiff * avgSigFreq * 360;
        }

        // Save the calculated phase
        prevPhase->insert(prevPhase->begin(), currPhase);
        prevPhase->pop_back();

        // Check stimTriggerPhase is within range
        if (stimTriggerPhase <= 0 || stimTriggerPhase > 360)
        {
            // If not, reset stimTriggerPhase
            stimTriggerPhase = 45;
        }
        return currPhase;
    }

    /// <summary>
    /// Function for updating the triggering phase value
    /// </summary>
    /// <param name="prevStimPhase">phase of current sample that just triggered stimulation</param>
    void BICListener::updateTriggerPhase(double prevStimPhase)
    {
        double phaseDiff = 0;

        // find phase difference and use that to update stimTriggerPhase alongside arbitrary gain
        phaseDiff = prevStimPhase - stimTargetPhase; 
        stimTriggerPhase -= (0.1) * phaseDiff;

        // Checks to reset stimTriggerPhase if out of bounds
        if (stimTriggerPhase < 1 || stimTriggerPhase > 170)
        {
            stimTriggerPhase = 25;
        }
    }

    /// <summary>
    /// Function for detecting if too many stimulation pulses have been sent out consecutively
    /// </summary>
    /// <param name="stimSampArray">Container of sample numbers indicating the onset of stimulation</param>
    /// <param name="selfTrigThresh">Value dictating the maximum amount of time between two stimulation onsets to be considered self triggering </param>
    void BICListener::detectSelfTriggering(std::vector<int> stimSampArray, double selfTrigThresh)
    {
        int counter = 0;

        // Identify consecutive stimulation pulses
        for (int i = 0; i < stimSampArray.size() - 1; i++)
        {
            if ((stimSampArray[i] - stimSampArray[i + 1]) <= selfTrigThresh)
            {
                // For every instance they are back to back, increase the counter
                counter++;
            }
        }

        // Check if the number of self-triggering pulses exceeds the threshold limit and return boolean state
        if (counter < stimSampArray.size() - 1)
        {
            isSelfTrig = false;
        }
        else
        {
            isSelfTrig = true;
        }
    }

    /// <summary>
    /// Private function that reads stimulation time data from queue and record to a file
    /// </summary>
    void BICListener::logStimTimeThread()
    {
        std::mutex stimTimeDataLock;
        std::unique_lock<std::mutex> stimTimeDataWait(stimTimeDataLock);

        // Open File
        std::ofstream myFile;

        // Get current date and time
        time_t currTime;
        char timeStampBuffer[100];
        struct tm* curr_tm;
        time(&currTime);
        curr_tm = localtime(&currTime);

        // Format date and time
        strftime(timeStampBuffer, 100, "%m%d%Y_%I%M%S", curr_tm);
        std::string timeStamp(timeStampBuffer);

        // Append to the name of the stim logging file 
        std::string fileName = "stimTimeLog_" + timeStamp + ".csv";
        myFile.open(fileName, std::ios_base::app);
        myFile << "BeforeStim, AfterStim, Exception, triggerPhase" << "\n";
        myFile.close();

        // Loop while streaming is active
        while (stimTimeLoggingState)
        {
            // if either are empty, wait
            if (stimTimeSampleQueue.empty())
            {
                stimTimeDataNotify->wait(stimTimeDataWait);
            }
            else
            {
                try {
                    // Lock the buffer
                    this->m_stimTimeBufferLock.lock();
                    // Write out new line to file
                    myFile.open(fileName, std::ios_base::app);
                    // log timestamp before and after stim command and exception
                    myFile << stimTimeSampleQueue.front().beforeStimTimeStamp << ", " << stimTimeSampleQueue.front().afterStimTimeStamp << ", " << stimTimeSampleQueue.front().recordedException << ", " << stimTriggerPhase << "\n";
                    myFile.close();
                    // Clean up the current sample from the list
                    stimTimeSampleQueue.pop(); // take out first item of queue
                    // Unlock the neurobuffer
                    this->m_stimTimeBufferLock.unlock();
                }
                catch (std::exception& anyException)
                {
                    std::cout << "ERROR: Stim Time Logging Failed: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "ERROR: Stim Time Logging Failed. No reason." << std::endl;
                }
            }
        }
    }

    /// <summary>
    /// Function that enables logging of the time needed to perform stimulation
    /// </summary>
    /// <param name="enableSensing">Boolean to determine whether logging of stimulation time is enabled or not</param>
    void BICListener::enableStimTimeLogging(bool enableSensing)
    {
        // Determine action to be taken. Only take action if requested action matches potential actions based on current state.
        if (enableSensing && stimTimeLoggingState == false)
        {
            // Update state tracking variable
            stimTimeLoggingState = true;
            // Conditional variable for thread notification
            stimTimeDataNotify = new std::condition_variable();
            // Start thread
            stimTimeLoggingThread = new std::thread(&BICListener::logStimTimeThread, this);
        }
        else if (!enableSensing && stimTimeLoggingState == true)
        {
            // Shut down streaming. First notify and wait for streaming handling thread to stop.
            stimTimeLoggingState = false;
            stimTimeDataNotify->notify_all();
            stimTimeLoggingThread->join();

            // Delete generated resources to free memory
            stimTimeLoggingThread->~thread();
            delete stimTimeLoggingThread;
            stimTimeLoggingThread = NULL;

            stimTimeDataNotify->~condition_variable();
            delete stimTimeDataNotify;
            stimTimeDataNotify = NULL;
        }
    }

    /// <summary>
    /// Function that enables the delivery of open-loop stimulation by continually re-triggering stimulation at a defined interval
    /// </summary>
    /// <param name="enableOpenLoop">Boolean indicating if open-loop stimulation should be enabled or disabled</param>
    /// <param name="watchdogInterval">Interval that the stimulation should be stopped and restarted in milliseconds</param>
    void BICListener::enableOpenLoopStim(bool enableOpenLoop, uint32_t watchdogInterval)
    {
        if (enableOpenLoop && !isTriggeringStimulation() && !isStimulating())
        {
            // Update state tracking variable
            isOLStimEn = true;
            if (watchdogInterval > 10)
            {
                openLoopSleepTimeDuration = watchdogInterval;
            }
            else
            {
                openLoopSleepTimeDuration = 10;
            }

            // Start thread up
            openLoopStimThread = new std::thread(&BICListener::openLoopStimLoopThread, this);
        }
        else if (!enableOpenLoop && isOLStimEn)
        {
            // Update state tracking variable
            isOLStimEn = false;
            
            // Ensure stimulation is stopped
            theImplantedDevice->stopStimulation();

            // Disable Closed Loop since it is enabled and request is to disable
            openLoopStimThread->join();
            openLoopStimThread->~thread();
            delete openLoopStimThread;
            openLoopStimThread = NULL;
        }
    }

    void BICListener::openLoopStimLoopThread(void)
    {
        // Create instance of stimTimes to keep track of before and after stim timestamps
        StimTimes startStimulationTimes;
        // Enable stim time logging
        enableStimTimeLogging(true);

        // initialize before and after timestamps
        std::chrono::system_clock::time_point before;
        std::chrono::system_clock::time_point after;

        while (isOLStimEn)
        {
            // Get time before start stimulation command (UTC)
            before = std::chrono::system_clock::now();
            startStimulationTimes.beforeStimTimeStamp = before.time_since_epoch().count();

            try {
                // Check if stimulation is already occuring- once stimulation is no longer active, retrigger
                while (isStimulating())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1)); // sleep for 1 ms before checking again
                }

                // Re-trigger stimulation
                theImplantedDevice->startStimulation();

                //Get time after start stimulation command (UTC)
                after = std::chrono::system_clock::now();
                startStimulationTimes.afterStimTimeStamp = after.time_since_epoch().count();

                // No exception encountered, so label as "0"
                startStimulationTimes.recordedException = "0";

                // Add latest received data packet to the buffer if there is room
                if (stimTimeSampleQueue.size() < 1000)
                {
                    // Lock the stim time buffer, add data, unlock
                    this->m_stimTimeBufferLock.lock();
                    stimTimeSampleQueue.push(startStimulationTimes); // add struct with timestamps and exception to queue
                    this->m_stimTimeBufferLock.unlock();

                    // Notify the streaming function that new data exists
                    stimTimeDataNotify->notify_all();
                }
                else
                {
                    std::cout << "WARNING: Before Stim Time Log Queue Size Overflow, streaming data skipped" << std::endl;
                }

                // Sleep for the triggering duration
                std::this_thread::sleep_for(std::chrono::milliseconds(openLoopSleepTimeDuration));
            }
            catch (std::exception& anyException)
            {
                std::cout << "ERROR: Open Loop Management Exception Encountered. Reason: " << anyException.what() << std::endl;

                // If exception occurred, after is the time the exception occurred
                after = std::chrono::system_clock::now();
                startStimulationTimes.afterStimTimeStamp = after.time_since_epoch().count();

                // Also keep track of exception encountered
                startStimulationTimes.recordedException = anyException.what();

                if (stimTimeSampleQueue.size() < 1000)
                {
                    // Lock the stim time buffer, add data, unlock
                    this->m_stimTimeBufferLock.lock();
                    stimTimeSampleQueue.push(startStimulationTimes); // add struct with timestamps and exception to queue
                    this->m_stimTimeBufferLock.unlock();
                }
                else
                {
                    std::cout << "WARNING: Before Stim Time Log Queue Size Overflow, streaming data skipped" << std::endl;
                }
            }
            catch (...)
            {
                std::cout << "ERROR: Open Loop Management Exception Encountered. No reason." << std::endl;
            }
        }
    }

    /// <summary>
    /// Status variable indicating if triggering stimulation is already ongoing.
    /// </summary>
    /// <returns>Boolean if open-loop or closed-loop stimulation is enabled.</returns>
    bool BICListener::isTriggeringStimulation()
    {
        return isCLStimEn || isOLStimEn;
    }

    //*************************************************** Power Data Streaming Functions ***************************************************
    /// <summary>
    /// Enable or disable power streaming to a gRPC client. 
    /// Function instructs BIC to start streaming, resulting in data being received by various power event handler functions.  
    /// Events puts data in a queue for independent transmission to client by thread utilizing "grpcPowerStreamThread" function.
    /// </summary>
    /// <param name="enableSensing">True if streaming is being requested to be enabled, false otherwise</param>
    /// <param name="aWriter">gRPC client writing interface.</param>
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
                    std::cout << "ERROR: GRPC Write Failed: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "ERROR: GRPC Write Buffer Failed. No reason." << std::endl;
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
                std::cout << "WARNING: GRPC Power Queue Size Overflow, streaming data skipped" << std::endl;
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
                std::cout << "WARNING: GRPC Power Queue Size Overflow, streaming data skipped" << std::endl;
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
                std::cout << "WARNING: GRPC Power Queue Size Overflow, streaming data skipped" << std::endl;
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
                    std::cout << "ERROR: GRPC Write Failed: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "ERROR: GRPC Write Buffer Failed. No reason." << std::endl;
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
                std::cout << "WARNING: GRPC Temperature Queue Size Overflow, streaming data skipped" << std::endl;
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
                    std::cout << "ERROR: GRPC Write Failed: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "ERROR: GRPC Write Buffer Failed. No reason." << std::endl;
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
                std::cout << "WARNING: GRPC Humidity Queue Size Overflow, streaming data skipped" << std::endl;
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
                    std::cout << "ERROR: GRPC Write Failed. Reason: " << anyException.what() << std::endl;
                }
                catch (...)
                {
                    std::cout << "ERROR: GRPC Write Buffer Failed. No reason." << std::endl;
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
                std::cout << "WARNING: GRPC Error Queue Size Overflow, streaming data skipped" << std::endl;
            }
        }
    }

    /// <summary>
    /// Brain Interchange event handler for processing rate errors. This event is called if the neural data is not being consumed quickly enough by the gRPC microservice.
    /// </summary>
    void BICListener::onDataProcessingTooSlow()
    {
        // Important event, write it out to the console
        std::cout << "CRITICAL WARNING: Data processing too slow" << std::endl;
        
        // If gRPC error streaming is enabled, write out the update
        if (errorStreamingState)
        {
            ErrorUpdate* errorMessage = new ErrorUpdate();
            errorMessage->set_message("CRITICAL WARNING: Data processing too slow");

            // Add it to the buffer if there is room
            if (errorSampleQueue.size() < 100)
            {
                errorSampleQueue.push(errorMessage);
                errorDataNotify->notify_all();
            }
            else
            {
                delete errorMessage;
                std::cout << "WARNING: GRPC Error Queue Size Overflow, streaming data skipped" << std::endl;
            }
        }
    }
}
