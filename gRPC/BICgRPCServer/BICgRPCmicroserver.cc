#include <cppapi/bicapi.h>
#include <cppapi/IImplantListener.h>
#include <cppapi/IImplantFactory.h>
#include <cppapi/IStimulationCommand.h>

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
#include "ClassesSource/BICListener.h"
#include "ClassesSource/BICDeviceGRPCService.h"
#include "ClassesSource/BICBridgeGRPCService.h"
#include "ClassesSource/BICInfoGRPCService.h"


// BIC Usings
using namespace cortec::implantapi;
using namespace BICGRPCHelperNamespace;

// GRPC Usings
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

// Server Objects
std::unique_ptr<Server> gRPCServer;
BICDeviceGRPCService deviceService;
BICBridgeGRPCService bridgeService;
BICInfoGRPCService infoService;

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
    infoService.addRepository(&deviceService.theImplants);
    
    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
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
