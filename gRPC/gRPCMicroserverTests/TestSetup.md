# Test Setup

First follow general instructions for C++ gRPC Installation and OMNI-BIC readme

##Initialization: 
1) Install nuget package gmock by google
	Add gmock-all, gtest-all and gmock_main
	May need to load/unload project
	More detailed steps: c++ - How to use GoogleMock in Visual Studio? - Stack Overflow 
	Disable precompiled headers 
2) In the C/C++ Additional Include, add
	cmake/build from repo
	grpc/third_party/abseil-cpp
	grpc/third_party/protobuf/src
	grpc/include
	grpc/include/grpcpp
3) In Linker-> General, add
	"C:\Program Files\Cortec\Bicapi\cppapi\include"
	"C:\Program Files\Cortec\Bicapi\cppapi\lnclude\cppapi"

##Debugging issues

# Can’t run file because it’s an executable
Set the startup project to be the test project
# Bicapid.dll can’t be found when running the debugger
Go to the project’s properties and add in the path to the bicapid.dll file. PATH=c:\path\where\the\dll-is;$(Path)

## When setting up a new googletest project

1) Create new google test project
2) Uninstall provided nuget package 
3) Follow initialization steps
4) When importing packages, ensure that either your test file has a mian function, or gmock_main is added to the project
