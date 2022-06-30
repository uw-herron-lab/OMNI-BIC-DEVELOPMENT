#pragma once
#include <cppapi/bicapi.h>
#include <cppapi/ImplantInfo.h>
#include <cppapi/IImplant.h>
#include <string>
#include "BICListener.h"

namespace BICGRPCHelperNamespace {
    struct BICDeviceInfoStruct
    {
        // Identifiers
        std::string deviceAddress;
        std::string bridgeId;
        std::string deviceId;

        // BIC Device-specific Objects
        std::unique_ptr <cortec::implantapi::IImplant> theImplant;
        std::unique_ptr <cortec::implantapi::CImplantInfo> theImplantInfo;                  // Cached Implant Info
        std::unique_ptr <BICListener> listener;

        // Stream Mutexs
        std::mutex tempStreamLock;
        std::mutex humidStreamLock;
        std::mutex neuralStreamLock;
        std::mutex connectionStreamLock;
        std::mutex errorStreamLock;
        std::mutex powerStreamLock;

        // Stream notify variables
        std::condition_variable tempStreamNotify;
        std::condition_variable humidStreamNotify;
        std::condition_variable neuralStreamNotify;
        std::condition_variable connectionStreamNotify;
        std::condition_variable errorStreamNotify;
        std::condition_variable powerStreamNotify;
    };
}