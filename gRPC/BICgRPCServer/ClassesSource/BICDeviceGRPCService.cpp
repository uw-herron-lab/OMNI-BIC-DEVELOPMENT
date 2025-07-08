#include "BICDeviceGRPCService.h"

// BIC Usings
using namespace cortec::implantapi;
using namespace BICGRPCHelperNamespace;

// GRPC Usings
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

// BIC Device Service Usings
using BICgRPC::RequestDeviceAddress;
using BICgRPC::BICDeviceService;
using BICgRPC::bicSuccessReply;
using BICgRPC::ScanDevicesRequest;
using BICgRPC::ScanDevicesReply;
using BICgRPC::ConnectDeviceRequest;
using BICgRPC::bicGetImplantInfoRequest;
using BICgRPC::bicGetImplantInfoReply;
using BICgRPC::bicGetImplantInfoReply_bicChannelInfo;
using BICgRPC::bicGetImpedanceRequest;
using BICgRPC::bicGetImpedanceReply;
using BICgRPC::bicGetTemperatureReply;
using BICgRPC::bicGetHumidityReply;
using BICgRPC::bicNeuralSetStreamingEnable;
using BICgRPC::bicSetImplantPowerRequest;
using BICgRPC::bicStartStimulationRequest;
using BICgRPC::bicEnqueueStimulationRequest;
using BICgRPC::StimulationFunctionDefinition;
using BICgRPC::TemperatureUpdate;
using BICgRPC::HumidityUpdate;
using BICgRPC::NeuralUpdate;
using BICgRPC::PowerUpdate;
using BICgRPC::ConnectionUpdate;
using BICgRPC::ErrorUpdate;
using BICgRPC::NeuralSample;

namespace BICGRPCHelperNamespace
{
    // ************************* Non-GRPC Helper Service Function Declarations *************************
    void BICDeviceGRPCService::passFactory(cortec::implantapi::IImplantFactory* serverFactory)
    {
        theImplantFactory = serverFactory;
    }

    void BICDeviceGRPCService::controlDispose()
    {
        for (auto it = deviceDirectory.begin(); it != deviceDirectory.end(); it++)
        {
            // Stop all streaming!
            it->second->tempStreamNotify.notify_all();
            it->second->humidStreamNotify.notify_all();
            it->second->neuralStreamNotify.notify_all();
            it->second->connectionStreamNotify.notify_all();
            it->second->errorStreamNotify.notify_all();
            it->second->powerStreamNotify.notify_all();

            // Dispose the things!
            it->second->theImplant->setImplantPower(false);
            it->second->theImplant->~IImplant();
        }
    }

    // ************************* Construction, Initialization, and Destruction Function Declarations *************************
    grpc::Status BICDeviceGRPCService::ScanDevices(grpc::ServerContext* context, const BICgRPC::ScanDevicesRequest* request, BICgRPC::ScanDevicesReply* reply)  {

        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcServiceLock);

        // Discover external units
        std::vector<CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();
        if (exInfos.empty())
        {
            reply->set_name("");
            return Status::OK;
        }

