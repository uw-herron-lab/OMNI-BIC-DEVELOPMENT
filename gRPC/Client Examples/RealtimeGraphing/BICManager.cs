using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

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
        private List<double> dataBuffer;

        // Task pointers for streaming methods
        private Task neuroMonitor = null;

        // Constructor
        public BICManager() 
        {
            aGRPChannel = new Channel("127.0.0.1:50051", ChannelCredentials.Insecure);
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
            deviceClient.bicSetSensingEnable(new bicSetSensingEnableRequest() { DeviceAddress = DeviceName, EnableSensing = true, BufferSize = 100 });
            neuroMonitor = Task.Run(neuralMonitorTaskAsync);

            // Success, return true
            return true;
        }

        public void Dispose()
        {
            // Tell BIC that we want to close!
            deviceClient.bicNeuralStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = false });
            var disposeReply = deviceClient.bicDispose(new RequestDeviceAddress() { DeviceAddress = DeviceName });
            Console.WriteLine("Dispose BIC Response: " + disposeReply.ToString());
            aGRPChannel.ShutdownAsync().Wait();
        }

        async Task neuralMonitorTaskAsync()
        {
            var stream = deviceClient.bicNeuralStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = true });
            while (await stream.ResponseStream.MoveNext())
            {
                Console.WriteLine("Implant Stream Neural Samples Received: " + stream.ResponseStream.Current.Samples.Count);
            }
            Console.WriteLine("(Neural Monitor Task Exited)");
        }
    }
}
