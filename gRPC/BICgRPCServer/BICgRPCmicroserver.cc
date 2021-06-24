#include <cppapi/bicapi.h>
#include <cppapi/IImplantListener.h>
#include <cppapi/IImplantFactory.h>
#include <cppapi/IStimulationCommand.h>
#include <cppapi/IStimulationFunction.h>
#include <cppapi/ImplantInfo.h>
#include <cppapi/ExternalUnitInfo.h>
#include <cppapi/IImplant.h>
#include <cppapi/Sample.h>
#include <cppapi/IStimulationCommandFactory.h>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <google/protobuf/any.pb.h>
#include <google/protobuf/empty.pb.h>

#include <mutex>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <cppapi/bic3232constants.h>

#include "BICgRPC.grpc.pb.h"

// BIC Usings
using namespace cortec::implantapi;

// GRPC Usings
using grpc::Server;
using grpc::ServerBuilder;
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
using BICgRPC::bicStimulationFunctionDefinitionRequest;
using BICgRPC::StimulationFunctionDefinition;
using BICgRPC::TemperatureUpdate;
using BICgRPC::HumidityUpdate;
using BICgRPC::NeuralUpdate;
using BICgRPC::PowerUpdate;
using BICgRPC::ConnectionUpdate;
using BICgRPC::ErrorUpdate;
using BICgRPC::NeuralSample;

// BIC Bridge Service Usings
using BICgRPC::BICBridgeService;
using BICgRPC::Bridge;
using BICgRPC::QueryBridgesRequest;
using BICgRPC::QueryBridgesResponse;
using BICgRPC::ConnectBridgeStatus;
using BICgRPC::DescribeBridgeRequest;
using BICgRPC::DescribeBridgeResponse;
using BICgRPC::ConnectBridgeRequest;
using BICgRPC::ConnectBridgeResponse;
using BICgRPC::bicSetStreamEnable;
using BICgRPC::DisconnectBridgeRequest;

class CImplantToGRPCListener : public IImplantListener
{
public:
    virtual ~CImplantToGRPCListener() {}

    virtual void onStimulationStateChanged(const bool isStimulating)
    {
        // TODO - anything? bug report? Should only trigger when there is a change right?
        std::cout << "\tDEBUG: Stimulation state changed: " << isStimulating << std::endl;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_isStimulating = isStimulating;
    }

    bool isStimulating()
    {
        // TODO - anything?
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isStimulating;
    }

    virtual void onMeasurementStateChanged(const bool isMeasuring)
    {
        // TODO - anything? bug report? Should only trigger when there is a change right?
        std::cout << "\tDEBUG: Measurement state changed: " << isMeasuring << std::endl;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_isMeasuring = isMeasuring;
    }

    bool isMeasuring()
    {
        // TODO - anything?
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isMeasuring;
    }

    virtual void onConnectionStateChanged(const connection_info_t& info)
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

    virtual void onConnectionStateChanged(const bool isConnected)
    {
        if (ConnectionWriter != NULL)
        {
            ConnectionUpdate connectionMessage;
            connectionMessage.set_connectiontype("Overall");
            connectionMessage.set_isconnected(isConnected);
            ConnectionWriter->Write(connectionMessage);
        }
    }

    virtual void onData(const std::vector<CSample>* samples)
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

    virtual void onImplantVoltageChanged(const double voltageMicroV)
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

    virtual void onPrimaryCoilCurrentChanged(const double currentMilliA)
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

    virtual void onImplantControlValueChanged(const double controlValue)
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

    virtual void onTemperatureChanged(const double temperature)
    {
        if (TemperatureWriter != NULL)
        {
            TemperatureUpdate tempMessage;
            tempMessage.set_temperature(temperature);
            tempMessage.set_units("celsius");
            TemperatureWriter->Write(tempMessage);
        }
    }

    virtual void onHumidityChanged(const double humidity)
    {
        if (HumidityWriter != NULL)
        {
            HumidityUpdate humidMessage;
            humidMessage.set_humidity(humidity);
            humidMessage.set_units("rh");
            HumidityWriter->Write(humidMessage);
        }
    }