        // Find specified exeternal unit
        for (int i = 0; i < exInfos.size(); i++)
        {
            if ("//bic/bridge/" + exInfos[i]->getDeviceId() == request->bridgename())
            {
                CImplantInfo* theImplantInfo = theImplantFactory->getImplantInfo(*exInfos.at(i));

                // Set the fields of the response with the Implant Information
                bicGetImplantInfoReply* responseImplantInfo = reply->mutable_discovereddevice();
                responseImplantInfo->set_firmwareversion(theImplantInfo->getFirmwareVersion());
                responseImplantInfo->set_devicetype(theImplantInfo->getDeviceType());
                responseImplantInfo->set_deviceid(theImplantInfo->getDeviceId());
                responseImplantInfo->set_measurementchannelcount(theImplantInfo->getMeasurementChannelCount());
                responseImplantInfo->set_stimulationchannelcount(theImplantInfo->getStimulationChannelCount());
                responseImplantInfo->set_samplingrate(theImplantInfo->getSamplingRate());

                int64_t numChannels = theImplantInfo->getChannelCount();
                responseImplantInfo->set_channelcount(numChannels);

                for (int j = 0; j < numChannels; j++)
                {
                    CChannelInfo* sourceChannel = theImplantInfo->getChannelInfo()[i];
                    bicGetImplantInfoReply_bicChannelInfo* addChannel = responseImplantInfo->add_channelinfolist();

                    addChannel->set_canmeasure(sourceChannel->canMeasure());
                    addChannel->set_measurevaluemin(sourceChannel->getMeasureValueMin());
                    addChannel->set_measurevaluemax(sourceChannel->getMeasureValueMax());
                    addChannel->set_canstimulate(sourceChannel->canStimulate());
                    addChannel->set_stimulationunit((bicGetImplantInfoReply::bicChannelInfo::UnitType)(sourceChannel->getStimulationUnit()));
                    addChannel->set_stimvaluemin(sourceChannel->getStimValueMin());
                    addChannel->set_stimvaluemax(sourceChannel->getStimValueMax());
                }
                reply->set_name(request->bridgename() + "/device/" + theImplantInfo->getDeviceId());
                return Status::OK;
            }
        }

        reply->set_name("");
        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::ConnectDevice(grpc::ServerContext* context, const BICgRPC::ConnectDeviceRequest* request, BICgRPC::bicSuccessReply* reply)  {
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcServiceLock);

        // Split requested bridge/address strings from request
        int64_t deviceIdIndex = request->deviceaddress().find("/device/");
        std::string deviceId = request->deviceaddress().substr(((int64_t)deviceIdIndex) + 8);
        std::string bridgeId = request->deviceaddress().substr(13, ((int64_t)deviceIdIndex) - 13);

