
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include <grpcpp/security/server_credentials.h>
#include "../../ClassesSource/BICBridgeGRPCService.h"
#include "../../ClassesSource/BICDeviceGRPCService.h"
#include "../../ClassesSource/BICInfoGRPCService.h"
#include <MockIImplantFactory.h>
#include <MockIImplant.h>
#include <MockCExternalUnitInfo.h>

using testing::Return;
using BICgRPC::BICBridgeService;

class BICBridgeGRPCServiceTest : public testing::Test {

protected:

	// Per-test-suite set-up.
	// Called before the first test in this test suite.
	static void SetUpTestSuite() {

		const std::string& deviceType = "DeviceType";
		const std::string& hardwareRevision = "Hardware";
		const std::string& deviceId = "deviceId";
		const std::string& firmwareVersion = "firmwareVersion";
		unitInfo1 = new cortec::implantapi::CExternalUnitInfo(deviceType, hardwareRevision, deviceId, firmwareVersion);

	}

	// Per-test-suite tear-down.
	// Called after the last test in this test suite.
	static void TearDownTestSuite() {
		delete unitInfo1;
		unitInfo1 = nullptr;
	}

	// Per-test set-up
	// Called before each test
	void SetUp() override {

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

	}

	// Per-test teardown
	// Called after each test
	void TearDown() override {
		delete mockManager;
		mockManager = nullptr;

	}

	// Resources shared by all tests.
	static cortec::implantapi::CExternalUnitInfo* unitInfo1;
	static BICGRPCHelperNamespace::BICBridgeGRPCService bridgeService;
	static MockIImplantFactory* mockManager;

};

// ExternalUnitInfo object containing info about dummy object 
cortec::implantapi::CExternalUnitInfo* BICBridgeGRPCServiceTest::unitInfo1 = nullptr;

// BicBridgeGRPCService object that connects to mock objects
BICGRPCHelperNamespace::BICBridgeGRPCService BICBridgeGRPCServiceTest::bridgeService;

// Mock of a IImplantFactory object
MockIImplantFactory* BICBridgeGRPCServiceTest::mockManager = nullptr;

TEST_F(BICBridgeGRPCServiceTest, ListBridgesWithoutConnections) {
	const std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo{};

	// Mock up a manager without any connections
	EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));

	grpc::ServerContext* context;
	const BICgRPC::QueryBridgesRequest* request;
	BICgRPC::QueryBridgesResponse* reply;
	grpc::Status val = bridgeService.ListBridges(context, request, reply);

	// Check that when no bridges are connected, listBridges can be called without any error
	EXPECT_EQ(val.ok(), true);

	// Check that mumber of connected bridges is 0
	EXPECT_EQ(reply->bridges_size(), 0);

	// Call destructor
	mockManager->~MockIImplantFactory();
}


TEST_F(BICBridgeGRPCServiceTest, ListBridgesWithOneConnections) {

	const std::string& deviceType = "DeviceType";
	const std::string& hardwareRevision = "Hardware";
	const std::string& deviceId = "deviceId";
	const std::string& firmwareVersion = "firmwareVersion";


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

	// Check that reply has one connected bridge
	EXPECT_EQ(reply->bridges_size(), 1);

	BICgRPC::Bridge connectedBridge = reply->bridges(0);
	const std::string expectedName = "//bic/bridge/deviceId";
	// Check that bridge has expected name
	EXPECT_EQ(connectedBridge.name(), expectedName);

	// Call destructor
	mockManager->~MockIImplantFactory();

}


TEST_F(BICBridgeGRPCServiceTest, ScanBridgesWithoutConnections) {

	const std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo{};

	// Mock up a manager without any connections
	EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));

	grpc::ServerContext* context;
	const BICgRPC::QueryBridgesRequest* request;
	BICgRPC::QueryBridgesResponse* reply;
	grpc::Status val = bridgeService.ScanBridges(context, request, reply);

	// Check that when no bridges are connected, scanBridges can be called without any error
	EXPECT_EQ(val.ok(), true);

	// Check that mumber of connected bridges is 0
	EXPECT_EQ(reply->bridges_size(), 0);

	// Call destructor
	mockManager->~MockIImplantFactory();

}



