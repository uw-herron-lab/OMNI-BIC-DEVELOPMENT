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
#include <mutex>
#include <condition_variable>

#include <iostream>
#include <memory>
#include <string>
#include <cppapi/bic3232constants.h>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <google/protobuf/any.pb.h>
#include <google/protobuf/empty.pb.h>

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
using BICgRPC::bicSetSensingEnableRequest;
using BICgRPC::bicSetImplantPowerRequest;
using BICgRPC::bicStartStimulationRequest;
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

class CImplantToStdOutListener : public IImplantListener
{
public:
    virtual ~CImplantToStdOutListener() {}

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
                            diff = (double)samples->at(i).getMeasurementCounter() + (UINT32_MAX - lastNeuroCount + 1);
                        }
                        // Update counter
                        lastNeuroCount = samples->at(i).getMeasurementCounter();

                        // Write error message to server console
                        std::cout << "** Missed Neural Datapoints: " << diff << "! **" << std::endl;

                        // If neural data buffer is not empty, flush it so we can keep buffers continuous
                        if (NeuralWriter != NULL)
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

/**
* @return example stimulation command, which must either be passed to startStimulation() or be deleted in order to
*         prevent memory leaks
*/
IStimulationCommand* createStimulationCommand()
{
    std::unique_ptr<IStimulationCommandFactory> factory(createStimulationCommandFactory());

    IStimulationCommand* cmd = factory->createStimulationCommand();
    IStimulationFunction* function = factory->createStimulationFunction();

    // apply signal to stimulation channel 
    std::set<uint32_t> sourceChannels;
    sourceChannels.insert(16u + 7u); // 16 measurement channel, stimulation channels in [0..7] 
    std::set<uint32_t> destinationChannels;
    destinationChannels.insert(BIC3232Constants::c_groundElectrode);

    function->setRepetitions(10);
    function->append(factory->createRectStimulationAtom(5.0, 20000));
    function->append(factory->createRectStimulationAtom(0, 30000));
    function->append(factory->createRectStimulationAtom(10.0, 20000));
    function->append(factory->createRectStimulationAtom(0, 30000));

    function->setVirtualStimulationElectrodes(sourceChannels, destinationChannels);
    cmd->append(function);

    function = factory->createStimulationFunction();
    function->setRepetitions(5);
    function->append(factory->createRectStimulationAtom(7.5, 10000));
    function->append(factory->createRectStimulationAtom(10.0, 10000));
    function->append(factory->createRectStimulationAtom(5.0, 10000));
    function->append(factory->createRectStimulationAtom(2.5, 10000));

    function->setVirtualStimulationElectrodes(sourceChannels, destinationChannels);
    cmd->append(function);

    function = factory->createStimulationFunction();
    function->setRepetitions(3);
    function->append(factory->createRectStimulationAtom(2.5, 1000));
    function->append(factory->createRectStimulationAtom(1.0, 5000));
    function->append(factory->createRectStimulationAtom(9.0, 2000));
    function->append(factory->createRectStimulationAtom(0, 17000));

    function->setVirtualStimulationElectrodes(sourceChannels, destinationChannels);
    cmd->append(function);

    return cmd;
}

// Logic and data behind the server's behavior.
class BICDeviceServiceImpl final : public BICDeviceService::Service {
    // ************************* Cross-Function Service Variable Declarations *************************
    // Currently only supports one BIC at a time, need to turn these into lists of implants and cached implant info
    std::unique_ptr <IImplant> theImplant;
    std::unique_ptr <IImplantFactory> theImplantFactory;
    std::unique_ptr <CImplantInfo> theImplantInfo;          // Cached Implant Info
    CImplantToStdOutListener listener;
    bool factoryInitialized = false;
    bool implantInitialized = false;
    std::mutex rpcLock;
    
    // Stream States, Mutexs, and Notifiers
    bool isStreamingTemp = false;
    bool isStreamingHumid = false;
    bool isStreamingNeural = false;
    bool isStreamingConnection = false;
    bool isStreamingErrors = false;
    bool isStreamingPower = false;

