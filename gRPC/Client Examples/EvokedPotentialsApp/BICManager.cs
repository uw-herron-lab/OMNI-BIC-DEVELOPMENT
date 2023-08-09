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
// D:\gitbuilds\OMNI - BIC - DEVELOPMENT\gRPC\Client Examples\EvokedPotentialsApp\BICManager.cs
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
        private List<double>[] runningTotals;
        private List<double>[] currPulseBuffer;
        public int currNumPulses { get; set; } = 0;
        private const int numSensingChannelsDef = 32;
        private object dataBufferLock = new object();
        //private int chartWidth;
        private uint stimPeriodSamples;
        private uint stimPeriod; // uS
        private uint baselinePeriodSamples;
        private uint samplingRate = 1000; // Hz

        // Logging Objects
        FileStream logFileStream;
        StreamWriter logFileWriter;
        string filePath = "./filterLog" + DateTime.Now.ToString("_MMMdyyyy_HHmmss") + ".csv";
        ConcurrentQueue<string> logLineQueue = new ConcurrentQueue<string>();
        Thread newLoggingThread;
        bool loggingNotDisposed = true;

        // Public Class Properties
        public int DataBufferMaxSampleNumber { get; set; }
        public int deviceSampleRate {  get; set; }
        public bool experimentRunning { get; set; }
        
        // Task pointers for streaming methods
        private Task neuroMonitor = null;

        // Random class used for jitter
        private static readonly Random random = new Random();
        private static readonly object syncLock = new object();
        public static int RandomNumber(int min, int max)
        {
            lock (syncLock)
            {
                return random.Next(min, max);
            }
        }

        // Constructor
        public BICManager(int definedDataBufferLength, uint stimPeriod, uint baselinePeriod) 
        {
            // Open up the GRPC Channel to the BIC microservice
            aGRPChannel = new Channel("127.0.0.1:50051", ChannelCredentials.Insecure);

            // Set up variables for visualization: instantiate data buffers and length variables.
            DataBufferMaxSampleNumber = definedDataBufferLength;
            //this.chartWidth = neuroStreamChartWidth;
            this.stimPeriod = stimPeriod;
            this.stimPeriodSamples = (uint)(stimPeriod / 1e6 * samplingRate);
            this.baselinePeriodSamples = (uint)(baselinePeriod / 1e6 * samplingRate);
            dataBuffer = new List<double>[numSensingChannelsDef];
            currPulseBuffer = new List<double>[numSensingChannelsDef];
            runningTotals = new List<double>[numSensingChannelsDef];
            filtDataBuffer = new List<double>[1];
            for(int i = 0; i < numSensingChannelsDef; i++)
            {
                dataBuffer[i] = new List<double>();
                currPulseBuffer[i] = new List<double>();
                runningTotals[i] = new List<double>(new double[stimPeriodSamples]);
            }
            filtDataBuffer[0] = new List<double>();

            // Set up the logging interface
            if(File.Exists(filePath))
            {
                File.Delete(filePath);
            }
            logFileStream = new FileStream(filePath, FileMode.Create, FileAccess.Write, FileShare.None, 4096, FileOptions.Asynchronous);
            logFileWriter = new StreamWriter(logFileStream);
            string chanHeader = "";
            for (int chNum = 0; chNum < numSensingChannelsDef; chNum++)
            {
                chanHeader += ", CH" + (chNum + 1).ToString();
            }
            // list of header names
            logFileWriter.WriteLine("PacketNum, TimeStamp, FilteredChannelNum, RawChannelData, FilteredChannelData, boolInterpolated, StimChannelData, StimActive" + chanHeader);
        }
        public void zeroRunningTotals()
        {
            for (int chan = 0; chan < numSensingChannelsDef; chan++)
            {
                runningTotals[chan] = new List<double>(new double[stimPeriodSamples]); ;
            }
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

        public void enableStimulationPulse(bool monopolar, uint stimChannel, uint returnChannel, double stimAmplitude, uint stimDuration, uint chargeBalancePWRatio, uint interPulseInterval, double stimThreshold)
        {
            bicEnqueueStimulationRequest aNewWaveformRequest = new bicEnqueueStimulationRequest() { DeviceAddress = DeviceName, Mode = EnqueueStimulationMode.PersistentWaveform, WaveformRepititions = 1 }; // what are waveform reps?? burst? changed from 255 to numPulses...
            if (monopolar)
            {
                // Create a pulse function for monopolar
                StimulationFunctionDefinition pulseFunction0 = new StimulationFunctionDefinition()
                {
                    FunctionName = "evokedPotentialStim",
                    StimPulse = new stimPulseFunction() { Amplitude = { stimAmplitude, 0, 0, 0 }, DZ0Duration = 10, DZ1Duration = 10, PulseWidth = stimDuration, PulseRepetitions = 1, SourceElectrodes = { stimChannel }, SinkElectrodes = { }, UseGround = true, BurstRepetitions = 1 }

                };
                aNewWaveformRequest.Functions.Add(pulseFunction0);
            }
            else
            {
                // Create a pulse function for bipolar stimulation
                StimulationFunctionDefinition pulseFunction0 = new StimulationFunctionDefinition()
                {
                    FunctionName = "evokedPotentialStim",
                    StimPulse = new stimPulseFunction() { Amplitude = { stimAmplitude, 0, 0, 0 }, DZ0Duration = 10, DZ1Duration = 10, PulseWidth = stimDuration, PulseRepetitions = 1, SourceElectrodes = { stimChannel }, SinkElectrodes = { returnChannel }, UseGround = false, BurstRepetitions = 1 }

                };
                aNewWaveformRequest.Functions.Add(pulseFunction0);
            }
            deviceClient.bicEnqueueStimulation(aNewWaveformRequest);
            deviceClient.bicStartStimulation(new bicStartStimulationRequest() { DeviceAddress = DeviceName });
        }

        public void stopEvokedPotentialStimulation()
        {
            deviceClient.bicStopStimulation(new RequestDeviceAddress() { DeviceAddress = DeviceName });
            experimentRunning = false;
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

        public List<double>[] getAvgsData() // need to modify this section in order to get filtered data
        {
            List<double>[] outputBuffer = new List<double>[runningTotals.Length];

            lock (dataBufferLock)
            {
                for (int i = 0; i < runningTotals.Length; i++)
                {
                    double[] chanBuffer = new double[stimPeriodSamples];
                    for (int j = 0; j < stimPeriodSamples; j++)
                    {
                        chanBuffer[j] = runningTotals[i][j] / currNumPulses;
                    }
                    outputBuffer[i] = new List<double>(chanBuffer);
                }
            }
            return outputBuffer;

            //lock (dataBufferLock)
            //{
            //    for (int i = 0; i < runningTotals.Length; i++)
            //    {
            //        double[] chanBuffer = new double[stimPeriodSamples];
            //        for (int j = 0; j < currPulseBuffer[0].Count; j ++)
            //        {
            //            chanBuffer[j] = currPulseBuffer[i][j];
            //        }
            //        int n = currPulseBuffer[0].Count;
            //        while (n < stimPeriodSamples)
            //        {
            //            chanBuffer[n] = 0;
            //        }
            //        outputBuffer[i] = new List<double>(chanBuffer);
            //    }
            //}
            //return outputBuffer;
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

        private int getStimulationActiveIndex(Google.Protobuf.Collections.RepeatedField<NeuralSample> samples)
        {
            for (int i = 0; i < samples.Count; i++)
            {
                if (samples[i].StimulationActive)
                {
                    return i;
                }
            }
            return -1;
        }

        private List<double> getFilteredChanBuffer(List<double> buffer)
        {
            List<double> filtChanBuffer = new List<double>(buffer);
            return filtChanBuffer;
        }

        /// <summary>
        /// Monitor the neural stream for new data
        /// </summary>
        /// <returns>Task completion information</returns>
        async Task neuralMonitorTaskAsync()
        {
            Debug.WriteLine("********************************************************* calling monitor task");
            var stream = deviceClient.bicNeuralStream(new bicNeuralSetStreamingEnable() { DeviceAddress = DeviceName, Enable = true, BufferSize = 100, MaxInterpolationPoints = 10, AmplificationFactor = RecordingAmplificationFactor.Amplification395DB, RefChannels = { 31 }, UseGroundReference = true});
        
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
                    if (diffPackets < 0)
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
                            currPulseBuffer[channelNum].AddRange(nanBuffer);
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
                // we also don't want a mid update copy of the running totals. will that be taken care of as well? maybe not relevant if I only tell it to access running totals after they've been updated?
                // do we want this all in the same lock? 
                lock (dataBufferLock)
                {
                    // Update the current pulse buffer
                    // If current pulse buffer is empty, watch for active stim. If found, start the next current pulse buffer
                    //if (experimentRunning && !pulseFound && currPulseBuffer[0].Count == 0)
                    //Debug.WriteLine(getStimulationActiveIndex(stream.ResponseStream.Current.Samples));
                    //Debug.WriteLine(stream.ResponseStream.Current.Samples[0].StimulationActive);
                    int stimulationActiveIndex = getStimulationActiveIndex(stream.ResponseStream.Current.Samples);
                    if (experimentRunning && currPulseBuffer[0].Count == 0)
                    {
                        //int stimulationActiveIndex = getStimulationActiveIndex(stream.ResponseStream.Current.Samples);
                        // if it finds active stim, start adding to buffer
                        if (stimulationActiveIndex != -1)
                        {
                            Debug.WriteLine("******* stim found at" + stimulationActiveIndex);
                            //pulseFound = true;
                            double testElapsedTime = (double)aStopwatch.ElapsedTicks / (double)Stopwatch.Frequency;
                            //Debug.WriteLine(currNumPulses + " pulses, found start stim, " + testElapsedTime);
                            int transferBufferSize = (int)(baselinePeriodSamples - stimulationActiveIndex); // how much data to take from the end of the buffer to get to baselinePeriodSamples before stim
                            int currDataBufferSize = dataBuffer[0].Count;
                            int dataBufferStartI = (int)(currDataBufferSize - transferBufferSize); // starting i in dataBuffer to transfer to currPulseBuffer
                            int pulseBufferCurrSize = (int)(baselinePeriodSamples + (numSamples - stimulationActiveIndex));
                            for (int channelNum = 0; channelNum < dataBuffer.Length; channelNum++)
                            {
                                //currPulseBuffer[channelNum] = new List<double>();
                                // add previous data from buffer to chanCurrPulseBuffer
                                double[] dataBufferTransfer = new double[transferBufferSize];
                                for (int i = dataBufferStartI; i < currDataBufferSize; i++)
                                {
                                    dataBufferTransfer[i - dataBufferStartI] = dataBuffer[channelNum][i];
                                }
                                currPulseBuffer[channelNum].AddRange(dataBufferTransfer);
                                // add new data to chanCurrPulseBuffer
                                double[] streamTransfer = new double[numSamples];
                                for (int sampleNum = 0; sampleNum < numSamples; sampleNum++)
                                {
                                    streamTransfer[sampleNum] = stream.ResponseStream.Current.Samples[sampleNum].Measurements[channelNum];
                                }
                                currPulseBuffer[channelNum].AddRange(streamTransfer);
                            }
                            Debug.WriteLine("currPulseBuffer size: " + currPulseBuffer[0].Count);
                        }
                    }
                    // if current pulse buffer is not empty, add new samples until it's full. once full, filter and send to running totals
                    else if (experimentRunning && currPulseBuffer[0].Count > 0)
                    {
                        for (int channelNum = 0; channelNum < currPulseBuffer.Length; channelNum++)
                        {
                            for (int sampleNum = 0; sampleNum < numSamples; sampleNum++)
                            {
                                copyBuffer[sampleNum] = stream.ResponseStream.Current.Samples[sampleNum].Measurements[channelNum];
                            }
                            // only add enough data to fill currPulseBuffer. if too long, remove
                            if (currPulseBuffer[channelNum].Count > stimPeriodSamples)
                            {
                                int diffLength = (int)(currPulseBuffer[channelNum].Count - stimPeriodSamples);
                                currPulseBuffer[channelNum].RemoveRange((int)stimPeriodSamples, diffLength);
                            }
                            else if (currPulseBuffer[channelNum].Count + copyBuffer.Length >= stimPeriodSamples)
                            {
                                currPulseBuffer[channelNum].AddRange(new ArraySegment<double>(copyBuffer, 0, (int)(stimPeriodSamples - currPulseBuffer[channelNum].Count)));
                            }
                            else
                            {
                                currPulseBuffer[channelNum].AddRange(copyBuffer);
                            }
                        }
                        // if we've filled up the buffer
                        if (currPulseBuffer[0].Count >= stimPeriodSamples) // it should never be greater than! check this?
                        {
                            // increment num pulses
                            currNumPulses++;
                            // filter and add to running totals
                            Debug.WriteLine("Adding to running totals");
                            for (int channelNum = 0; channelNum < currPulseBuffer.Length; channelNum++)
                            {
                                List<double> filtPulseBuffer = getFilteredChanBuffer(currPulseBuffer[channelNum]);
                                currPulseBuffer[channelNum].Clear();
                                for (int sampleNum = 0; sampleNum < stimPeriodSamples; sampleNum++)
                                {
                                    runningTotals[channelNum][sampleNum] += filtPulseBuffer[sampleNum];
                                }
                            }
                            //pulseFound = false;
                        }
                    }
                    

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
                            stream.ResponseStream.Current.Samples[sampleNum].StimulationActive.ToString();
                        for (int chNum = 0; chNum < numSensingChannelsDef; chNum++)
                        {
                            logString += ", " + stream.ResponseStream.Current.Samples[sampleNum].Measurements[chNum].ToString();
                        }
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
                    //Debug.WriteLine("current buffer size: " + currPulseBuffer[0].Count);
                }

                // Update packet tracking number
                latestPacketNum = stream.ResponseStream.Current.Samples[numSamples - 1].SampleCounter;

                // Output status
                aStopwatch.Stop();
                double elapsedTime = (double)aStopwatch.ElapsedTicks / (double)Stopwatch.Frequency;
                //Console.WriteLine("Implant Stream Neural Samples Received: " + stream.ResponseStream.Current.Samples.Count + "copyTime: " + elapsedTime.ToString());
            }
            Console.WriteLine("(Neural Monitor Task Exited)");
        }
    }
}