TEST_F(BICBridgeGRPCServiceTest, ScanBridgesWithOneConnections) {

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

	BICgRPC::Bridge connectedBridge = reply->bridges(0);
	const std::string expectedName = "//bic/bridge/deviceId";
	// Check that bridge has expected name
	EXPECT_EQ(connectedBridge.name(), expectedName);

	// Call destructor
	mockManager->~MockIImplantFactory();

}


TEST_F(BICBridgeGRPCServiceTest, ConnectedBridgesWithoutConnections) {
	const std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo{};

	// Mock up a manager without any connections
	EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));

	grpc::ServerContext* context;
	const BICgRPC::QueryBridgesRequest* request;
	BICgRPC::QueryBridgesResponse* reply;
	grpc::Status val = bridgeService.ConnectedBridges(context, request, reply);

	// Check that when no bridges are connected, connectedBridges can be called without any error
	EXPECT_EQ(val.ok(), true);

	// Check that mumber of connected bridges is 0
	EXPECT_EQ(reply->bridges_size(), 0);

	// Call destructor
	mockManager->~MockIImplantFactory();

}



TEST_F(BICBridgeGRPCServiceTest, ConnectedBridgeWithOneConnection) {

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

	// Check that mumber of connected bridges is 1
	EXPECT_EQ(reply->bridges_size(), 1);
	
	// Check values of reply
	BICgRPC::Bridge connectedBridge = reply->bridges(0);
	const std::string expectedName = "//bic/bridge/deviceId";
	EXPECT_EQ(connectedBridge.name(), expectedName);
	EXPECT_EQ(connectedBridge.firmwareversion(), "firmwareVersion");
	EXPECT_EQ(connectedBridge.devicetype(), "deviceType");
	EXPECT_EQ(connectedBridge.deviceid(), "deviceId");

	// Call destructor
	mockManager->~MockIImplantFactory();

}


TEST_F(BICBridgeGRPCServiceTest, ConnectBridgeWithoutConnections) {

	const std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo{};

	// Mock up a manager without any connections
	EXPECT_CALL(*mockManager, getExternalUnitInfos).WillOnce(Return(unitInfo));

	grpc::ServerContext* context;
	const BICgRPC::ConnectBridgeRequest* request;
	BICgRPC::ConnectBridgeResponse* reply;
	grpc::Status val = bridgeService.ConnectBridge(context, request, reply);

	// Check that when no bridges are connected, connectBridge can be called without any error
	EXPECT_EQ(val.ok(), true);

	// Check that request name was correctly set
	EXPECT_EQ(request->name(), reply->name());

	// Check that conenction status was set correctly
	EXPECT_EQ(reply->connection_status(), (BICgRPC::ConnectBridgeStatus)2);

	// Call destructor
	mockManager->~MockIImplantFactory();

}


TEST_F(BICBridgeGRPCServiceTest, ConnectBridgeWithOneConnectionsIncorrectDeviceId) {

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



TEST_F(BICBridgeGRPCServiceTest, ConnectBridgeWithOneConnectionsCorrectDeviceId) {
	const char* requestName = "//bic/bridge/deviceId";
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
	EXPECT_EQ(reply->connection_status(), (BICgRPC::ConnectBridgeStatus)1);

	// Call destructor
	mockManager->~MockIImplantFactory();

}

TEST_F(BICBridgeGRPCServiceTest, DescribeBridgeWithoutConnections) {

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


TEST_F(BICBridgeGRPCServiceTest, DescribeBridgeWithOneConnectionIncorrectDeviceId) {

	const char* requestName = "//bic/bridge/deviceId";

	std::vector<cortec::implantapi::CExternalUnitInfo*> unitInfo;
	unitInfo.push_back(unitInfo1);

	grpc::ServerContext* context;
	const BICgRPC::DescribeBridgeRequest* request;
	BICgRPC::DescribeBridgeResponse* reply;
	grpc::Status val = bridgeService.DescribeBridge(context, request, reply);

	// Check that whensn one bridge is connected, describeBridge can be called without any error
	EXPECT_EQ(val.ok(), true);

	// Check that reply name was not set
	ASSERT_NE(reply->name(), requestName);

	// Call destructor
	mockManager->~MockIImplantFactory();

}


TEST_F(BICBridgeGRPCServiceTest, DescribeBridgeWithOneConnectionsCorrectDeviceId) {

	const char* requestName = "//bic/bridge/deviceId";
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