    virtual void onError(const std::exception& err)
    {
        if (ErrorWriter != NULL)
        {
            ErrorUpdate errorMessage;
            errorMessage.set_message(err.what());
            ErrorWriter->Write(errorMessage);
        }
    }

    virtual void onDataProcessingTooSlow()
    {
        std::cout << "*** Warning: Data processing too slow" << std::endl;
        if (ErrorWriter != NULL)
        {
            ErrorUpdate errorMessage;
            errorMessage.set_message("Warning: Data processing too slow");
            ErrorWriter->Write(errorMessage);
        }
    }

    // Pointers for gRPC-managed streaming interfaces. Set by the BICDeviceServiceImpl class, null when not in use.
    grpc::ServerWriter<TemperatureUpdate>* TemperatureWriter = NULL;
    grpc::ServerWriter<HumidityUpdate>* HumidityWriter = NULL;
    grpc::ServerWriter<NeuralUpdate>* NeuralWriter = NULL;
    grpc::ServerWriter<ConnectionUpdate>* ConnectionWriter = NULL;
    grpc::ServerWriter<ErrorUpdate>* ErrorWriter = NULL;
    grpc::ServerWriter<PowerUpdate>* PowerWriter = NULL;
    uint32_t neuroDataBufferThreshold = 1;
    NeuralUpdate bufferedNeuroUpdate;
    
private:
    std::mutex m_mutex;
    bool m_isStimulating;
    bool m_isMeasuring;
    uint32_t lastNeuroCount = UINT32_MAX;
    
};

struct BICDeviceInfoStruct
{
    // Identifiers
    std::string deviceAddress;
    std::string bridgeId;
    std::string deviceId;

    // BIC Device-specific Objects
    std::unique_ptr <IImplant> theImplant;
    std::unique_ptr <CImplantInfo> theImplantInfo;                  // Cached Implant Info
    std::unique_ptr <CImplantToGRPCListener> listener;
    IStimulationCommand* theStimulationCommand;
    //std::mutex rpcDeviceLock;

    // Stream States
    bool isStreamingTemp = false;
    bool isStreamingHumid = false;
    bool isStreamingNeural = false;
    bool isStreamingConnection = false;
    bool isStreamingErrors = false;
    bool isStreamingPower = false;

    // Stream Mutexs
    std::mutex tempStreamLock;
    std::mutex humidStreamLock;
    std::mutex neuralStreamLock;
    std::mutex connectionStreamLock;
    std::mutex errorStreamLock;
    std::mutex powerStreamLock;

    // Stream notify variables
    std::condition_variable tempStreamNotify;
    std::condition_variable humidStreamNotify;
    std::condition_variable neuralStreamNotify;
    std::condition_variable connectionStreamNotify;
    std::condition_variable errorStreamNotify;
    std::condition_variable powerStreamNotify;
};

// Logic and data behind the server's behavior.
class BICDeviceServiceImpl final : public BICDeviceService::Service {
    // ************************* Cross-Function Service Variable Declarations *************************
    // BIC Initialization Objects - service wide
    IImplantFactory* theImplantFactory;
    std::vector<std::unique_ptr<BICDeviceInfoStruct>> theImplants;
    std::unordered_map<std::string, BICDeviceInfoStruct*> deviceDirectory;
    std::mutex rpcServiceLock;

    // ************************* Non-GRPC Helper Service Function Declarations *************************
    public:void passFactory(IImplantFactory* serverFactory)
    {
        theImplantFactory = serverFactory;
    }

    public:void controlDispose()   
    {   
        for (auto it=deviceDirectory.begin(); it!=deviceDirectory.end(); it++)
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
    Status ScanDevices(ServerContext* context, const ScanDevicesRequest* request, ScanDevicesReply* reply) override {

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
            // ### TODO Need to simplify ways to find known bridge/device combinations other than iterating through each time
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

                int numChannels = theImplantInfo->getChannelCount();
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
        return Status::OK;
    }

    Status ConnectDevice(ServerContext* context, const ConnectDeviceRequest* request, bicSuccessReply* reply) override {
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcServiceLock);
      
        // Split requested bridge/address strings from request
        int deviceIdIndex = request->deviceaddress().find("/device/");
        std::string deviceId = request->deviceaddress().substr(((int)deviceIdIndex) + 8);
        std::string bridgeId = request->deviceaddress().substr(13, ((int)deviceIdIndex) - 13);

        // Discover external units
        std::vector<CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();
        if (exInfos.empty())
        {
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "No Bridges");
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
                    return Status(grpc::StatusCode::NOT_FOUND, "Bridge/Device ID Mismatch");
                }

