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
    class BICBridgeGRPCService final : public BICgRPC::BICBridgeService::Service {
    public:
        // ************************* Cross-Function Service Variable Declarations *************************
        cortec::implantapi::IImplantFactory* theImplantFactory;
        std::mutex rpcLock;

        // Initialize Service with a Factory
        void passFactory(cortec::implantapi::IImplantFactory* serverFactory);

        // ************************* General Service Function Declarations *************************
        grpc::Status ListBridges(grpc::ServerContext* context, const BICgRPC::QueryBridgesRequest* request, BICgRPC::QueryBridgesResponse* reply) override;

        grpc::Status ScanBridges(grpc::ServerContext* context, const BICgRPC::QueryBridgesRequest* request, BICgRPC::QueryBridgesResponse* reply) override;

        grpc::Status ConnectedBridges(grpc::ServerContext* context, const BICgRPC::QueryBridgesRequest* request, BICgRPC::QueryBridgesResponse* reply) override;

        grpc::Status ConnectBridge(grpc::ServerContext* context, const BICgRPC::ConnectBridgeRequest* request, BICgRPC::ConnectBridgeResponse* reply) override;

        grpc::Status DescribeBridge(grpc::ServerContext* context, const BICgRPC::DescribeBridgeRequest* request, BICgRPC::DescribeBridgeResponse* reply) override;

        grpc::Status DisconnectBridge(grpc::ServerContext* context, const BICgRPC::DisconnectBridgeRequest* request, google::protobuf::Empty* reply) override;
    };
}
