﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;

using Grpc.Core;
using BICgRPC;

namespace RealtimeGraphing
{
    class BICManager : IDisposable
    {
        private Channel aGRPChannel;
        private BICBridgeService.BICBridgeServiceClient bridgeClient;
        private BICDeviceService.BICDeviceServiceClient deviceClient;
        private string DeviceName;
        private List<double>[] dataBuffer;
        private const int numSensingChannelsDef = 32;
        public int DataBufferMaxSampleNumber { get; set; }
        private object dataBufferLock = new object();


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
            for(int i = 0; i < numSensingChannelsDef; i++)
            {
                dataBuffer[i] = new List<double>();
            }
        }

        public bool BICConnect()
        {
            // Instantiate the Bridge and scan for bridges
            Console.WriteLine("Instantiating Bridge Service Client and scanning for bridges...");
            bridgeClient = new BICBridgeService.BICBridgeServiceClient(aGRPChannel);
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
                Console.WriteLine("\tBridge.ImplantType: " + scanBridgesReply.Bridges[i].ImplantType);
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
        }

        /// <summary>
        /// Provide a copy of the current data buffers
        /// </summary>
        /// <returns>A list of double-arrays, each array is composed of the latest time domain data from each BIC channel. index 0 is the oldest data. </returns>
        public List<double>[] getData()
        {
            List<double>[] outputBuffer = new List<double>[dataBuffer.Length];

            lock(dataBufferLock)
            {
                for(int i = 0; i < dataBuffer.Length; i++)
                {
                    outputBuffer[i] = new List<double>(dataBuffer[i]);
                }
            }

            return outputBuffer;
        }

        /// <summary>
        /// Monitor the neural stream for new data
        /// </summary>
        /// <returns>Task completion information</returns>
        async Task neuralMonitorTaskAsync()
        {
            var stream = deviceClient.bicNeuralStream(new bicNeuralSetStreamingEnable() { DeviceAddress = DeviceName, Enable = true, BufferSize = 100 });
            
            // Create performance-tracking interpacket variables
            Stopwatch aStopwatch = new Stopwatch();
            uint latestPacketNum = 0;
            
            while (await stream.ResponseStream.MoveNext())
            {
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
                    }
                }

                // Status update to console
                aStopwatch.Restart();

                // Create local copy buffers and loop variables
                int numSamples = stream.ResponseStream.Current.Samples.Count;
                double[] copyBuffer = new double[numSamples];

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
