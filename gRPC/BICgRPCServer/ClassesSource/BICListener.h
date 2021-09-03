#pragma once
#include <cppapi/bicapi.h>
#include <cppapi/Sample.h>
#include <cppapi/bic3232constants.h>
#include <queue>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "BICgRPC.grpc.pb.h"

namespace BICGRPCHelperNamespace
{
    class BICListener : public cortec::implantapi::IImplantListener
    {
    public:
        // ************************* Sensing Management **********************
        void enableNeuralSensing(bool enableSensing, uint32_t dataBufferSize, uint32_t interplationThreshold, grpc::ServerWriter<BICgRPC::NeuralUpdate>* aWriter);

        // ************************* Public Event Handlers *************************
        void onStimulationStateChanged(const bool isStimulating);
        void onMeasurementStateChanged(const bool isMeasuring);
        void onConnectionStateChanged(const cortec::implantapi::connection_info_t& info);
        void onConnectionStateChanged(const bool isConnected);
        void onData(const std::vector<cortec::implantapi::CSample>* samples);
        void onImplantVoltageChanged(const double voltageMicroV);
        void onPrimaryCoilCurrentChanged(const double currentMilliA);
        void onImplantControlValueChanged(const double controlValue);
        void onTemperatureChanged(const double temperature);
        void onHumidityChanged(const double humidity);
        void onError(const std::exception& err);
        void onDataProcessingTooSlow();
        
        // ************************* Public Boolean State Accessors *************************
        bool isStimulating();
        bool isMeasuring();
        
        // ************************* Public Streaming Objects *******************************
        // Pointers for gRPC-managed streaming interfaces. Set by the BICDeviceServiceImpl class, null when not in use.
        grpc::ServerWriter<BICgRPC::TemperatureUpdate>* TemperatureWriter = NULL;
        grpc::ServerWriter<BICgRPC::HumidityUpdate>* HumidityWriter = NULL;
        grpc::ServerWriter<BICgRPC::ConnectionUpdate>* ConnectionWriter = NULL;
        grpc::ServerWriter<BICgRPC::ErrorUpdate>* ErrorWriter = NULL;
        grpc::ServerWriter<BICgRPC::PowerUpdate>* PowerWriter = NULL;

        // ************************* Public Streaming Parameter Objects *************************
       

    private:
        // ************************* Private Stream State Objects *************************
        void grpcNeuralStreamThread(void);

        // ************************* Private Stream State Objects *************************
        std::mutex m_mutex;
        bool m_isStimulating;
        bool m_isMeasuring;
        uint32_t neuroDataBufferThreshold;
        uint32_t neuroInterplationThreshold;

        // ************************* Private Streaming Queues ***********************************
        grpc::ServerWriter<BICgRPC::NeuralUpdate>* neuralWriter;
        std::queue<BICgRPC::NeuralSample*> neuralSampleQueue;
        std::thread* neuralProcessingThread;
        std::condition_variable* neuroDataNotify;
        bool neuralStreamingState = false;
        uint32_t lastNeuroCount = 0;
        double latestData[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
    };
}
