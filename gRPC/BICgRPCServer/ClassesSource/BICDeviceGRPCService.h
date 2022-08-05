#pragma once
#include <cppapi/bicapi.h>

#include <grpcpp/grpcpp.h>

#include <mutex>
#include <string>
#include <unordered_map>

#include "BICgRPC.grpc.pb.h"
#include "BICDeviceInfoStruct.h"

namespace BICGRPCHelperNamespace
{
    class BICDeviceGRPCService final : public BICgRPC::BICDeviceService::Service {
    public:
        // ************************* Cross-Function Service Variable Declarations *************************
        // BIC Initialization Objects - service wide
        cortec::implantapi::IImplantFactory* theImplantFactory;
        std::vector<std::unique_ptr<BICDeviceInfoStruct>> theImplants;
        std::unordered_map<std::string, BICDeviceInfoStruct*> deviceDirectory;
        std::mutex rpcServiceLock;

        // ************************* Non-GRPC Helper Service Function Declarations *************************
        void passFactory(cortec::implantapi::IImplantFactory* serverFactory);

        void controlDispose();

        // ************************* Construction, Initialization, and Destruction Function Declarations *************************
        grpc::Status ScanDevices(grpc::ServerContext* context, const BICgRPC::ScanDevicesRequest* request, BICgRPC::ScanDevicesReply* reply) override;

        grpc::Status ConnectDevice(grpc::ServerContext* context, const BICgRPC::ConnectDeviceRequest* request, BICgRPC::bicSuccessReply* reply) override;

        grpc::Status bicDispose(grpc::ServerContext* context, const BICgRPC::RequestDeviceAddress* request, BICgRPC::bicSuccessReply* reply) override;

        // ************************* Implant State Get and Set Function Declarations *************************
        grpc::Status bicGetImplantInfo(grpc::ServerContext* context, const BICgRPC::bicGetImplantInfoRequest* request, BICgRPC::bicGetImplantInfoReply* reply) override;

        grpc::Status bicGetImpedance(grpc::ServerContext* context, const BICgRPC::bicGetImpedanceRequest* request, BICgRPC::bicGetImpedanceReply* reply) override;

        grpc::Status bicGetTemperature(grpc::ServerContext* context, const BICgRPC::RequestDeviceAddress* request, BICgRPC::bicGetTemperatureReply* reply) override;

        grpc::Status bicGetHumidity(grpc::ServerContext* context, const BICgRPC::RequestDeviceAddress* request, BICgRPC::bicGetHumidityReply* reply) override;

        grpc::Status bicSetImplantPower(grpc::ServerContext* context, const BICgRPC::bicSetImplantPowerRequest* request, BICgRPC::bicSuccessReply* reply) override;

        // ************************* Streaming Control Function Declarations *************************
        grpc::Status bicTemperatureStream(grpc::ServerContext* context, const BICgRPC::bicSetStreamEnable* request, grpc::ServerWriter<BICgRPC::TemperatureUpdate>* writer) override;

        grpc::Status bicHumidityStream(grpc::ServerContext* context, const BICgRPC::bicSetStreamEnable* request, grpc::ServerWriter<BICgRPC::HumidityUpdate>* writer) override;

        grpc::Status bicConnectionStream(grpc::ServerContext* context, const BICgRPC::bicSetStreamEnable* request, grpc::ServerWriter<BICgRPC::ConnectionUpdate>* writer) override;

        grpc::Status bicErrorStream(grpc::ServerContext* context, const BICgRPC::bicSetStreamEnable* request, grpc::ServerWriter<BICgRPC::ErrorUpdate>* writer) override;

        grpc::Status bicPowerStream(grpc::ServerContext* context, const BICgRPC::bicSetStreamEnable* request, grpc::ServerWriter<BICgRPC::PowerUpdate>* writer) override;

        grpc::Status bicNeuralStream(grpc::ServerContext* context, const BICgRPC::bicNeuralSetStreamingEnable* request, grpc::ServerWriter<BICgRPC::NeuralUpdate>* writer) override;

          // ************************* Stimulation Control Function Declarations *************************
        grpc::Status bicStartStimulation(grpc::ServerContext* context, const BICgRPC::bicStartStimulationRequest* request, BICgRPC::bicSuccessReply* reply) override;

        grpc::Status bicStopStimulation(grpc::ServerContext* context, const BICgRPC::RequestDeviceAddress* request, BICgRPC::bicSuccessReply* reply) override;

        grpc::Status bicEnqueueStimulation(grpc::ServerContext* context, const BICgRPC::bicEnqueueStimulationRequest* request, BICgRPC::bicSuccessReply* reply) override;

        grpc::Status enableDistributedStimulation(grpc::ServerContext* context, const BICgRPC::distributedStimEnableRequest* request, BICgRPC::bicSuccessReply* reply) override;

        grpc::Status enableOpenLoopStimulation(grpc::ServerContext* context, const BICgRPC::openLoopStimEnableRequest* request, BICgRPC::bicSuccessReply* reply) override;
    };
}
