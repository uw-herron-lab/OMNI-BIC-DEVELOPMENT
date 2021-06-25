#include "BICBridgeGRPCService.h"

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

namespace BICGRPCHelperNamespace
{

    // Initialize Service with a Factory
    void BICBridgeGRPCService::passFactory(cortec::implantapi::IImplantFactory* serverFactory)
    {
        theImplantFactory = serverFactory;
    }

    // ************************* General Service Function Declarations *************************
    grpc::Status BICBridgeGRPCService::ListBridges(grpc::ServerContext* context, const BICgRPC::QueryBridgesRequest* request, BICgRPC::QueryBridgesResponse* reply) {
        // Finds all bridges connected to the PC
        std::vector<cortec::implantapi::CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();

        for (int i = 0; i < exInfos.size(); i++)
        {
            //construct URIs
            Bridge* aBridge = reply->add_bridges();
            aBridge->set_name("//bic/bridge/" + exInfos[i]->getDeviceId());
        }

        return grpc::Status::OK;
    }

    grpc::Status BICBridgeGRPCService::ScanBridges(grpc::ServerContext* context, const BICgRPC::QueryBridgesRequest* request, BICgRPC::QueryBridgesResponse* reply) {
        // Finds all bridges connected to the PC
        std::vector<cortec::implantapi::CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();

        for (int i = 0; i < exInfos.size(); i++)
        {
            //construct URIs
            Bridge* aBridge = reply->add_bridges();
            aBridge->set_name("//bic/bridge/" + exInfos[i]->getDeviceId());
        }

        return grpc::Status::OK;
    }

    grpc::Status BICBridgeGRPCService::ConnectedBridges(grpc::ServerContext* context, const BICgRPC::QueryBridgesRequest* request, BICgRPC::QueryBridgesResponse* reply) {
        // All bridges found by this function are connected to the PC
        std::vector<cortec::implantapi::CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();

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
        return grpc::Status::OK;
    }

    grpc::Status BICBridgeGRPCService::ConnectBridge(grpc::ServerContext* context, const BICgRPC::ConnectBridgeRequest* request, BICgRPC::ConnectBridgeResponse* reply) {
        // All bridges are by definition 'connected' via USB, so if it exists in the list it's connected
        reply->set_name(request->name());
        std::vector<cortec::implantapi::CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();

        for (int i = 0; i < exInfos.size(); i++)
        {
            if ("//bic/bridge/" + exInfos[i]->getDeviceId() == request->name())
            {
                reply->set_connection_status((ConnectBridgeStatus)1);
                return grpc::Status::OK;
            }
        }

        reply->set_connection_status((ConnectBridgeStatus)2);
        return grpc::Status::OK;
    }

    grpc::Status BICBridgeGRPCService::DescribeBridge(grpc::ServerContext* context, const BICgRPC::DescribeBridgeRequest* request, BICgRPC::DescribeBridgeResponse* reply) {
        std::vector<cortec::implantapi::CExternalUnitInfo*> exInfos = theImplantFactory->getExternalUnitInfos();

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
                return grpc::Status::OK;
            }
        }

        return grpc::Status::OK;
    }

    grpc::Status BICBridgeGRPCService::DisconnectBridge(grpc::ServerContext* context, const BICgRPC::DisconnectBridgeRequest* request, google::protobuf::Empty* reply) {
        // All bridges are by definition 'connected' via USB, so they can't be disconnected
        return grpc::Status::OK;
    }
};