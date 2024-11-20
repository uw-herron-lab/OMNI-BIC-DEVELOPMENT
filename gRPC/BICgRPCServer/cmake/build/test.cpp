
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include <grpcpp/security/server_credentials.h>
#include "../../ClassesSource/BICBridgeGRPCService.h"
#include "../../ClassesSource/BICDeviceGRPCService.h"
#include "../../ClassesSource/BICInfoGRPCService.h"
#include <MockIImplantFactory.h>
#include <MockIImplant.h>

using testing::Return;
using BICgRPC::BICBridgeService;


TEST(BICBridgeGRPCServiceTest, ListBridgesWithoutConnections) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);
    const std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo{};

    // Mock up a manager without any connections
    EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));


    grpc::ServerContext* context;
    const BICgRPC::QueryBridgesRequest* request;
    BICgRPC::QueryBridgesResponse* reply;
    grpc::Status val = bridgeService.ListBridges(context, request, reply);




    // Check that when no bridges are connected, listBridges can be called without any error
    EXPECT_EQ(val.ok(), true);

    // Call destructor
    mockManager->~MockIImplantFactory();
   


}


TEST(BICBridgeGRPCServiceTest, ListBridgesWithOneConnections) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);


    const std::string& deviceType = "DeviceType";
    const std::string& hardwareRevision = "Hardware";
    const std::string& deviceId = "deviceId";
    const std::string& firmwareVersion = "firmwareVersion";

    cortec::implantapi::CExternalUnitInfo* unitInfo1;
    unitInfo1 = new cortec::implantapi::CExternalUnitInfo(deviceType, hardwareRevision, deviceId, firmwareVersion);
    std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo;
    unitInfo.push_back(unitInfo1);

    // Mock up a manager without any connections
    EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));


    grpc::ServerContext* context;
    const BICgRPC::QueryBridgesRequest* request;
    BICgRPC::QueryBridgesResponse* reply;
    grpc::Status val = bridgeService.ListBridges(context, request, reply);




    // Check that when one bridges is connected, listBridges can be called without any error
    EXPECT_EQ(val.ok(), true);

    // Call destructor
    mockManager->~MockIImplantFactory();




}


TEST(BICBridgeGRPCServiceTest, ScanBridgesWithoutConnections) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);
    const std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo{};

    // Mock up a manager without any connections
    EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));


    grpc::ServerContext* context;
    const BICgRPC::QueryBridgesRequest* request;
    BICgRPC::QueryBridgesResponse* reply;
    grpc::Status val = bridgeService.ScanBridges(context, request, reply);




    // Check that when no bridges are connected, scanBridges can be called without any error
    EXPECT_EQ(val.ok(), true);

    // Call destructor
    mockManager->~MockIImplantFactory();



}



TEST(BICBridgeGRPCServiceTest, ScanBridgesWithOneConnections) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);


    const std::string& deviceType = "DeviceType";
    const std::string& hardwareRevision = "Hardware";
    const std::string& deviceId = "deviceId";
    const std::string& firmwareVersion = "firmwareVersion";

    cortec::implantapi::CExternalUnitInfo* unitInfo1;
    unitInfo1 = new cortec::implantapi::CExternalUnitInfo(deviceType, hardwareRevision, deviceId, firmwareVersion);
    std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo;
    unitInfo.push_back(unitInfo1);

    // Mock up a manager without any connections
    EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));


    grpc::ServerContext* context;
    const BICgRPC::QueryBridgesRequest* request;
    BICgRPC::QueryBridgesResponse* reply;
    grpc::Status val = bridgeService.ScanBridges(context, request, reply);




    // Check that when one bridge is connected, scanBridges can be called without any error
    EXPECT_EQ(val.ok(), true);

    // Call destructor
    mockManager->~MockIImplantFactory();




}


TEST(BICBridgeGRPCServiceTest, ConnectedBridgesWithoutConnections) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);
    const std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo{};

    // Mock up a manager without any connections
    EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));


    grpc::ServerContext* context;
    const BICgRPC::QueryBridgesRequest* request;
    BICgRPC::QueryBridgesResponse* reply;
    grpc::Status val = bridgeService.ConnectedBridges(context, request, reply);




    // Check that when no bridges are connected, connectedBridges can be called without any error
    EXPECT_EQ(val.ok(), true);

    // Call destructor
    mockManager->~MockIImplantFactory();



}