                break;
            }
        }
        
        // Check if the bridge wasn't found
        if (theImplantInfo == NULL)
        {
            return Status(grpc::StatusCode::NOT_FOUND, "No Bridge Found with Id");
        }
        else
        {
            // Found Device/Bridge Match - Proceed with System Initialization
            theImplants.push_back(std::unique_ptr<BICDeviceInfoStruct>(new BICDeviceInfoStruct()));
            std::pair<std::string, BICDeviceInfoStruct*> newKeyPair;
            newKeyPair.first = request->deviceaddress();
            newKeyPair.second = theImplants.back().get();
            newKeyPair.second->theImplant.reset(theImplantFactory->create(*exInfos.at(exInfoIndex), *theImplantInfo));
            newKeyPair.second->listener.reset(new CImplantToGRPCListener());
            newKeyPair.second->theImplant->registerListener(newKeyPair.second->listener.get());
            newKeyPair.second->theImplantInfo.reset(theImplantInfo);
            newKeyPair.second->deviceAddress = request->deviceaddress();
            newKeyPair.second->bridgeId = bridgeId;
            newKeyPair.second->deviceId = deviceId;

            deviceDirectory.insert(newKeyPair);

            // Everything went ok
            return Status::OK;
        }
    }

    Status bicDispose(ServerContext* context, const RequestDeviceAddress* request, bicSuccessReply* reply) override {
      
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcServiceLock);
        
        // Check if the requested device exists
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            // Not found!
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
            return Status::OK;
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
    Status bicGetImplantInfo(ServerContext* context, const bicGetImplantInfoRequest* request, bicGetImplantInfoReply* reply) override {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            // Not found!
            reply->set_success("error: not initialized");
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
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
                reply->set_success("error: exception (useful I know?)");
                return Status::OK;
            }
        }

        // Set the fields of the response with the Implant Information
        reply->set_firmwareversion(deviceDirectory[request->deviceaddress()]->theImplantInfo->getFirmwareVersion());
        reply->set_devicetype(deviceDirectory[request->deviceaddress()]->theImplantInfo->getDeviceType());
        reply->set_deviceid(deviceDirectory[request->deviceaddress()]->theImplantInfo->getDeviceId());
        reply->set_measurementchannelcount(deviceDirectory[request->deviceaddress()]->theImplantInfo->getMeasurementChannelCount());
        reply->set_stimulationchannelcount(deviceDirectory[request->deviceaddress()]->theImplantInfo->getStimulationChannelCount());
        reply->set_samplingrate(deviceDirectory[request->deviceaddress()]->theImplantInfo->getSamplingRate());

        int numChannels = deviceDirectory[request->deviceaddress()]->theImplantInfo->getChannelCount();
        reply->set_channelcount(numChannels);

        for (int i = 0; i < numChannels; i++)
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

    Status bicGetImpedance(ServerContext* context, const bicGetImpedanceRequest* request, bicGetImpedanceReply* reply) override {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            // Not found!
            reply->set_success("error: not initialized");
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        double impedanceValue;
        try
        {
            impedanceValue = deviceDirectory[request->deviceaddress()]->theImplant->getImpedance(request->channel());
        }
        catch (const std::exception&)
        {
            reply->set_success("error: exception (useful I know?)");
            return Status::OK;
        }

        // Respond to client
        reply->set_channelimpedance(impedanceValue);
        reply->set_units("ohms");
        reply->set_success("success");
        return Status::OK;
    }

    Status bicGetTemperature(ServerContext* context, const RequestDeviceAddress* request, bicGetTemperatureReply* reply) override {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            // Not found!
            reply->set_success("error: not initialized");
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        double temperatureValue;
        try
        {
            temperatureValue = deviceDirectory[request->deviceaddress()]->theImplant->getTemperature();
        }
        catch (const std::exception&)
        {
            reply->set_success("error: exception (useful I know?)");
            return Status::OK;
        }

        // Respond to client
        reply->set_temperature(temperatureValue);
        reply->set_units("celsius");
        reply->set_success("success");
        return Status::OK;
    }

    Status bicGetHumidity(ServerContext* context, const RequestDeviceAddress* request, bicGetHumidityReply* reply) override {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            // Not found!
            reply->set_success("error: not initialized");
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        double humidityValue;
        try
        {
            humidityValue = deviceDirectory[request->deviceaddress()]->theImplant->getHumidity();
        }
        catch (const std::exception&)
        {
            reply->set_success("error: exception (useful I know?)");
            return Status::OK;
        }

        // Respond to client
        reply->set_humidity(humidityValue);
        reply->set_units("celsius");
        reply->set_success("success");
        return Status::OK;
    }

    Status bicSetImplantPower(ServerContext* context, const bicSetImplantPowerRequest* request, bicSuccessReply* reply) override {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        try
        {
            deviceDirectory[request->deviceaddress()]->theImplant->setImplantPower(request->powerenabled());
        }
        catch (const std::exception&)
        {
            // TODO Exception Handling
            return Status::OK;
        }

        // Respond to client=
        return Status::OK;
    }

    // ************************* Streaming Control Function Declarations *************************
    Status bicTemperatureStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<TemperatureUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
         if (!deviceDirectory[request->deviceaddress()]->isStreamingTemp && request->enable())
        {
            // Not already streaming and requesting enable
            deviceDirectory[request->deviceaddress()]->isStreamingTemp = true;
            deviceDirectory[request->deviceaddress()]->listener->TemperatureWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->tempStreamLock);
            deviceDirectory[request->deviceaddress()]->tempStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->TemperatureWriter = NULL;
            deviceDirectory[request->deviceaddress()]->isStreamingTemp = false;
        }
        else if (deviceDirectory[request->deviceaddress()]->isStreamingTemp && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
             deviceDirectory[request->deviceaddress()]->tempStreamNotify.notify_one();
        }

        return Status::OK;
    }

    Status bicHumidityStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<HumidityUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->isStreamingHumid && request->enable())
        {
            // Not already streaming and requesting enable
            deviceDirectory[request->deviceaddress()]->isStreamingHumid = true;
            deviceDirectory[request->deviceaddress()]->listener->HumidityWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->humidStreamLock);
            deviceDirectory[request->deviceaddress()]->humidStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->HumidityWriter = NULL;
            deviceDirectory[request->deviceaddress()]->isStreamingHumid = false;
        }
        else if (deviceDirectory[request->deviceaddress()]->isStreamingHumid && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->humidStreamNotify.notify_one();
        }

        return Status::OK;
    }

    Status bicConnectionStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<ConnectionUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->isStreamingConnection && request->enable())
        {
            // Not already streaming and requesting enable
            deviceDirectory[request->deviceaddress()]->isStreamingConnection = true;
            deviceDirectory[request->deviceaddress()]->listener->ConnectionWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->connectionStreamLock);
            deviceDirectory[request->deviceaddress()]->connectionStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->ConnectionWriter = NULL;
            deviceDirectory[request->deviceaddress()]->isStreamingConnection = false;
        }
        else if (deviceDirectory[request->deviceaddress()]->isStreamingConnection && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->connectionStreamNotify.notify_one();
        }

        return Status::OK;
    }

    Status bicErrorStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<ErrorUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->isStreamingErrors && request->enable())
        {
            // Not already streaming and requesting enable
            deviceDirectory[request->deviceaddress()]->isStreamingErrors = true;
            deviceDirectory[request->deviceaddress()]->listener->ErrorWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->errorStreamLock);
            deviceDirectory[request->deviceaddress()]->errorStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->ErrorWriter = NULL;
            deviceDirectory[request->deviceaddress()]->isStreamingErrors = false;
        }
        else if (deviceDirectory[request->deviceaddress()]->isStreamingErrors && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->errorStreamNotify.notify_one();
        }

        return Status::OK;
    }

    Status bicPowerStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<PowerUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->isStreamingPower && request->enable())
        {
            // Not already streaming and requesting enable
            deviceDirectory[request->deviceaddress()]->isStreamingPower = true;
            deviceDirectory[request->deviceaddress()]->listener->PowerWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->powerStreamLock);
            deviceDirectory[request->deviceaddress()]->powerStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            deviceDirectory[request->deviceaddress()]->listener->PowerWriter = NULL;
            deviceDirectory[request->deviceaddress()]->isStreamingPower = false;
        }
        else if (deviceDirectory[request->deviceaddress()]->isStreamingPower && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->powerStreamNotify.notify_one();
        }

        return Status::OK;
    }

    Status bicNeuralStream(ServerContext* context, const bicNeuralSetStreamingEnable* request, ServerWriter<NeuralUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!deviceDirectory[request->deviceaddress()]->isStreamingNeural && request->enable())
        {
            // Not already streaming and requesting enable
            // Configure reference electrodes
            std::set<uint32_t> referenceElectrodes;
            for (int i = 0; i < request->refchannels_size(); i++)
            {
                referenceElectrodes.insert(referenceElectrodes.begin(), request->refchannels()[i]);
            }
            deviceDirectory[request->deviceaddress()]->theImplant->startMeasurement(referenceElectrodes);

            // Configure buffers
            deviceDirectory[request->deviceaddress()]->listener->neuroDataBufferThreshold = request->buffersize();
            deviceDirectory[request->deviceaddress()]->listener->bufferedNeuroUpdate.clear_samples();
            deviceDirectory[request->deviceaddress()]->isStreamingNeural = true;
            deviceDirectory[request->deviceaddress()]->listener->NeuralWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(deviceDirectory[request->deviceaddress()]->neuralStreamLock);
            deviceDirectory[request->deviceaddress()]->neuralStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags, and stop LFP measurement
            deviceDirectory[request->deviceaddress()]->listener->NeuralWriter = NULL;
            deviceDirectory[request->deviceaddress()]->listener->bufferedNeuroUpdate.clear_samples();
            deviceDirectory[request->deviceaddress()]->isStreamingNeural = false;
            deviceDirectory[request->deviceaddress()]->theImplant->stopMeasurement();
        }
        else if (deviceDirectory[request->deviceaddress()]->isStreamingNeural && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            deviceDirectory[request->deviceaddress()]->neuralStreamNotify.notify_one();
        }

        return Status::OK;
    }

    // ************************* Stimulation Control Function Declarations *************************
    Status bicStartStimulation(ServerContext* context, const bicStartStimulationRequest* request, bicSuccessReply* reply) override {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        try
        {
            deviceDirectory[request->deviceaddress()]->theImplant->startStimulation(deviceDirectory[request->deviceaddress()]->theStimulationCommand);
        }
        catch (const std::exception theExeption)
        {
            // TODO Exception Handling
            std::cout << "Start Stimulation Exception: " << theExeption.what() << std::endl;
            return Status::OK;
        }

        // Respond to client
        return Status::OK;
    }

    Status bicStopStimulation(ServerContext* context, const RequestDeviceAddress* request, bicSuccessReply* reply) override {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        try
        {
            deviceDirectory[request->deviceaddress()]->theImplant->stopStimulation();
        }
        catch (const std::exception&)
        {
            // TODO Exception Handling
            return Status::OK;
        }

        // Respond to client
        return Status::OK;
    }

    Status bicDefineStimulationWaveform(ServerContext* context, const bicStimulationFunctionDefinitionRequest* request, bicSuccessReply* reply) override
    {
        // Check if already initialized
        if (deviceDirectory.find(request->deviceaddress()) == deviceDirectory.end())
        {
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Create objects required for 
        std::unique_ptr<IStimulationCommandFactory> theFactory(createStimulationCommandFactory());
        deviceDirectory[request->deviceaddress()]->theStimulationCommand = theFactory->createStimulationCommand();

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
                    theFunction->setRepetitions(request->functions().at(i).stimpulse().repetitions());

                    // Pull out the electrodes and set the electrode fields
                    std::set<uint32_t> sources;
                    std::set<uint32_t> sinks;
                    for (int j = 0; j < request->functions().at(i).stimpulse().sourceelectrodes().size(); j++)
                    {
                        sources.insert(request->functions().at(i).stimpulse().sourceelectrodes()[j]);
                    }
                    for (int j = 0; j < request->functions().at(i).stimpulse().sinkelectrodes().size(); j++)
                    {
                        if (request->functions().at(i).stimpulse().sinkelectrodes()[j] >= 32)
                        {
                            sinks.insert(0x0002FFFF);
                        }
                        else
                        {
                            sinks.insert(request->functions().at(i).stimpulse().sinkelectrodes()[j]);
                        }

                    }
                    theFunction->setVirtualStimulationElectrodes(sources, sinks);

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

                    // Genmerate atoms -- charge balance ( 1/4 amplitude, 4 x pulsewidth of main pulse)
                    theFunction->append(theFactory->createRect4AmplitudeStimulationAtom(
                        request->functions().at(i).stimpulse().amplitude()[0] / -4,
                        request->functions().at(i).stimpulse().amplitude()[1] / -4,
                        request->functions().at(i).stimpulse().amplitude()[2] / -4,
                        request->functions().at(i).stimpulse().amplitude()[3] / -4,
                        request->functions().at(i).stimpulse().pulsewidth() * 4));

                    // Generate atoms -- DZ0
                    theFunction->append(theFactory->createRect4AmplitudeStimulationAtom(0, 0, 0, 0, request->functions().at(i).stimpulse().dz0duration()));

                    // Generate atoms -- DZ1
                    theFunction->append(theFactory->createRect4AmplitudeStimulationAtom(0, 0, 0, 0, request->functions().at(i).stimpulse().dz1duration()));

                    // Add the function to the command
                    deviceDirectory[request->deviceaddress()]->theStimulationCommand->append(theFunction);
                }
                else if (request->functions().at(i).has_pause())
                {
                    // Create the pulse function
                    theFunction->append(theFactory->createStimulationPauseAtom(request->functions().at(i).pause().duration()));

                    // Add the function to the command
                    deviceDirectory[request->deviceaddress()]->theStimulationCommand->append(theFunction);
                }
                else
                {
                    // No atoms?
                }
            }
        }
        catch (const std::exception theExeption)
        {
            // TODO Exception Handling
            std::cout << "Define Stimulation Waveform Exception: " << theExeption.what() << std::endl;
            return Status::OK;
        }
        
        return Status::OK;
    }
};