    std::mutex tempStreamLock;
    std::mutex humidStreamLock;
    std::mutex neuralStreamLock;
    std::mutex connectionStreamLock;
    std::mutex errorStreamLock;
    std::mutex powerStreamLock;

    std::condition_variable tempStreamNotify;
    std::condition_variable humidStreamNotify;
    std::condition_variable neuralStreamNotify;
    std::condition_variable connectionStreamNotify;
    std::condition_variable errorStreamNotify;
    std::condition_variable powerStreamNotify;

    // ************************* Helper Service Function Declarations *************************
    void factoryInitializeCheck()
    {
        // Check if already initialized
        if (!factoryInitialized)
        {
            // Get implant factory
            theImplantFactory.reset(createImplantFactory(false, "./bridgeService.log"));
            factoryInitialized = true;
        }
    }

    // ************************* General Service Function Declarations *************************
    Status ScanDevices(ServerContext* context, const ScanDevicesRequest* request, ScanDevicesReply* reply) override {

        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcLock);
        factoryInitializeCheck();

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
                theImplantInfo.reset(theImplantFactory->getImplantInfo(*exInfos.at(i)));

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
        const std::lock_guard<std::mutex> lock(rpcLock);
      
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
        for (exInfoIndex; exInfoIndex < exInfos.size(); exInfoIndex++)
        {
            if (exInfos[exInfoIndex]->getDeviceId() == bridgeId)
            {
                // pull the info from the device
                theImplantInfo.reset(theImplantFactory->getImplantInfo(*exInfos.at(exInfoIndex)));

                // Check that deviceId matches
                if (theImplantInfo->getDeviceId() != deviceId)
                {
                    return Status(grpc::StatusCode::NOT_FOUND, "Bridge/Device ID Mismatch");
                }

                break;
            }
        }