TEST(BICBridgeGRPCServiceTest, ConnectedBridgeWithOneConnection) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);


    const std::string& deviceType = "DeviceType";
    const std::string& hardwareRevision = "Hardware";
    const std::string& deviceId = "deviceId";
    const std::string& firmwareVersion = "firmwareVersion";

    cortec::implantapi::CExternalUnitInfo* unitInfo1;
    unitInfo1 = new cortec::implantapi::CExternalUnitInfo(deviceType, hardwareRevision, deviceId, firmwareVersion);
    std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo;
    unitInfo.push_back(unitInfo1);

    // Mock up a manager without any connections
    EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));


    grpc::ServerContext* context;
    const BICgRPC::QueryBridgesRequest* request;
    BICgRPC::QueryBridgesResponse* reply;
    grpc::Status val = bridgeService.ConnectedBridges(context, request, reply);




    // Check that when one bridge is connected, connectedBridges can be called without any error
    EXPECT_EQ(val.ok(), true);

    // Call destructor
    mockManager->~MockIImplantFactory();




}


TEST(BICBridgeGRPCServiceTest, ConnectBridgeWithoutConnections) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);
    const std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo{};

    // Mock up a manager without any connections
    EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));


    grpc::ServerContext* context;
    const BICgRPC::ConnectBridgeRequest* request;
    BICgRPC::ConnectBridgeResponse* reply;
    grpc::Status val = bridgeService.ConnectBridge(context, request, reply);




    // Check that when no bridges are connected, connectBridge can be called without any error
    EXPECT_EQ(val.ok(), true);

    // Call destructor
    mockManager->~MockIImplantFactory();



}


TEST(BICBridgeGRPCServiceTest, ConnectBridgeWithOneConnectionsIncorrectDeviceId) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);

    const std::string& deviceType = "DeviceType";
    const std::string& hardwareRevision = "Hardware";
    const std::string& deviceId = "deviceId";
    const std::string& firmwareVersion = "firmwareVersion";

    cortec::implantapi::CExternalUnitInfo* unitInfo1;
    unitInfo1 = new cortec::implantapi::CExternalUnitInfo(deviceType, hardwareRevision, deviceId, firmwareVersion);
    std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo;
    unitInfo.push_back(unitInfo1);


    grpc::ServerContext* context;
    const BICgRPC::ConnectBridgeRequest* request;
    BICgRPC::ConnectBridgeResponse* reply;
    grpc::Status val = bridgeService.ConnectBridge(context, request, reply);




    // Check that when one bridge is connected, connectBridge can be called without any error
    EXPECT_EQ(val.ok(), true);

    // Check that request name was correctly set
    EXPECT_EQ(request->name(), reply->name());


    // Check that conenction status was set correctly
    EXPECT_EQ(reply->connection_status(), (BICgRPC::ConnectBridgeStatus)2);

    // Call destructor
    mockManager->~MockIImplantFactory();



}



TEST(BICBridgeGRPCServiceTest, ConnectBridgeWithOneConnectionsCorrectDeviceId) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);

    const std::string& deviceType = "DeviceType";
    const std::string& hardwareRevision = "Hardware";
    const std::string& deviceId = "deviceId";
    const std::string& firmwareVersion = "firmwareVersion";
    const char* requestName = "//bic/bridge/deviceId";

    cortec::implantapi::CExternalUnitInfo* unitInfo1;
    unitInfo1 = new cortec::implantapi::CExternalUnitInfo(deviceType, hardwareRevision, deviceId, firmwareVersion);
    std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo;
    unitInfo.push_back(unitInfo1);


    grpc::ServerContext* context;
    BICgRPC::ConnectBridgeRequest* request;
    request->set_name(requestName);
    BICgRPC::ConnectBridgeResponse* reply;
    grpc::Status val = bridgeService.ConnectBridge(context, request, reply);




    // Check that when bridge is connected, connectBridge can be called without any error
    EXPECT_EQ(val.ok(), true);

    // Check that request name was correctly set
    EXPECT_EQ(request->name(), reply->name());

    
    // Check that conenction status was set correctly
    EXPECT_EQ(reply->connection_status(),(BICgRPC::ConnectBridgeStatus)1);

    // Call destructor
    mockManager->~MockIImplantFactory();



}