class BICBridgeServiceImpl final : public BICBridgeService::Service {
    // ************************* Cross-Function Service Variable Declarations *************************
    IImplantFactory* theImplantFactory;
    std::mutex rpcLock;

    // Initialize Service with a Factory
    public:void passFactory(IImplantFactory* serverFactory)
    {
        theImplantFactory = serverFactory;
    }

    // ************************* General Service Function Declarations *************************
    Status ListBridges(ServerContext* context, const QueryBridgesRequest* request, QueryBridgesResponse* reply) override {
        // Finds all bridges connected to the PC
        std::vector<CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();
        
        for (int i = 0; i < exInfos.size(); i++)
        {
            //construct URIs
            Bridge* aBridge = reply->add_bridges();
            aBridge->set_name("//bic/bridge/" + exInfos[i]->getDeviceId());
        }
        
        return Status::OK;
    }

    Status ScanBridges(ServerContext* context, const QueryBridgesRequest* request, QueryBridgesResponse* reply) override {
        // Finds all bridges connected to the PC
        std::vector<CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();

        for (int i = 0; i < exInfos.size(); i++)
        {
            //construct URIs
            Bridge* aBridge = reply->add_bridges();
            aBridge->set_name("//bic/bridge/" + exInfos[i]->getDeviceId());
        }

        return Status::OK;
    }

