using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.Diagnostics;
using System.IO;
using Grpc.Core;
using BICgRPC;

namespace RealtimeGraphing
{
    class BICManager : IDisposable
    {
        // Private class objects
        private Channel aGRPChannel;
        private BICBridgeService.BICBridgeServiceClient bridgeClient;
        private BICDeviceService.BICDeviceServiceClient deviceClient;
        private BICInfoService.BICInfoServiceClient infoClient;
        private string DeviceName;
        private List<double>[] dataBuffer;
        private List<double>[] filtDataBuffer;
        private const int numSensingChannelsDef = 32;
        private object dataBufferLock = new object();

        // Logging Objects
        FileStream logFileStream;
        StreamWriter logFileWriter;
        string filePath = "./filterLog.csv";
        ConcurrentQueue<string> logLineQueue = new ConcurrentQueue<string>();
        Thread newLoggingThread;
        bool loggingNotDisposed = true;

        // Public Class Properties
        public int DataBufferMaxSampleNumber { get; set; }
        
        // Task pointers for streaming methods
        private Task neuroMonitor = null;

        // Constructor
        public BICManager(int definedDataBufferLength) 
        {
            // Open up the GRPC Channel to the BIC microservice
            aGRPChannel = new Channel("127.0.0.1:50051", ChannelCredentials.Insecure);

            // Set up variables for visualization: instantiate data buffers and length variables.
            DataBufferMaxSampleNumber = definedDataBufferLength;
            dataBuffer = new List<double>[numSensingChannelsDef];
            filtDataBuffer = new List<double>[1];
            for(int i = 0; i < numSensingChannelsDef; i++)
            {
                dataBuffer[i] = new List<double>();
            }
            filtDataBuffer[0] = new List<double>();

            // Set up the logging interface
            if(File.Exists(filePath))
            {
                File.Delete(filePath);
            }
            logFileStream = new FileStream("./filterLog.csv", FileMode.Create, FileAccess.Write, FileShare.None, 4096, FileOptions.Asynchronous);
            logFileWriter = new StreamWriter(logFileStream);
            logFileWriter.WriteLine("PacketNum, TimeStamp, FilteredChannelNum, RawChannelData, FilteredChannelData, boolInterpolated, StimChannelData, StimActive, CalcPhase");
        }
        public bool BICConnect()
        {
//#warning TODO: Add a check for already connected devices?

//#warning TODO: Add InfoService- prompted by "?"
            infoClient = new BICInfoService.BICInfoServiceClient(aGRPChannel);
            Console.WriteLine("Grabbing information: ");
            var versionNumberReply = infoClient.VersionNumber(new VersionNumberRequest());
            var supportedDevicesReply = infoClient.SupportedDevices(new SupportedDevicesRequest());
            var inspectRepositoryReply = infoClient.InspectRepository(new InspectRepositoryRequest());
            Console.WriteLine("\tVersion Number: " + versionNumberReply.VersionNumber);
            Console.WriteLine("\tSupported Devices: " + supportedDevicesReply.SupportedDevices);
            Console.WriteLine("\tRepository: " + inspectRepositoryReply.RepoUri);

            // Instantiate the Bridge and scan for bridges
            Console.WriteLine("Instantiating Bridge Service Client and scanning for bridges...");
            bridgeClient = new BICBridgeService.BICBridgeServiceClient(aGRPChannel);
            var connectedBridgesReply = bridgeClient.ConnectedBridges(new QueryBridgesRequest());
            var scanBridgesReply = bridgeClient.ScanBridges(new QueryBridgesRequest());
            if (scanBridgesReply.Bridges.Count == 0)
            {
                Console.WriteLine("No bridges found. BICConnect returns false.");
                return false;
            }

            // Print out all Bridges Found
            Console.WriteLine("Bridges Found:");
            for (int i = 0; i < scanBridgesReply.Bridges.Count; i++)
            {
                Console.WriteLine("Bridge Index: " + i.ToString());
                Console.WriteLine("\tBridge.Address: " + scanBridgesReply.Bridges[i].Name);
                Console.WriteLine("\tBridge.DeviceId: " + scanBridgesReply.Bridges[i].DeviceId);
                Console.WriteLine("\tBridge.FirmwareVersion: " + scanBridgesReply.Bridges[i].FirmwareVersion);
                Console.WriteLine("\tBridge.DeviceType: " + scanBridgesReply.Bridges[i].DeviceType);
                Console.WriteLine();
            }

            // Connect to the first bridge
            Console.WriteLine("Connecting to the first bridge");
            string bridgeName = scanBridgesReply.Bridges[0].Name;
            var connectBridgeReply = bridgeClient.ConnectBridge(new ConnectBridgeRequest() { Name = bridgeName });
            Console.WriteLine("Connect Bridge Response: " + connectBridgeReply.ConnectionStatus);
            Console.WriteLine();

            // Scan for Implantable Devices using the chosen bridge
            Console.WriteLine("Instantiating Bridge Service Client and scanning for bridges...");
            deviceClient = new BICDeviceService.BICDeviceServiceClient(aGRPChannel);
            var scanDevicesReply = deviceClient.ScanDevices(new ScanDevicesRequest() { BridgeName = bridgeName });

            // Ensure a device was found
            if (scanDevicesReply.Name == "")
            {
                Console.WriteLine("No devices found. BICConnect returns false.");
                return false;
            }
            DeviceName = scanDevicesReply.Name;

            // Print out all devices found
            Console.WriteLine("Device Found:");
            Console.WriteLine("\tDevice.Address: " + scanDevicesReply.Name);
            Console.WriteLine("\tDevice.Id: " + scanDevicesReply.DiscoveredDevice.DeviceId.ToString());
            Console.WriteLine("\tDevice.Type: " + scanDevicesReply.DiscoveredDevice.DeviceType.ToString());
            Console.WriteLine("\tDevice.FirmwareVersion: " + scanDevicesReply.DiscoveredDevice.FirmwareVersion.ToString());
            Console.WriteLine("\tDevice.ChannelCount: " + scanDevicesReply.DiscoveredDevice.ChannelCount.ToString());
            Console.WriteLine("\tDevice.MeasurementChannelCount: " + scanDevicesReply.DiscoveredDevice.MeasurementChannelCount.ToString());
            Console.WriteLine("\tDevice.StimulationChannelCount: " + scanDevicesReply.DiscoveredDevice.StimulationChannelCount.ToString());
            Console.WriteLine("\tDevice.SamplingRate: " + scanDevicesReply.DiscoveredDevice.SamplingRate.ToString());
            Console.WriteLine();

            // Connect to the device
            Console.WriteLine("Connecting to implantable device.");
            var connectDeviceReply = deviceClient.ConnectDevice(new ConnectDeviceRequest() { DeviceAddress = DeviceName, LogFileName = "./deviceLog.txt" });

            // Start up the neural stream
            neuroMonitor = Task.Run(neuralMonitorTaskAsync);

            // Success, return true
            return true;
        }

