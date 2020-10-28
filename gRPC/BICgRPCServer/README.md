# BrainInterchange gRPC Microservice Source (C++)

First follow general instructions for C++ gRPC Installation
https://grpc.io/docs/languages/cpp/quickstart/

Build Instructions:
1) Create cmake/build subdirectory (starting from this directory)
2) Open gitbash IN cmake/build
3) USE "export MY_INSTALL_DIR=/d/gitbuilds" (or whatever the directory is where you set grpc to install to)
4) use "cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../.."
5) add BIC API cpp include folder to project C/C++ Additional Include Directories
	a) C:\Program Files\Cortec\Bicapi\cppapi\include
6) add BIC Libs to Linker General Additional Libary Directories
	b) C:\Program Files\Cortec\Bicapi\cppapi\lib64
7) add "bicapid.lib;" to Additional Dependencies on Linker Input Properties
8) copy cortec provided DLLs into build/debug