        // Found Device/Bridge Match - Proceed with System Initialization
        theImplantFactory.reset(createImplantFactory(true, request->logfilename()));
        theImplant.reset(theImplantFactory->create(*exInfos.at(exInfoIndex), *theImplantInfo));
        theImplant->registerListener(&listener);
        implantInitialized = true;
        return Status::OK;
    }

    Status bicDispose(ServerContext* context, const google::protobuf::Empty* request, bicSuccessReply* reply) override {
      
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcLock);

        // Check if 
        if (!implantInitialized)
        {
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
            return Status::OK;
        }
        
        // Stop all streaming!
        tempStreamNotify.notify_all();
        humidStreamNotify.notify_all();
        neuralStreamNotify.notify_all();
        connectionStreamNotify.notify_all();
        errorStreamNotify.notify_all();
        powerStreamNotify.notify_all();

        // Dispose the things!
        theImplant->setImplantPower(false);
        theImplant->~IImplant();
        theImplantFactory->~IImplantFactory();

        // Reset initialized flag
        implantInitialized = false;

        // Respond to client
        return Status::OK;
    }

    // ************************* Implant Function Declarations *************************
    Status bicGetImplantInfo(ServerContext* context, const bicGetImplantInfoRequest* request, bicGetImplantInfoReply* reply) override {

        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcLock);

        // Check if already initialized
        if (!implantInitialized)
        {
            reply->set_success("error: not initialized");
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Check if we need to update cache
        if (request->updatecachedinfo())
        {
            try
            {
                theImplantInfo.reset(theImplant->getImplantInfo());
            }
            catch (const std::exception& theError)
            {
                reply->set_success("error: exception (useful I know?)");
                return Status::OK;
            }
        }

        // Set the fields of the response with the Implant Information
        reply->set_firmwareversion(theImplantInfo->getFirmwareVersion());
        reply->set_devicetype(theImplantInfo->getDeviceType());
        reply->set_deviceid(theImplantInfo->getDeviceId());
        reply->set_measurementchannelcount(theImplantInfo->getMeasurementChannelCount());
        reply->set_stimulationchannelcount(theImplantInfo->getStimulationChannelCount());
        reply->set_samplingrate(theImplantInfo->getSamplingRate());

        int numChannels = theImplantInfo->getChannelCount();
        reply->set_channelcount(numChannels);

        for (int i = 0; i < numChannels; i++)
        {
            CChannelInfo* sourceChannel = theImplantInfo->getChannelInfo()[i];
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
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcLock);

        // Check if already initialized
        if (!implantInitialized)
        {
            reply->set_success("error: not initialized");
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        double impedanceValue;
        try
        {
            impedanceValue = theImplant->getImpedance(request->channel());
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

    Status bicGetTemperature(ServerContext* context, const google::protobuf::Empty* request, bicGetTemperatureReply* reply) override {
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcLock);

        // Check if already initialized
        if (!implantInitialized)
        {
            reply->set_success("error: not initialized");
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        double temperatureValue;
        try
        {
            temperatureValue = theImplant->getTemperature();
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

    Status bicGetHumidity(ServerContext* context, const google::protobuf::Empty* request, bicGetHumidityReply* reply) override {
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcLock);

        // Check if already initialized
        if (!implantInitialized)
        {
            reply->set_success("error: not initialized");
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        double humidityValue;
        try
        {
            humidityValue = theImplant->getHumidity();
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

    Status bicSetSensingEnable(ServerContext* context, const bicSetSensingEnableRequest* request, bicSuccessReply* reply) override {
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcLock);

        // Check if already initialized
        if (!implantInitialized)
        {
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        try
        {
            if (request->enablesensing())
            {
                std::set<uint32_t> referenceElectrodes;
                for (int i = 0; i < request->refchannels_size(); i++)
                {
                    referenceElectrodes.insert(referenceElectrodes.begin(), request->refchannels()[i]);
                }
                listener.neuroDataBufferThreshold = request->buffersize();
                theImplant->startMeasurement(referenceElectrodes);
            }
            else
            {
                theImplant->stopMeasurement();
            }
        }
        catch (const std::exception&)
        {
            // TODO add exception handling
            return Status::OK;
        }

        // Respond to client
        return Status::OK;
    }

    Status bicSetImplantPower(ServerContext* context, const bicSetImplantPowerRequest* request, bicSuccessReply* reply) override {
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcLock);

        // Check if already initialized
        if (!implantInitialized)
        {
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
            return Status::OK;
        }

        // Perform the operation
        try
        {
            theImplant->setImplantPower(request->powerenabled());
        }
        catch (const std::exception&)
        {
            // TODO Exception Handling
            return Status::OK;
        }

        // Respond to client=
        return Status::OK;
    }

    Status bicStartStimulation(ServerContext* context, const bicStartStimulationRequest* request, bicSuccessReply* reply) override {
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcLock);

        // Check if already initialized
        if (!implantInitialized)
        {
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        try
        {

        }
        catch (const std::exception&)
        {
            // TODO Exception Handling
            return Status::OK;
        }

        // Respond to client
        return Status::OK;
    }

    Status bicStopStimulation(ServerContext* context, const google::protobuf::Empty* request, bicSuccessReply* reply) override {
        // Pull the mutex for guarding against inappropriate multi-threaded access
        const std::lock_guard<std::mutex> lock(rpcLock);

        // Check if already initialized
        if (!implantInitialized)
        {
            return Status(grpc::StatusCode::FAILED_PRECONDITION, "Not Initialized");
        }

        // Perform the operation
        try
        {
            theImplant->stopStimulation();
        }
        catch (const std::exception&)
        {
            // TODO Exception Handling
            return Status::OK;
        }

        // Respond to client
        return Status::OK;
    }

    // ************************* Streaming Declarations *************************
    Status bicTemperatureStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<TemperatureUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
         if (!isStreamingTemp && request->enable())
        {
            // Not already streaming and requesting enable
            isStreamingTemp = true;
            listener.TemperatureWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(tempStreamLock);
            tempStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            listener.TemperatureWriter = NULL;
            isStreamingTemp = false;
        }
        else if (isStreamingTemp && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            tempStreamNotify.notify_one();
        }

        return Status::OK;
    }

    Status bicHumidityStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<HumidityUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!isStreamingHumid && request->enable())
        {
            // Not already streaming and requesting enable
            isStreamingHumid = true;
            listener.HumidityWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(humidStreamLock);
            humidStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            listener.HumidityWriter = NULL;
            isStreamingHumid = false;
        }
        else if (isStreamingHumid && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            humidStreamNotify.notify_one();
        }

        return Status::OK;
    }

    Status bicConnectionStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<ConnectionUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!isStreamingConnection && request->enable())
        {
            // Not already streaming and requesting enable
            isStreamingConnection = true;
            listener.ConnectionWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(connectionStreamLock);
            connectionStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            listener.ConnectionWriter = NULL;
            isStreamingConnection = false;
        }
        else if (isStreamingConnection && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            connectionStreamNotify.notify_one();
        }

        return Status::OK;
    }

    Status bicErrorStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<ErrorUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!isStreamingErrors && request->enable())
        {
            // Not already streaming and requesting enable
            isStreamingErrors = true;
            listener.ErrorWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(errorStreamLock);
            errorStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            listener.ErrorWriter = NULL;
            isStreamingErrors = false;
        }
        else if (isStreamingErrors && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            errorStreamNotify.notify_one();
        }

        return Status::OK;
    }

    Status bicPowerStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<PowerUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!isStreamingPower && request->enable())
        {
            // Not already streaming and requesting enable
            isStreamingPower = true;
            listener.PowerWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(powerStreamLock);
            powerStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            listener.PowerWriter = NULL;
            isStreamingPower = false;
        }
        else if (isStreamingPower && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            powerStreamNotify.notify_one();
        }

        return Status::OK;
    }

    Status bicNeuralStream(ServerContext* context, const bicSetStreamEnable* request, ServerWriter<NeuralUpdate>* writer) override {
        // Check requested stream state and current streaming state (don't want to destroy a previously requested stream without it being stopped first)
        if (!isStreamingNeural && request->enable())
        {
            // Not already streaming and requesting enable
            isStreamingNeural = true;
            listener.bufferedNeuroUpdate.clear_samples();
            listener.NeuralWriter = writer;

            // Create the waiting objects for notification for end of stream
            std::unique_lock<std::mutex> StreamLockInst(neuralStreamLock);
            neuralStreamNotify.wait(StreamLockInst);

            // Clean up the writers and busy flags
            listener.NeuralWriter = NULL;
            listener.bufferedNeuroUpdate.clear_samples();
            isStreamingNeural = false;
        }
        else if (isStreamingNeural && request->enable())
        {
            // Error State, already streaming, do nothing
                // Would love to send an error back, but if we don't send Status::OK then the "await ResponseStream.MoveNext()" doesn't work right and gracefully exit :/
            return Status::OK;
        }
        else
        {
            // disable streaming
            neuralStreamNotify.notify_one();
        }

        return Status::OK;
    }
};

class BICBridgeServiceImpl final : public BICBridgeService::Service {
    // ************************* Cross-Function Service Variable Declarations *************************
    std::unique_ptr <IImplantFactory> theImplantFactory;
    bool initialized = false;
    std::mutex rpcLock;

    // Initialize
    void initializeCheck()
    {
        // Check if already initialized
        if (!initialized)
        {
            // Get implant factory
            theImplantFactory.reset(createImplantFactory(false, "./bridgeService.log"));
            initialized = true;
        }
    }

    // ************************* General Service Function Declarations *************************
    Status ListBridges(ServerContext* context, const QueryBridgesRequest* request, QueryBridgesResponse* reply) override {
        initializeCheck();

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
        initializeCheck();

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
        initializeCheck();

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
        initializeCheck();

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
        initializeCheck();

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
        initializeCheck();

        // All bridges are by definition 'connected' via USB, so they can't be disconnected
        return Status::OK;
    }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  BICDeviceServiceImpl deviceService;
  BICBridgeServiceImpl bridgeService;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&deviceService);
  builder.RegisterService(&bridgeService);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
  
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}