        public void Dispose()
        {
            // Tell BIC that we want to close!
            deviceClient.bicNeuralStream(new bicNeuralSetStreamingEnable() { DeviceAddress = DeviceName, Enable = false });
            var disposeReply = deviceClient.bicDispose(new RequestDeviceAddress() { DeviceAddress = DeviceName });
            Console.WriteLine("Dispose BIC Response: " + disposeReply.ToString());
            aGRPChannel.ShutdownAsync().Wait();

            // Tell logging we want to close
            loggingNotDisposed = false;
            logFileWriter.Flush();
            logFileWriter.Dispose();
            logFileStream.Dispose();
        }

        public void enableOpenLoopStimulation(bool openStimEn, bool monopolar, uint stimChannel, uint returnChannel, double stimAmplitude, uint stimDuration, uint chargeBalancePWRatio, uint interPulseInterval, double stimThreshold)
        {
            if (openStimEn)
            {
                // Create a waveform defintion request 
                bicEnqueueStimulationRequest aNewWaveformRequest = new bicEnqueueStimulationRequest() { DeviceAddress = DeviceName, Mode = EnqueueStimulationMode.PersistentWaveform, WaveformRepititions = 255 };

                // check if interPulseInterval is greater than 20400 us (DZ1 duration limit) to determine how to add inter pulse interval
                if (interPulseInterval <= 20400)
                {
                    // Create a pulse function
                    StimulationFunctionDefinition pulseFunction0 = new StimulationFunctionDefinition()
                    {
                        FunctionName = "openLoopPulse",
                        StimPulse = new stimPulseFunction() { Amplitude = { stimAmplitude, 0, 0, 0 }, DZ0Duration = 10, DZ1Duration = interPulseInterval, PulseWidth = stimDuration, PulseRepetitions = 1, SourceElectrodes = { stimChannel }, SinkElectrodes = { returnChannel }, UseGround = monopolar, BurstRepetitions = 1 }

                    };
                    aNewWaveformRequest.Functions.Add(pulseFunction0);
                }
                else
                {
                    // Create a pulse function
                    StimulationFunctionDefinition pulseFunction0 = new StimulationFunctionDefinition()
                    {
                        FunctionName = "openLoopPulse",
                        StimPulse = new stimPulseFunction() { Amplitude = { stimAmplitude, 0, 0, 0 }, DZ0Duration = 10, DZ1Duration = 10, PulseWidth = stimDuration, PulseRepetitions = 1, SourceElectrodes = { stimChannel }, SinkElectrodes = { returnChannel }, UseGround = monopolar, BurstRepetitions = 1 }

                    };
                    aNewWaveformRequest.Functions.Add(pulseFunction0);

                    // Create the interpulse pause
                    StimulationFunctionDefinition interpulsePause = new StimulationFunctionDefinition()
                    {
                        FunctionName = "pausePulse",
                        Pause = new pauseFunction() { Duration = interPulseInterval }
                    };
                    aNewWaveformRequest.Functions.Add(interpulsePause);
                }

                // Enqueue the stimulation waveform
                deviceClient.bicEnqueueStimulation(aNewWaveformRequest);
                deviceClient.enableOpenLoopStimulation(new openLoopStimEnableRequest() { DeviceAddress = DeviceName, Enable = true, WatchdogInterval = 5000, TriggerStimThreshold = stimThreshold });
            }
            else
            {
                // Stop the stim
                deviceClient.enableOpenLoopStimulation(new openLoopStimEnableRequest() { DeviceAddress = DeviceName, Enable = false});
            }
        }