    Status ConnectedBridges(ServerContext* context, const QueryBridgesRequest* request, QueryBridgesResponse* reply) override {
        // All bridges found by this function are connected to the PC
        std::vector<CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();

        for (int i = 0; i < exInfos.size(); i++)
        {
            //construct URIs
            Bridge* aBridge = reply->add_bridges();
            aBridge->set_name("//bic/bridge/" + exInfos[i]->getDeviceId());
            aBridge->set_firmwareversion(exInfos[i]->getFirmwareVersion());
            aBridge->set_implanttype(exInfos[i]->getImplantType());
            aBridge->set_deviceid(exInfos[i]->getDeviceId());
        }
        exInfos.~vector();
        return Status::OK;
    }

    Status ConnectBridge(ServerContext* context, const ConnectBridgeRequest* request, ConnectBridgeResponse* reply) override {
        // All bridges are by definition 'connected' via USB, so if it exists in the list it's connected
        reply->set_name(request->name());
        std::vector<CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();

        for (int i = 0; i < exInfos.size(); i++)
        {
            if ("//bic/bridge/" + exInfos[i]->getDeviceId() == request->name())
            {
                reply->set_connection_status((ConnectBridgeStatus)1);
                return Status::OK;
            }
        }

        reply->set_connection_status((ConnectBridgeStatus)2);
        return Status::OK;
    }

