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

namespace ImpedanceCheckApp
{
    class ImpedanceBICManager : IDisposable
    {
        // Private class objects
        private Channel aGRPChannel;
        private BICBridgeService.BICBridgeServiceClient bridgeClient;
        private BICDeviceService.BICDeviceServiceClient deviceClient;
        private BICInfoService.BICInfoServiceClient infoClient;
        private string DeviceName;
        private const int numSensingChannelsDef = 32;
        private List<string> impBuffer;

        // Logging Objects
        FileStream impedFileStream;
        StreamWriter impedFileWriter;
        string impedFilePath;

        // Constructor
        public ImpedanceBICManager(int definedDataBufferLength)
        {
            // Open up the GRPC Channel to the BIC microservice
            aGRPChannel = new Channel("127.0.0.1:50051", ChannelCredentials.Insecure);
        }
        public bool BICConnect()
        {
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

            // Success, return true
            return true;
        }

        public void performImpCheck()
        {
            // Logging impedance items
            impedFilePath = "./impedances" + DateTime.Now.ToString("_MMddyyyy_HHmmss") + ".csv";
            impedFileStream = new FileStream(impedFilePath, FileMode.Create, FileAccess.Write, FileShare.None, 4096, FileOptions.Asynchronous);
            impedFileWriter = new StreamWriter(impedFileStream);

            // Get timestamp
            string timestamp = DateTime.Now.ToString("hh:mm:ss tt");

            // Check impedances and log them
            impBuffer = new List<string>();
            string impedEntry = "";
            for (uint channelNum = 0; channelNum < numSensingChannelsDef; channelNum++)
            {
                impedEntry = "CH" + (channelNum + 1).ToString();
                bicGetImpedanceReply chanImpedValue = deviceClient.bicGetImpedance(new bicGetImpedanceRequest() { DeviceAddress = DeviceName, Channel = channelNum });
                if (chanImpedValue.Success == "success")
                {
                    impBuffer.Add(chanImpedValue.ChannelImpedance.ToString() + chanImpedValue.Units);
                }
                else
                {
                    impBuffer.Add("Unsuccessful impedance reading");
                }
                impedEntry += ", " + impBuffer.Last();
                impedFileWriter.WriteLine(impedEntry);
            }

            // Close impedance logging items
            impedFileWriter.Flush();
            impedFileWriter.Dispose();
            impedFileStream.Dispose();
        }

        public void Dispose()
        {
            // Tell BIC that we want to close!
            Console.WriteLine("Disposing...");
            var disposeReply = deviceClient.bicDispose(new RequestDeviceAddress() { DeviceAddress = DeviceName });
            deviceClient = null;
            Console.WriteLine("Dispose BIC Response: " + disposeReply.ToString());
            aGRPChannel.ShutdownAsync().Wait();
        }  

        public List<string> getImpedances()
        {
            return impBuffer;
        }
    }
}