        public void enableDistributedStim(bool closedStimEn, uint stimChannel, uint senseChannel, double stimAmplitude, uint stimDuration, uint chargeBalancePWRatio, List<double> filterCoefficients_B, List<double> filterCoefficients_A, double stimThreshold, double initTriggerPhase)
        {
            if (closedStimEn)
            {
                // Create a waveform defintion request 
                bicEnqueueStimulationRequest aNewWaveformRequest = new bicEnqueueStimulationRequest() { DeviceAddress = DeviceName, Mode = EnqueueStimulationMode.PersistentWaveform, WaveformRepititions = 1 };
                // Create a pulse function
                StimulationFunctionDefinition pulseFunction0 = new StimulationFunctionDefinition()
                {
                    FunctionName = "betaPulseFunction",
                    StimPulse = new stimPulseFunction() { Amplitude = { stimAmplitude, 0, 0, 0 }, DZ0Duration = 10, DZ1Duration = 10, PulseWidth = stimDuration, PulseRepetitions = 1, SourceElectrodes = { stimChannel }, SinkElectrodes = { }, UseGround = true, BurstRepetitions = 1 }
                };
                aNewWaveformRequest.Functions.Add(pulseFunction0);

                // Enqueue the stimulation waveform
                deviceClient.bicEnqueueStimulation(aNewWaveformRequest);
            }

            // Start the distributed stimulation function
            deviceClient.enableDistributedStimulation(new distributedStimEnableRequest() { DeviceAddress = DeviceName, Enable = closedStimEn, SensingChannel = senseChannel, 
                FilterCoefficientsB = { filterCoefficients_B }, FilterCoefficientsA = { filterCoefficients_A }, TriggerStimThreshold = stimThreshold, InitTriggerStimPhase = initTriggerPhase}); 
        }

