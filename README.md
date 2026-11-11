# BrainInterchange gRPC Microservice Source (C++)

First follow general instructions for C++ gRPC Installation
https://grpc.io/docs/languages/cpp/quickstart/

gRPC Install/Update Instructions:
1) delete pre-existing folders in /d/gitbuilds (or wherever) if they exist
2) Refer to https://grpc.io/docs/languages/cpp/quickstart/ for more details but basically...
3) Open gitbash in /c/gitbuilds (or wherever)
4) copy/paste "export MY_INSTALL_DIR=/c/gitbuilds"
5) clone the repo from github -> "git clone --recurse-submodules -b v1.45.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc"
6) do all of the following:
	$ cd grpc
	$ mkdir -p cmake/build
	$ pushd cmake/build
	$ cmake -DgRPC_INSTALL=ON \
      		-DgRPC_BUILD_TESTS=OFF \
			-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      		-DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      		../..
7) Without closing gitbash, go into cmake/build and open up the "Install.vcxproj" in visual studio (C:\gitbuildspublic\grpc\cmake\build)
8) Build the install project (Find Install in solution explorer, right click, then select build)
9) Next:
	$ popd
	$ mkdir -p third_party/abseil-cpp/cmake/build
	$ pushd third_party/abseil-cpp/cmake/build
	$ cmake -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      		-DCMAKE_POSITION_INDEPENDENT_CODE=TRUE \
      		../..
10) Without closing gitbash, go into cmake/build and open up the "Install.vcxproj" in visual studio ( C:/gitbuilds/grpc/third_party/abseil-cpp/CMake/build)
11) Build the install project (Find install in solution explorer, right click, then select build)

### BIC Setup
1) Clone the BrainInterchange-Development repo in /c/gitbuilds
2) Go to BrainInterchange-Development/gRPC/BICgRPCServer and create cmake/build subdirectory (can use same commands as above) (i.e. full directory: C:\gitbuilds\BrainInterchange-Development\gRPC\BICgRPCServer\cmake\build)
2) Open gitbash IN cmake/build
3) USE "export MY_INSTALL_DIR=/c/gitbuilds" (or whatever the directory is where you set grpc to install to)
4) use "cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../.."
5) Open the BICgRPCdev.sln in the base BrainInterchange-Development/gRPC directory
6) Open the BICgRPCmicroserver properties
7) Add BIC API cpp include folder to project C/C++ Additional Include Directories
	a) On right hand panel, go to C/C++ > General and find the Additional Include Directories
	b) Edit to add include folder of BIC C++ API (C:\Program Files\Cortec\Bicapi\cppapi\include)
8) Add BIC Libs to Linker General Additional Libary Directories
	a) On right hand panel, go to Linker > General and find Additional Library Directories
	b) Edit to add library folder of BIC C++ API (C:\Program Files\Cortec\Bicapi\cppapi\lib64)
9) Add "bicapid.lib;" to Additional Dependencies on Linker Input Properties
	a) On right hand panel, go to Linker > Input and find Additional Dependencies
	b) Edit and add "bicapid.lib"
10) Right click on the BICgRPCmicroserver project and selected Add -> Existing (specifically right click on Source Files folder under microserver)
11) Select all source files in BICgRPCServer\ClassesSource\ and add to the project
	a) Full path of source files is C:\gitbuilds\OMNI-BIC-DEVELOPMENT\gRPC\BICgRPCServer\ClassesSource
12) Copy cortec provided DLLs into build/debug (right click microserver, open file explorer, and look for build/debug folder to put .dll files)
	a) If there is no Debug folder, create a new one (C:\gitbuilds\OMNI-BIC-DEVELOPMENT\gRPC\BICgRPCServer\cmake\build\Debug)
	b) DLL files are located in the cppapi folder where BIC API was installed (C:\Program Files\Cortec\Bicapi\cppapi\lib64)
13) Build the solution


### Troubleshooting
1) There may be an error that appears: This project references NuGet package(s) that are missing on this computer. 
	a) Reference this link for solution https://stackoverflow.com/questions/22909506/this-project-references-nuget-packages-that-are-missing-on-this-computer
	b) Under the project, click on the arrow next to References. There will be some references with a yellow triangle and exclamation mark. Right click on those packages and select Remove.
	c) Right click on the project and got ot Manage NuGet packages.
	d) Reinstall the previously removed packages. Alternatively, can update to a newer version and revert to the previous version (latter step is done only if desired)
2) There may be a "The type or namespace name could not be found" error. This most likely will appear under the gRPCClient_ConsoleTest project. To solve this, perform the following:
	a) Expand "References" under the project
	b) Identify references that have a yellow triangular warning symbol
	c) For any reference with the above symbol, right click on it and select "Remove"
	d) Go to Tools -> NuGet Package Manager -> Manage NuGet Pacakges for Solution
	e) On the left pane of the window that appears, select the package that was removed. Select the checkbox next to "Project" in the right pane, and then Install.
	f) Perform for all references
	Note: This may change the .csproj file and the package number used. Do not push these changes when performing any commits.

3) There may be an error that appears when trying to build the project: "Metadata file 'C:\gitbuilds\OMNI-BIC-DEVELOPMENT\gRPC\Client Examples\BICgRPC_wpfLib\bin\Debug\BICgRPC_wpfLib.dll' could not be found. You may need to remove and reinstall Google.Protobuf.Tools package. Refer to the resolution in 2)
	Note: You need to install the original version, installing the latest version will bring up build issues
	Note: Certain projects might have this error, so identify which projects require this step


Setting to access API Logs:
install Bicapi, in python script, change the directory to the directory of where bip2x is, and it should not be in the Programs subdirectory for access permissions reasons
script converts the api log to csv, and the json takes the data we are interested in. csv doesn't extract the headers, so the script also further adds the labels to the csv files.

0 - good
1 or 2 - some issue in getting the data (2 - issue with trying to create the file; 1 - some issue with the bip2x itself, make sure the path, permission and versions match)
json file has a version number, if the version number doesn't match the one of the api or the data, error will also be thrown