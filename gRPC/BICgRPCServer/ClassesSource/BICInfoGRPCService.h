#pragma once
#include <cppapi/bicapi.h>
#include <cppapi/bic3232constants.h>

#include <grpcpp/grpcpp.h>

#include <string>

#include "BICgRPC.grpc.pb.h"
#include "BICDeviceInfoStruct.h"

namespace BICGRPCHelperNamespace
{
    class BICInfoGRPCService final : public BICgRPC::BICInfoService::Service {
    public:
        // ************************* Cross-Function Service Variable Declarations *************************
        // BIC Initialization Objects - service wide

        std::vector<std::unique_ptr<BICDeviceInfoStruct>>* pointerVar;

        // ************************* Non-GRPC Helper Service Function Declarations *************************

        void addRepository(std::vector<std::unique_ptr<BICDeviceInfoStruct>>* theImplantsAddress);

        // ************************* Construction, Initialization, and Destruction Function Declarations *************************
        grpc::Status VersionNumber(grpc::ServerContext* context, const BICgRPC::VersionNumberRequest* request, BICgRPC::VersionNumberResponse* reply) override;

        grpc::Status SupportedDevices(grpc::ServerContext* context, const BICgRPC::SupportedDevicesRequest* request, BICgRPC::SupportedDevicesResponse* reply) override;

        grpc::Status InspectRepository(grpc::ServerContext* context, const BICgRPC::InspectRepositoryRequest* request, BICgRPC::InspectRepositoryResponse* reply) override;

    };
}