        // Discover external units
        std::vector<CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();
        if (exInfos.empty())
        {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "No Bridges");
        }

        // Find the Bridge, confirm that device/bridge pair match
        int exInfoIndex = 0;
        CImplantInfo* theImplantInfo = NULL;
        for (exInfoIndex; exInfoIndex < exInfos.size(); exInfoIndex++)
        {
            if (exInfos[exInfoIndex]->getDeviceId() == bridgeId)
            {
                // pull the info from the device
                theImplantInfo = theImplantFactory->getImplantInfo(*exInfos.at(exInfoIndex));

                // Check that deviceId matches
                if (theImplantInfo->getDeviceId() != deviceId)
                {
                    return grpc::Status(grpc::StatusCode::NOT_FOUND, "Bridge/Device ID Mismatch");
                }

                break;
            }
        }

        // Check if the bridge wasn't found
        if (theImplantInfo == NULL)
        {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "No Bridge Found with Id");
        }
        else
        {
            // Found Device/Bridge Match - Proceed with System Initialization
            theImplants.push_back(std::unique_ptr<BICDeviceInfoStruct>(new BICDeviceInfoStruct()));
            std::pair<std::string, BICDeviceInfoStruct*> newKeyPair;
            newKeyPair.first = request->deviceaddress();
            newKeyPair.second = theImplants.back().get();
            newKeyPair.second->theImplant.reset(theImplantFactory->create(*exInfos.at(exInfoIndex), *theImplantInfo));
            newKeyPair.second->listener.reset(new BICListener());
            newKeyPair.second->listener.get()->addImplantPointer(newKeyPair.second->theImplant.get());
            newKeyPair.second->theImplant->registerListener(newKeyPair.second->listener.get());
            newKeyPair.second->theImplantInfo.reset(theImplantInfo);
            newKeyPair.second->deviceAddress = request->deviceaddress();
            newKeyPair.second->bridgeId = bridgeId;
            newKeyPair.second->deviceId = deviceId;

            deviceDirectory.insert(newKeyPair);

            // Everything went ok
            return grpc::Status::OK;
        }
    }

    grpc::Status BICDeviceGRPCService::bicDispose(grpc::ServerContext* context, const BICgRPC::RequestDeviceAddress* request, BICgRPC::bicSuccessReply* reply)  {

        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcServiceLock);

        // Check if the requested device exists
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            // Not found!
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Stop all streaming!
        deviceDirectory[request->deviceaddress()]->tempStreamNotify.notify_all();
        deviceDirectory[request->deviceaddress()]->humidStreamNotify.notify_all();
        deviceDirectory[request->deviceaddress()]->neuralStreamNotify.notify_all();
        deviceDirectory[request->deviceaddress()]->connectionStreamNotify.notify_all();
        deviceDirectory[request->deviceaddress()]->errorStreamNotify.notify_all();
        deviceDirectory[request->deviceaddress()]->powerStreamNotify.notify_all();

        // Dispose the things!
        deviceDirectory[request->deviceaddress()]->theImplant->setImplantPower(false);
        deviceDirectory[request->deviceaddress()]->theImplant->~IImplant();
        deviceDirectory.erase(request->deviceaddress());

        // Respond to client
        return Status::OK;
    }

    // ************************* Implant State Get and Set Function Declarations *************************
    grpc::Status BICDeviceGRPCService::bicGetImplantInfo(grpc::ServerContext* context, const BICgRPC::bicGetImplantInfoRequest* request, BICgRPC::bicGetImplantInfoReply* reply)  {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            // Not found!
            reply->set_success("error: not initialized");
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Check if we need to update cache
        if (request->updatecachedinfo())
        {
            try
            {
                deviceDirectory[request->deviceaddress()]->theImplantInfo.reset(deviceDirectory[request->deviceaddress()]->theImplant->getImplantInfo());
            }
            catch (const std::exception& theError)
            {
                std::string returnMessage = "error: exception - ";
                returnMessage += theError.what();
                reply->set_success(returnMessage);
                return grpc::Status::OK;
            }
        }

        // Set the fields of the response with the Implant Information
        reply->set_firmwareversion(deviceDirectory[request->deviceaddress()]->theImplantInfo->getFirmwareVersion());
        reply->set_devicetype(deviceDirectory[request->deviceaddress()]->theImplantInfo->getDeviceType());
        reply->set_deviceid(deviceDirectory[request->deviceaddress()]->theImplantInfo->getDeviceId());
        reply->set_measurementchannelcount(deviceDirectory[request->deviceaddress()]->theImplantInfo->getMeasurementChannelCount());
        reply->set_stimulationchannelcount(deviceDirectory[request->deviceaddress()]->theImplantInfo->getStimulationChannelCount());
        reply->set_samplingrate(deviceDirectory[request->deviceaddress()]->theImplantInfo->getSamplingRate());

        int64_t numChannels = deviceDirectory[request->deviceaddress()]->theImplantInfo->getChannelCount();
        reply->set_channelcount(numChannels);

        for (int64_t i = 0; i < numChannels; i++)
        {
            CChannelInfo* sourceChannel = deviceDirectory[request->deviceaddress()]->theImplantInfo->getChannelInfo()[i];
            bicGetImplantInfoReply_bicChannelInfo* addChannel = reply->add_channelinfolist();

            addChannel->set_canmeasure(sourceChannel->canMeasure());
            addChannel->set_measurevaluemin(sourceChannel->getMeasureValueMin());
            addChannel->set_measurevaluemax(sourceChannel->getMeasureValueMax());
            addChannel->set_canstimulate(sourceChannel->canStimulate());
            addChannel->set_stimulationunit((bicGetImplantInfoReply::bicChannelInfo::UnitType)(sourceChannel->getStimulationUnit()));
            addChannel->set_stimvaluemin(sourceChannel->getStimValueMin());
            addChannel->set_stimvaluemax(sourceChannel->getStimValueMax());
        }

        // Respond to client
        reply->set_success("success");
        return Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicGetImpedance(grpc::ServerContext* context, const BICgRPC::bicGetImpedanceRequest* request, BICgRPC::bicGetImpedanceReply* reply)  {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            // Not found!
            reply->set_success("error: not initialized");
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        double impedanceValue;
        try
        {
            impedanceValue = deviceDirectory[request->deviceaddress()]->theImplant->getImpedance(request->channel());
        }
        catch (const std::exception& theError)
        {
            std::string returnMessage = "error: exception - ";
            returnMessage += theError.what();
            reply->set_success(returnMessage);
            return grpc::Status::OK;
        }

        // Respond to client
        reply->set_channelimpedance(impedanceValue);
        reply->set_units("ohms");
        reply->set_success("success");

        // Notify server about impedance check
        std::cout << "CH " << request->channel() << ": success" << std::endl;
        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicGetTemperature(grpc::ServerContext* context, const BICgRPC::RequestDeviceAddress* request, BICgRPC::bicGetTemperatureReply* reply)  {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            // Not found!
            reply->set_success("error: not initialized");
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        double temperatureValue;
        try
        {
            temperatureValue = deviceDirectory[request->deviceaddress()]->theImplant->getTemperature();
        }
        catch (const std::exception& theError)
        {
            std::string returnMessage = "error: exception - ";
            returnMessage += theError.what();
            reply->set_success(returnMessage);
            return grpc::Status::OK;
        }

        // Respond to client
        reply->set_temperature(temperatureValue);
        reply->set_units("celsius");
        reply->set_success("success");
        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicGetHumidity(grpc::ServerContext* context, const BICgRPC::RequestDeviceAddress* request, BICgRPC::bicGetHumidityReply* reply)  {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            // Not found!
            reply->set_success("error: not initialized");
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        double humidityValue;
        try
        {
            humidityValue = deviceDirectory[request->deviceaddress()]->theImplant->getHumidity();
        }
        catch (const std::exception& theError)
        {
            std::string returnMessage = "error: exception - ";
            returnMessage += theError.what();
            reply->set_success(returnMessage);
            return grpc::Status::OK;
        }

        // Respond to client
        reply->set_humidity(humidityValue);
        reply->set_units("celsius");
        reply->set_success("success");
        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicSetImplantPower(grpc::ServerContext* context, const BICgRPC::bicSetImplantPowerRequest* request, BICgRPC::bicSuccessReply* reply)  {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        try
        {
            deviceDirectory[request->deviceaddress()]->theImplant->setImplantPower(request->powerenabled());
        }
        catch (const std::exception&)
        {
            // TODO Exception Handling
            return grpc::Status::OK;
        }

        // Respond to client=
        return grpc::Status::OK;
    }

    // ************************* Streaming Control Function Declarations *************************
    grpc::Status BICDeviceGRPCService::bicTemperatureStream(grpc::ServerContext* context, const BICgRPC::bicSetStreamEnable* request, grpc::ServerWriter<BICgRPC::TemperatureUpdate>* writer)  {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->listener->temperatureStreamingState && request->enable())
        {
            // Not already streaming and requesting enable
            deviceDirectory[request->deviceaddress()]->listener->enableTemperatureStreaming(true, writer);

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->tempStreamLock);
            deviceDirectory[request->deviceaddress()]->tempStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->enableTemperatureStreaming(false, NULL);
        }
        else if (deviceDirectory[request->deviceaddress()]->listener->temperatureStreamingState && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send grpc::Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return grpc::Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->tempStreamNotify.notify_one();
        }

        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicHumidityStream(grpc::ServerContext* context, const BICgRPC::bicSetStreamEnable* request, grpc::ServerWriter<BICgRPC::HumidityUpdate>* writer)  {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->listener->humidityStreamingState && request->enable())
        {
            // Not already streaming and requesting enable
            deviceDirectory[request->deviceaddress()]->listener->enableHumidityeStreaming(true, writer);

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->humidStreamLock);
            deviceDirectory[request->deviceaddress()]->humidStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->enableHumidityeStreaming(false, NULL);
        }
        else if (deviceDirectory[request->deviceaddress()]->listener->humidityStreamingState && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send grpc::Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return grpc::Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->humidStreamNotify.notify_one();
        }

        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicConnectionStream(grpc::ServerContext* context, const BICgRPC::bicSetStreamEnable* request, grpc::ServerWriter<BICgRPC::ConnectionUpdate>* writer)  {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->listener->connectionStreamingState && request->enable())
        {
            // Not already streaming and requesting enable
            deviceDirectory[request->deviceaddress()]->listener->enableConnectionStreaming(true, writer);

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->connectionStreamLock);
            deviceDirectory[request->deviceaddress()]->connectionStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->enableConnectionStreaming(false, NULL);
        }
        else if (deviceDirectory[request->deviceaddress()]->listener->connectionStreamingState && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send grpc::Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return grpc::Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->connectionStreamNotify.notify_one();
        }

        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicErrorStream(grpc::ServerContext* context, const BICgRPC::bicSetStreamEnable* request, grpc::ServerWriter<BICgRPC::ErrorUpdate>* writer)  {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->listener->errorStreamingState && request->enable())
        {
            // Not already streaming and requesting enable
            deviceDirectory[request->deviceaddress()]->listener->enableErrorStreaming(true, writer);

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->errorStreamLock);
            deviceDirectory[request->deviceaddress()]->errorStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->enableErrorStreaming(false, NULL);
        }
        else if (deviceDirectory[request->deviceaddress()]->listener->errorStreamingState && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send grpc::Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return grpc::Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->errorStreamNotify.notify_one();
        }

        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicPowerStream(grpc::ServerContext* context, const BICgRPC::bicSetStreamEnable* request, grpc::ServerWriter<BICgRPC::PowerUpdate>* writer)  {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->listener->powerStreamingState && request->enable())
        {
            // Not already streaming and requesting enable
            deviceDirectory[request->deviceaddress()]->listener->enablePowerStreaming(true, writer);

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->powerStreamLock);
            deviceDirectory[request->deviceaddress()]->powerStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->enablePowerStreaming(false, NULL);
        }
        else if (deviceDirectory[request->deviceaddress()]->listener->powerStreamingState && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send grpc::Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return grpc::Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->powerStreamNotify.notify_one();
        }

        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicNeuralStream(grpc::ServerContext* context, const BICgRPC::bicNeuralSetStreamingEnable* request, grpc::ServerWriter<BICgRPC::NeuralUpdate>* writer)  {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->listener->neuralStreamingState && request->enable())
        { 
            // Not already streaming and requesting enable
            // Configure reference electrodes
            std::set<uint32_t> referenceElectrodes;
            for (int i = 0; i < request->refchannels_size(); i++)
            {
                referenceElectrodes.insert(referenceElectrodes.begin(), request->refchannels()[i]);
            }

            // Configure buffers and state variables for streaming start
            deviceDirectory[request->deviceaddress()]->listener->enableNeuralStreaming(true, request->buffersize(), request->maxinterpolationpoints(), writer);

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->neuralStreamLock);

            // Start measurement until lock is called
            deviceDirectory[request->deviceaddress()]->theImplant->startMeasurement(referenceElectrodes, (RecordingAmplificationFactor)request->amplificationfactor(), request->usegroundreference());
            deviceDirectory[request->deviceaddress()]->neuralStreamNotify.wait(StreamLockInst);
            deviceDirectory[request->deviceaddress()]->theImplant->stopMeasurement();

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->enableNeuralStreaming(false, 0, 0, NULL);
        }
        else if (deviceDirectory[request->deviceaddress()]->listener->neuralStreamingState && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send grpc::Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return grpc::Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->neuralStreamNotify.notify_one();
        }

        return grpc::Status::OK;
    }

    // ************************* Stimulation Control Function Declarations *************************
    grpc::Status BICDeviceGRPCService::bicStartStimulation(grpc::ServerContext* context, const BICgRPC::bicStartStimulationRequest* request, BICgRPC::bicSuccessReply* reply)  {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        try
        {
            if (deviceDirectory[request->deviceaddress()]->lastEnqueueType == StimulationMode::STIM_MODE_PERSISTENT_FUNC_PRELOADING)
            {
                deviceDirectory[request->deviceaddress()]->theImplant->startStimulation(request->functionindex());
            }
            else
            {
                deviceDirectory[request->deviceaddress()]->theImplant->startStimulation();
            }
        }
        catch (const std::exception theExeption)
        {
            // TODO Exception Handling
            std::cout << "Start Stimulation Exception: " << theExeption.what() << std::endl;
            return grpc::Status::OK;
        }

        // Respond to client
        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicStopStimulation(grpc::ServerContext* context, const BICgRPC::RequestDeviceAddress* request, BICgRPC::bicSuccessReply* reply)  {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        if (deviceDirectory[request->deviceaddress()]->listener->isStimulating())
        {
            try
            {
                deviceDirectory[request->deviceaddress()]->theImplant->stopStimulation();
            }
            catch (const std::exception&)
            {
                // TODO Exception Handling
                return grpc::Status::OK;
            }
        }

        // Respond to client
        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::bicEnqueueStimulation(grpc::ServerContext* context, const BICgRPC::bicEnqueueStimulationRequest* request, BICgRPC::bicSuccessReply* reply)
    {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Create objects required for 
        std::unique_ptr<IStimulationCommandFactory> theFactory(createStimulationCommandFactory());
        IStimulationCommand* theStimulationCommand = theFactory->createStimulationCommand();
        theStimulationCommand->setRepetitions(request->waveformrepititions());

        // Iterate through provided functions and add them to the command 
        try
        {
            for (int i = 0; i < request->functions_size(); i++)
            {
                // Set general function parameters
                IStimulationFunction* theFunction = theFactory->createStimulationFunction();
                theFunction->setName(request->functions().at(i).functionname());

                // Check which requested type of function requested
                if (request->functions().at(i).has_stimpulse())
                {
                    // Set repetition field
                    theFunction->setRepetitions(request->functions().at(i).stimpulse().pulserepetitions(), request->functions().at(i).stimpulse().burstrepetitions());

                    // Pull out the electrodes and set the electrode fields
                    std::set<uint32_t> sources;
                    std::set<uint32_t> sinks;
                    for (int j = 0; j < request->functions().at(i).stimpulse().sourceelectrodes().size(); j++)
                    {
                        sources.insert(request->functions().at(i).stimpulse().sourceelectrodes()[j]);
                    }
                    for (int j = 0; j < request->functions().at(i).stimpulse().sinkelectrodes().size(); j++)
                    {
                        sinks.insert(request->functions().at(i).stimpulse().sinkelectrodes()[j]);
                    }
                    theFunction->setVirtualStimulationElectrodes(sources, sinks, request->functions().at(i).stimpulse().useground());

                    // Generate the stimulation pulse by assembling atoms and appending them to the function
                    // Generate Atoms -- positive  pulse
                    theFunction->append(theFactory->createRect4AmplitudeStimulationAtom(
                        request->functions().at(i).stimpulse().amplitude()[0],
                        request->functions().at(i).stimpulse().amplitude()[1],
                        request->functions().at(i).stimpulse().amplitude()[2],
                        request->functions().at(i).stimpulse().amplitude()[3],
                        request->functions().at(i).stimpulse().pulsewidth()));

                    // Generate atoms -- DZ0
                    theFunction->append(theFactory->createRect4AmplitudeStimulationAtom(0, 0, 0, 0, request->functions().at(i).stimpulse().dz0duration()));

                    // Genmerate atoms -- charge balance ( based on charge balance ratio - does this have to be 4?)
                    theFunction->append(theFactory->createRect4AmplitudeStimulationAtom(
                        request->functions().at(i).stimpulse().amplitude()[0] / -4,
                        request->functions().at(i).stimpulse().amplitude()[1] / -4,
                        request->functions().at(i).stimpulse().amplitude()[2] / -4,
                        request->functions().at(i).stimpulse().amplitude()[3] / -4,
                        request->functions().at(i).stimpulse().pulsewidth() * 4));

                    // Generate atoms -- DZ0
                    // TODO - SHOULD THIS BE HERE?
                    theFunction->append(theFactory->createRect4AmplitudeStimulationAtom(0, 0, 0, 0, request->functions().at(i).stimpulse().dz0duration()));

                    // Generate atoms -- DZ1
                    theFunction->append(theFactory->createRect4AmplitudeStimulationAtom(0, 0, 0, 0, request->functions().at(i).stimpulse().dz1duration()));

                    // Add the function to the command
                    theStimulationCommand->append(theFunction);
                }
                else if (request->functions().at(i).has_pause())
                {
                    // Create the pulse function
                    theFunction->append(theFactory->createStimulationPauseAtom(request->functions().at(i).pause().duration()));

                    // Add the function to the command
                    theStimulationCommand->append(theFunction);
                }
                else
                {
                    // No atoms?
                }
            }

            // Stimulation Command created, now enqueue
            deviceDirectory[request->deviceaddress()]->theImplant->enqueueStimulationCommand(theStimulationCommand, (StimulationMode)request->mode());
            deviceDirectory[request->deviceaddress()]->lastEnqueueType = (StimulationMode)request->mode();
            delete theStimulationCommand;
        }
        catch (const std::exception theExeption)
        {
            // TODO Exception Handling
            std::cout << "Define Stimulation Waveform Exception: " << theExeption.what() << std::endl;
            return grpc::Status::OK;
        }

        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::enableDistributedStimulation(grpc::ServerContext* context, const BICgRPC::distributedStimEnableRequest* request, BICgRPC::bicSuccessReply* reply)
    {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Check if parameters are valid
        if (request->sensingchannel() < 0 || request->sensingchannel() > 31)
        {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Arguments out of range");
        }

        // Check that there are at least three coefficients for filtering
        if (request->filtercoefficients_b_size() < 3 || request->filtercoefficients_a_size() < 3)
        {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Need at least 3 values for filter coefficients");
        }

        // Check that starting trigger phase is valid
        if (request->inittriggerstimphase() < 0 || request->inittriggerstimphase() > 360)
        {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Arguments out of range");
        }

        // Check that target phase is valid
        if (request->targetphase() < 0 || request->targetphase() > 360)
        {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Arguments out of range");
        }

        // Grab all coefficient values and store in a vector
        std::vector<double> coefficients_B;
        std::vector<double> coefficients_A;
        for (int i = 0; i < request->filtercoefficients_b_size(); i++)
        {
            coefficients_B.push_back(request->filtercoefficients_b()[i]);
        }
        for (int i = 0; i < request->filtercoefficients_a_size(); i++)
        {
            coefficients_A.push_back(request->filtercoefficients_a()[i]);
        }

        // Perform the operation
        deviceDirectory[request->deviceaddress()]->listener->enableDistributedStim(request->enable(), request->sensingchannel(), coefficients_B, coefficients_A, request->triggeredfunctionindex(), request->triggerstimthreshold(), request->inittriggerstimphase(), request->targetphase());

        // Respond to client
        return grpc::Status::OK;
    }

    grpc::Status BICDeviceGRPCService::enableOpenLoopStimulation(grpc::ServerContext* context, const BICgRPC::openLoopStimEnableRequest* request, BICgRPC::bicSuccessReply* reply) {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        deviceDirectory[request->deviceaddress()]->listener->enableOpenLoopStim(request->enable(), request->watchdoginterval());

        // Respond to client
        return grpc::Status::OK;
    }
}