    Status DescribeBridge(ServerContext* context, const DescribeBridgeRequest* request, DescribeBridgeResponse* reply) override {
        std::vector<CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();

        for (int i = 0; i < exInfos.size(); i++)
        {
            if ("//bic/bridge/" + exInfos[i]->getDeviceId() == request->name())
            {
                reply->set_name("//bic/bridge/" + exInfos[i]->getDeviceId());
                Bridge* aBridge = reply->mutable_details();
                aBridge->set_name("//bic/bridge/" + exInfos[i]->getDeviceId());
                aBridge->set_firmwareversion(exInfos[i]->getFirmwareVersion());
                aBridge->set_implanttype(exInfos[i]->getImplantType());
                aBridge->set_deviceid(exInfos[i]->getDeviceId());
                return Status::OK;
            }
        }

        return Status::OK;
    }

    Status DisconnectBridge(ServerContext* context, const DisconnectBridgeRequest* request, google::protobuf::Empty* reply) override {
        // All bridges are by definition 'connected' via USB, so they can't be disconnected
        return Status::OK;
    }
};

// Server Objects
std::unique_ptr<Server> gRPCServer;
BICDeviceServiceImpl deviceService;
BICBridgeServiceImpl bridgeService;

// Server Control Handler
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
        // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
        printf("Ctrl-C event\n\n");
        deviceService.controlDispose();
        gRPCServer->Shutdown();
        return TRUE;

        // CTRL-CLOSE: confirm that the user wants to exit.
    case CTRL_CLOSE_EVENT:
        printf("Ctrl-Close event\n\n");
        deviceService.controlDispose();
        gRPCServer->Shutdown();
        return TRUE;

        // Pass other signals to the next handler.
    case CTRL_BREAK_EVENT:
        printf("Ctrl-Break event\n\n");
        deviceService.controlDispose();
        gRPCServer->Shutdown();
        return TRUE;

    case CTRL_LOGOFF_EVENT:
        printf("Ctrl-Logoff event\n\n");
        deviceService.controlDispose();
        gRPCServer->Shutdown();
        return TRUE;

    case CTRL_SHUTDOWN_EVENT:
        printf("Ctrl-Shutdown event\n\n");
        deviceService.controlDispose();
        gRPCServer->Shutdown();
        return TRUE;

    default:
        return TRUE;
    }
}

// Server Instance
void RunServer() {
    // ******************* Build up GRPC Interface *******************
    // Define the server address
    std::string server_address("0.0.0.0:50051");
    // enable gRPC functionality
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    // Create implant factory for cross-service usage
    std::unique_ptr <IImplantFactory> theImplantFactory;
    theImplantFactory.reset(createImplantFactory(false, ""));
    bridgeService.passFactory(theImplantFactory.get());
    deviceService.passFactory(theImplantFactory.get());
    
    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;

    // ******************* Subscribe to Windows Control Event Handler *******************
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    // ******************* Wait for Shutdown *******************
    gRPCServer->Wait();

    // ******************* Cleanup *******************
    theImplantFactory->~IImplantFactory();
}

// Main Stub, just runs the server
int main(int argc, char** argv) {
  RunServer();

  return 0;
}
