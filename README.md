# BrainInterchange gRPC Microservice Source (C++) - Setup and Compilation Instructions

First follow general instructions for C++ gRPC Installation
https://grpc.io/docs/languages/cpp/quickstart/

gRPC Install/Update Instructions:
1) delete pre-existing folders in /d/gitbuilds (or wherever) if they exist
2) Refer to https://grpc.io/docs/languages/cpp/quickstart/ for more details but basically...
3) Open gitbash in /d/gitbuilds (or wherever)
4) copy/paste "export MY_INSTALL_DIR=/d/gitbuilds"
5) clone the repo from github -> "git clone --recurse-submodules -b v1.38.0 https://github.com/grpc/grpc" (or later)
6) do all of the following:
	$ cd grpc
	$ mkdir -p cmake/build
	$ pushd cmake/build
	$ cmake -DgRPC_INSTALL=ON \
      		-DgRPC_BUILD_TESTS=OFF \
      		-DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      		../..
7) Without closing gitbash, go into cmake/build and open up the "Install.vcxproj" in visual studio
8) Build the install project
9) Next:
	$ popd
	$ mkdir -p third_party/abseil-cpp/cmake/build
	$ pushd third_party/abseil-cpp/cmake/build
	$ cmake -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      		-DCMAKE_POSITION_INDEPENDENT_CODE=TRUE \
      		../..
10) Without closing gitbash, go into cmake/build and open up the "Install.vcxproj" in visual studio
11) Build the install project

Setting up the Protobuf build instructions:
1) Create cmake/build subdirectory (starting from this directory)
2) Open gitbash IN cmake/build
3) USE "export MY_INSTALL_DIR=/d/gitbuilds" (or whatever the directory is where you set grpc to install to)
4) use "cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../.."
5) Open the BICgRPCdev.sln in the base BrainInterchange-Development/gRPC directory
6) Open the BICgRPCmicroserver properties
7) add BIC API cpp include folder to project C/C++ Additional Include Directories -> "C:\Program Files\Cortec\Bicapi\cppapi\include"
8) add BIC Libs to Linker General Additional Libary Directories -> "C:\Program Files\Cortec\Bicapi\cppapi\lib64"
9) add "bicapid.lib;" to Additional Dependencies on Linker Input Properties
10) right click on the BICgRPCmicroserver project and selected Add->Add Existing
11) select all source files in BICgRPCServer\ClassesSource\ and add to the project
11) copy cortec provided DLLs into build/debug
