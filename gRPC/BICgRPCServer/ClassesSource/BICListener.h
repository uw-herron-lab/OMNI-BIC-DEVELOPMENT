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
        // ************************* Public Sensing Management **********************
        void enableNeuralStreaming(bool enableSensing, uint32_t dataBufferSize, uint32_t interplationThreshold, grpc::ServerWriter<BICgRPC::NeuralUpdate>* aWriter);
        void enableTemperatureStreaming(bool enableSensing, grpc::ServerWriter<BICgRPC::TemperatureUpdate>* aWriter);
        void enableHumidityeStreaming(bool enableSensing, grpc::ServerWriter<BICgRPC::HumidityUpdate>* aWriter);
        void enableConnectionStreaming(bool enableSensing, grpc::ServerWriter<BICgRPC::ConnectionUpdate>* aWriter);
        void enableErrorStreaming(bool enableSensing, grpc::ServerWriter<BICgRPC::ErrorUpdate>* aWriter);
        void enablePowerStreaming(bool enableSensing, grpc::ServerWriter<BICgRPC::PowerUpdate>* aWriter);

        // ************************* Public Research Stim *************************
        void enableClosedLoopStim(bool enableClosedLoop);
        void addImplantPointer(cortec::implantapi::IImplant* theImplantedDevice);

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
        bool neuralStreamingState = false;
        bool temperatureStreamingState = false;
        bool humidityStreamingState = false;
        bool connectionStreamingState = false;
        bool errorStreamingState = false;
        bool powerStreamingState = false;
       
    private:
        // ************************* Private Stream Functions *************************
        void grpcNeuralStreamThread(void);
        void grpcTemperatureStreamThread(void);
        void grpcHumidityStreamThread(void);
        void grpcConnectionStreamThread(void);
        void grpcPowerStreamThread(void);
        void grpcErrorStreamThread(void);

        // ************************* Private Research Stim *************************
        void phaseTriggeredSendStimThread(void);
        double filterIIR(double currSamp, double b[], double a[]);
        bool isZeroCrossing(std::vector<double> filtData);

        // ************************* Private State Objects *************************
        // Generic state variables.
        std::mutex m_mutex;                     // General purpose mutex used for protecting against multi-threaded state access.
        std::mutex m_neuroBufferLock;           // General purpose mutex for protecting the neurobuffer against incomplete read/writes
        bool m_isStimulating;                   // State variable indicating latest stimulation state received from device.
        bool m_isMeasuring;                     // State variable indicating latest measurement state received from device.
        bool isCLStimEn = false;
        cortec::implantapi::IImplant* theImplantedDevice;

        // ************************* Private Neural Streaming Objects *************************
        // Neural streaming requires additional state variables because of data buffering and interpolation functionality.
        // Other streams do not have data buffering and interpolation functionality.
        int neuroDataBufferThreshold;      // Neural data tranmission buffer size, provided to Listener using enableNeuralSensing()
        uint32_t neuroInterplationThreshold;    // Neural interpolation length limit, provided to Listener using enableNeuralSensing()
        uint32_t lastNeuroCount = 0;            // Used to determine the number of samples required for interpolation
        double latestData[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        std::vector<double> filtData = { 0,0 };
        std::vector<double> prevData = { 0,0 };

        // ************************* Private Stream Coordination Objects *************************
        // Pointers for gRPC-managed streaming interfaces. Set by the BICDeviceServiceImpl class, null when not in use.
        grpc::ServerWriter<BICgRPC::NeuralUpdate>* neuralWriter;
        grpc::ServerWriter<BICgRPC::TemperatureUpdate>* temperatureWriter = NULL;
        grpc::ServerWriter<BICgRPC::HumidityUpdate>* humidityWriter = NULL;
        grpc::ServerWriter<BICgRPC::ConnectionUpdate>* connectionWriter = NULL;
        grpc::ServerWriter<BICgRPC::ErrorUpdate>* errorWriter = NULL;
        grpc::ServerWriter<BICgRPC::PowerUpdate>* powerWriter = NULL;

        //  Streaming Data Queues 
        std::queue<BICgRPC::NeuralSample*> neuralSampleQueue;
        // std::queue<BICgRPC::NeuralSample*> filtNeuralSampleQueue;
        std::queue<BICgRPC::TemperatureUpdate*> temperatureSampleQueue;
        std::queue<BICgRPC::HumidityUpdate*> humiditySampleQueue;
        std::queue<BICgRPC::ConnectionUpdate*> connectionSampleQueue;
        std::queue<BICgRPC::ErrorUpdate*> errorSampleQueue;
        std::queue<BICgRPC::PowerUpdate*> powerSampleQueue;

        // Stream processing threads
        std::thread* neuralProcessingThread;
        std::thread* temperatureProcessingThread;
        std::thread* humidityProcessingThread;
        std::thread* connectionProcessingThread;
        std::thread* errorProcessingThread;
        std::thread* powerProcessingThread;
        // for beta- based stim
        std::thread* betaStimThread;

        // Stream data ready signals
        std::condition_variable* neuralDataNotify;
        std::condition_variable* temperatureDataNotify;
        std::condition_variable* humidityDataNotify;
        std::condition_variable* connectionDataNotify;
        std::condition_variable* errorDataNotify;
        std::condition_variable* powerDataNotify;
        std::condition_variable* stimTrigger;
    };
}
