#include "BICInfoGRPCService.h"

// BIC Usings
using namespace BICGRPCHelperNamespace;

// BIC Info Service Usings
using BICgRPC::BICInfoService;
using BICgRPC::VersionNumberRequest;
using BICgRPC::VersionNumberResponse;
using BICgRPC::SupportedDevicesRequest;
using BICgRPC::SupportedDevicesResponse;
using BICgRPC::InspectRepositoryRequest;
using BICgRPC::InspectRepositoryResponse;

namespace BICGRPCHelperNamespace
{
    void BICInfoGRPCService::addRepository(std::vector<std::unique_ptr<BICDeviceInfoStruct>>* theImplantsAddress)
    {
        infoVector = theImplantsAddress;
    }

    grpc::Status BICInfoGRPCService::VersionNumber(grpc::ServerContext* context, const BICgRPC::VersionNumberRequest* request, BICgRPC::VersionNumberResponse* reply) {
        std::string returnMessage = "0.0.1";
        reply->set_version_number(returnMessage);
        return grpc::Status::OK;
    }

    grpc::Status BICInfoGRPCService::SupportedDevices(grpc::ServerContext* context, const BICgRPC::SupportedDevicesRequest* request, BICgRPC::SupportedDevicesResponse* reply) {
        std::string returnMessage = "Cortec BrainInterchange";
        reply->add_supported_devices(returnMessage);
        return grpc::Status::OK;
    }

    grpc::Status BICInfoGRPCService::InspectRepository(grpc::ServerContext* context, const BICgRPC::InspectRepositoryRequest* request, BICgRPC::InspectRepositoryResponse* reply) {
        for (const auto& i : *infoVector) {
            reply->add_repo_uri(i->deviceAddress);
        }
        return grpc::Status::OK;
    }
}