TEST(BICBridgeGRPCServiceTest, DescribeBridgeWithoutConnections) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);
    const std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo{};

    // Mock up a manager without any connections
    EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));


    grpc::ServerContext* context;
    const BICgRPC::DescribeBridgeRequest* request;
    BICgRPC::DescribeBridgeResponse* reply;
    grpc::Status val = bridgeService.DescribeBridge(context, request, reply);




    // Check that when no bridges are connected, describeBridge can be called without any error
    EXPECT_EQ(val.ok(), true);

    // Call destructor
    mockManager->~MockIImplantFactory();



}


TEST(BICBridgeGRPCServiceTest, DescribeBridgeWithOneConnectionIncorrectDeviceId) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);

    const std::string& deviceType = "DeviceType";
    const std::string& hardwareRevision = "Hardware";
    const std::string& deviceId = "deviceId";
    const std::string& firmwareVersion = "firmwareVersion";
    const char* requestName = "//bic/bridge/deviceId";

    cortec::implantapi::CExternalUnitInfo* unitInfo1;
    unitInfo1 = new cortec::implantapi::CExternalUnitInfo(deviceType, hardwareRevision, deviceId, firmwareVersion);
    std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo;
    unitInfo.push_back(unitInfo1);


    grpc::ServerContext* context;
    const BICgRPC::DescribeBridgeRequest* request;
    BICgRPC::DescribeBridgeResponse* reply;
    grpc::Status val = bridgeService.DescribeBridge(context, request, reply);




    // Check that when one bridge is connected, describeBridge can be called without any error
    EXPECT_EQ(val.ok(), true);


    // Check that reply name was not set
    ASSERT_NE(reply->name(), requestName);

    // Call destructor
    mockManager->~MockIImplantFactory();



}


TEST(BICBridgeGRPCServiceTest, DescribeBridgeWithOneConnectionsCorrectDeviceId) {

    MockIImplantFactory* mockManager;
    MockIImplant mockDevice;
    BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
    BICGRPCHelperNamespace::BICDeviceGRPCService deviceService;
    BICGRPCHelperNamespace::BICInfoGRPCService infoService;
    std::unique_ptr<::grpc::Server> gRPCServer;
    std::string server_address("0.0.0.0:50051");
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    // Listen on the given address without any authentication mechanism.
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // ******************* Build up BIC Services *******************
    bridgeService.passFactory(mockManager);
    deviceService.passFactory(mockManager);
    infoService.addRepository(&deviceService.theImplants);


    // ******************* Start up the gRPC BIC Server *******************
    // Register "service" as the instance through which we'll communicate with clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&deviceService);
    builder.RegisterService(&bridgeService);
    builder.RegisterService(&infoService);
    // Finally assemble the server.
    gRPCServer = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;



    // Create a client that connects to the service.
    char* address = "127.0.0.1:50051";
    const std::shared_ptr<grpc::ChannelCredentials> credentials = grpc::InsecureChannelCredentials();
    std::shared_ptr < grpc::Channel> channel = grpc::CreateChannel("127.0.0.1:50051", credentials);

    const std::string& deviceType = "DeviceType";
    const std::string& hardwareRevision = "Hardware";
    const std::string& deviceId = "deviceId";
    const std::string& firmwareVersion = "firmwareVersion";
    const char* requestName = "//bic/bridge/deviceId";

    cortec::implantapi::CExternalUnitInfo* unitInfo1;
    unitInfo1 = new cortec::implantapi::CExternalUnitInfo(deviceType, hardwareRevision, deviceId, firmwareVersion);
    std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo;
    unitInfo.push_back(unitInfo1);


    grpc::ServerContext* context;
    BICgRPC::DescribeBridgeRequest* request;
    request->set_name(requestName);
    BICgRPC::DescribeBridgeResponse* reply;
    grpc::Status val = bridgeService.DescribeBridge(context, request, reply);




    // Check that when bridge is connected, connectBridge can be called without any error
    EXPECT_EQ(val.ok(), true);

    const std::string& recieved = reply->name();
    // Check that reply name was correctly set
    EXPECT_EQ(recieved, requestName);


    // Call destructor
    mockManager->~MockIImplantFactory();



}