        /// <summary>
        /// Provide a copy of the current data buffers
        /// </summary>
        /// <returns>A list of double-arrays, each array is composed of the latest time domain data from each BIC channel. index 0 is the oldest data. </returns>
        public List<double>[] getData() // need to modify this section in order to get filtered data
        {
            List<double>[] outputBuffer = new List<double>[dataBuffer.Length + filtDataBuffer.Length];

            lock(dataBufferLock)
            {
                for (int i = 0; i < dataBuffer.Length; i++)
                {
                    outputBuffer[i] = new List<double>(dataBuffer[i]);
                }
                for (int j = 0; j < filtDataBuffer.Length; j++)
                {
                    outputBuffer[dataBuffer.Length + j] = new List<double>(filtDataBuffer[j]);
                }

            }
            // have something similar for filtered data buffer

            return outputBuffer;
        }

        private void loggingThread()
        {
            // Declare output from concurrent queue
            string dequeuedString;

            // Loop until quit
            while(loggingNotDisposed)
            {
                try
                {
                    if (logLineQueue.TryDequeue(out dequeuedString))
                    {
                        logFileWriter.WriteLine(dequeuedString);
                    }
                    else
                    {
                        Thread.Sleep(50);
                    }
                }
                catch (Exception e)
                {
                    Console.WriteLine("logging exception occured: " + e.Message);
                }

            }
        }

        private bool validBufferOrderCheck(Google.Protobuf.Collections.RepeatedField<NeuralSample> sampleBuffer)
        {
            uint prevPacketNum = sampleBuffer[0].SampleCounter;
            for (int i = 1; i < sampleBuffer.Count; i++)
            {
                // check if the sequential packet number is less than the previous packet number
                if (sampleBuffer[i].SampleCounter < prevPacketNum + 1) 
                {
                    Console.WriteLine("ERROR: Packet numbers are not in order! Sample (prev, next): " + prevPacketNum.ToString() + ", " + sampleBuffer[i].SampleCounter.ToString());
                    return false;
                }
                // update the prevPacketNum value
                prevPacketNum = sampleBuffer[i].SampleCounter;
            }
            return true;
        }

