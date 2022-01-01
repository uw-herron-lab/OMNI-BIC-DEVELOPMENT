#pragma once
#include <cppapi/bicapi.h>
#include <cppapi/bic3232constants.h>

#include <grpcpp/grpcpp.h>

#include <mutex>
#include <string>
#include <unordered_map>

#include "BICgRPC.grpc.pb.h"
#include "BICDeviceInfoStruct.h"

namespace BICGRPCHelperNamespace
{
    class BICInfoGRPCService final : public BICgRPC::BICInfoService::Service {
    public:
        // ************************* Cross-Function Service Variable Declarations *************************
        // BIC Initialization Objects - service wide
        cortec::implantapi::IImplantFactory* theImplantFactory;
        std::vector<std::unique_ptr<BICDeviceInfoStruct>> theImplants;
        std::unordered_map<std::string, BICDeviceInfoStruct*> deviceDirectory;
        std::mutex rpcServiceLock;

        // ************************* Non-GRPC Helper Service Function Declarations *************************
        void passFactory(cortec::implantapi::IImplantFactory* serverFactory);

        // ************************* Construction, Initialization, and Destruction Function Declarations *************************
        grpc::Status VersionNumber(grpc::ServerContext* context, const BICgRPC::VersionNumberRequest* request, BICgRPC::VersionNumberResponse* reply) override;

        grpc::Status SupportedDevices(grpc::ServerContext* context, const BICgRPC::SupportedDevicesRequest* request, BICgRPC::SupportedDevicesResponse* reply) override;

        grpc::Status InspectRepository(grpc::ServerContext* context, const BICgRPC::InspectRepositoryRequest* request, BICgRPC::InspectRepositoryResponse* reply) override;

    };
}