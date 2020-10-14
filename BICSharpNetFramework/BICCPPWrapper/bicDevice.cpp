#include "pch.h"
#include "bicDevice.h"

using namespace std;

// Constructor
bicCppWrapper::bicCppWrapper()
{
	return;
}

bicCppWrapper theBicCppWrapper;

EXTERN_C __declspec(dllexport) int __cdecl initialize(void)
{
	return theBicCppWrapper.count++;
}