        /// <summary>
        /// Monitor the neural stream for new data
        /// </summary>
        /// <returns>Task completion information</returns>
        async Task neuralMonitorTaskAsync()
        {

            var stream = deviceClient.bicNeuralStream(new bicNeuralSetStreamingEnable() { DeviceAddress = DeviceName, Enable = true, BufferSize = 100, MaxInterpolationPoints = 10, AmplificationFactor = RecordingAmplificationFactor.Amplification395DB, RefChannels = { 31 }, UseGroundReference = true });

            // Create performance-tracking interpacket variables
            Stopwatch aStopwatch = new Stopwatch();
            newLoggingThread = new Thread(loggingThread);
            newLoggingThread.Start();
            
            uint latestPacketNum = 0;
            while (await stream.ResponseStream.MoveNext())
            {
                // Check if buffer has out of order packet numbers
                validBufferOrderCheck(stream.ResponseStream.Current.Samples);

                // Missing packet handling
                if (stream.ResponseStream.Current.Samples[0].SampleCounter != (latestPacketNum + 1) )
                {
                    // Determine the number of packets missing
                    long diffPackets = stream.ResponseStream.Current.Samples[0].SampleCounter - (latestPacketNum + 1);
                    
                    // Account for uint wrap around (RARE)
                    if(diffPackets < 0)
                    {
                        diffPackets = uint.MaxValue - (latestPacketNum + 1) + stream.ResponseStream.Current.Samples[0].SampleCounter;
                    }

                    // Insert a maximum of 10 NANs
                    if(diffPackets > 10)
                    {
                        diffPackets = 10;
                    }

                    // Create a NAN buffer to append into the dataBuffer
                    double[] nanBuffer = new double[diffPackets];

                    // Fill the buffer with NANs
                    for (int i = 0; i < diffPackets; i++)
                    {
                        nanBuffer[i] = double.NaN;
                    }
                    
                    // Lock dataBuffer access to ensure no mid-update copy.
                    lock (dataBufferLock)
                    {
                        // Update the data buffers with NAN points
                        for (int channelNum = 0; channelNum < dataBuffer.Length; channelNum++)
                        {
                            dataBuffer[channelNum].AddRange(nanBuffer);
                        }

                        // do the same for the filtered data buffer
                        filtDataBuffer[0].AddRange(nanBuffer);
                    }
                }

                // Status update to console
                aStopwatch.Restart();

                // Create local copy buffers and loop variables
                int numSamples = stream.ResponseStream.Current.Samples.Count;
                double[] copyBuffer = new double[numSamples];
                double[] filtBuffer = new double [numSamples];

                // Lock dataBuffer access to ensure no mid-update copy.
                lock (dataBufferLock)
                {
                    // Update the data buffers
                    for (int channelNum = 0; channelNum < dataBuffer.Length; channelNum++)
                    {
                        // Assemble the array of new data for an individual channel
                        for (int sampleNum = 0; sampleNum < numSamples; sampleNum++)
                        {
                            copyBuffer[sampleNum] = stream.ResponseStream.Current.Samples[sampleNum].Measurements[channelNum];
                        }

                        // Add the new data to the channel data buffer
                        dataBuffer[channelNum].AddRange(copyBuffer);

                        // Check if the channel data buffer is now too long
                        int diffLength = dataBuffer[channelNum].Count - DataBufferMaxSampleNumber;
                        if (diffLength > 0)
                        {
                            // Too long, remove the difference
                            dataBuffer[channelNum].RemoveRange(0, diffLength);
                        }
                    }

                    // Update the filter data buffer
                    for (int sampleNum = 0; sampleNum < numSamples; sampleNum++)
                    {
                        filtBuffer[sampleNum] = stream.ResponseStream.Current.Samples[sampleNum].FiltSample;

                        // Log the latest info out to the CSV
                        int filteredIndex = (int)stream.ResponseStream.Current.Samples[sampleNum].FiltChannel;

                        // Enqueue the data for the logging thread
                        string logString = stream.ResponseStream.Current.Samples[sampleNum].SampleCounter.ToString() + ", " +
                            stream.ResponseStream.Current.Samples[sampleNum].TimeStamp.ToString() + ", " + 
                            filteredIndex.ToString() + ", " +
                            stream.ResponseStream.Current.Samples[sampleNum].Measurements[filteredIndex].ToString() + ", " +
                            stream.ResponseStream.Current.Samples[sampleNum].FiltSample.ToString() + ", " +
                            stream.ResponseStream.Current.Samples[sampleNum].IsInterpolated.ToString() + ", " +
                            stream.ResponseStream.Current.Samples[sampleNum].Measurements[5].ToString() + ", " +
                            stream.ResponseStream.Current.Samples[sampleNum].StimulationActive.ToString() + ", " + 
                            stream.ResponseStream.Current.Samples[sampleNum].Phase.ToString();
                        logLineQueue.Enqueue(logString);
                    }
                    // Add new data to filtered data buffer
                    filtDataBuffer[0].AddRange(filtBuffer);

                    // Check if filtered data buffer is too long
                    int filtDiffLength = filtDataBuffer[0].Count - DataBufferMaxSampleNumber;
                    if (filtDiffLength > 0)
                    {
                        // if too long, remove the difference
                        filtDataBuffer[0].RemoveRange(0, filtDiffLength);
                    }
                }

                // Update packet tracking number
                latestPacketNum = stream.ResponseStream.Current.Samples[numSamples - 1].SampleCounter;

                // Output status
                aStopwatch.Stop();
                double elapsedTime = (double)aStopwatch.ElapsedTicks / (double)Stopwatch.Frequency;
                Console.WriteLine("Implant Stream Neural Samples Received: " + stream.ResponseStream.Current.Samples.Count + "copyTime: " + elapsedTime.ToString());
            }
            Console.WriteLine("(Neural Monitor Task Exited)");
        }
    }
}
