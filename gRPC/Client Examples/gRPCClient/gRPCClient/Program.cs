using System;
using Grpc.Core;
using BICgRPC;
using System.Threading.Tasks;
using Google.Protobuf.WellKnownTypes;

namespace BICgRPC_ConsoleTest
{
    class Program
    {
        // 
        static BICBridgeService.BICBridgeServiceClient bridgeClient;
        static BICDeviceService.BICDeviceServiceClient deviceClient;
        static bool tempStreamEnabled = false;

        public static void Main(string[] args)
        {
            // Basic Server Information
            Channel channel = new Channel("127.0.0.1:50051", ChannelCredentials.Insecure);

            // Instantiate the Bridge and scan for bridges
            Console.WriteLine("Instantiating Bridge Service Client and scanning for bridges...");
            bridgeClient = new BICBridgeService.BICBridgeServiceClient(channel);
            var scanBridgesReply = bridgeClient.ScanBridges(new QueryBridgesRequest());
            if(scanBridgesReply.Bridges.Count == 0)
            {
                Console.WriteLine("No bridges found, press a key to exit program.");
                Console.ReadKey();
                return;
            }

            // Print out all Bridges Found
            Console.WriteLine("Bridges Found:");
            for(int i = 0; i < scanBridgesReply.Bridges.Count; i++)
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
            deviceClient = new BICDeviceService.BICDeviceServiceClient(channel);
            var scanDevicesReply = deviceClient.ScanDevices(new ScanDevicesRequest() { BridgeName = bridgeName });

            // Ensure a device was found
            if(scanDevicesReply.Name == "")
            {
                Console.WriteLine("No devices found, press a key to exit program.");
                Console.ReadKey();
                return;
            }
            string deviceName = scanDevicesReply.Name;

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
            Console.WriteLine("Connecting to implantable device");
            var connectDeviceReply = deviceClient.ConnectDevice(new ConnectDeviceRequest() { DeviceAddress = deviceName, LogFileName = "./deviceLog.txt" });
            Console.WriteLine("Server Response: " + connectDeviceReply.Success);

            // Test starting up a thread
            tempStreamEnabled = true;
            Task tempMonitor = Task.Run(tempMonitorThreadAsync);

            // Done initializing, start user controlled loop
            writeCommandMenu();
            Random randomNumGen = new Random();
            ConsoleKeyInfo userCommand;
            do
            {
                userCommand = Console.ReadKey();
                Console.WriteLine();

                switch (userCommand.Key)
                {
                    case ConsoleKey.G:
                        bicGetImplantInfoReply anInfoReply = deviceClient.bicGetImplantInfo(new bicGetImplantInfoRequest() { UpdateCachedInfo = true });
                        Console.WriteLine("Received Device Info:");
                        Console.WriteLine("\tDevice.Address: " + deviceName);
                        Console.WriteLine("\tDevice.Id: " + anInfoReply.DeviceId.ToString());
                        Console.WriteLine("\tDevice.Type: " + anInfoReply.DeviceType.ToString());
                        Console.WriteLine("\tDevice.FirmwareVersion: " + anInfoReply.FirmwareVersion.ToString());
                        Console.WriteLine("\tDevice.ChannelCount: " + anInfoReply.ChannelCount.ToString());
                        Console.WriteLine("\tDevice.MeasurementChannelCount: " + anInfoReply.MeasurementChannelCount.ToString());
                        Console.WriteLine("\tDevice.StimulationChannelCount: " + anInfoReply.StimulationChannelCount.ToString());
                        Console.WriteLine("\tDevice.SamplingRate: " + anInfoReply.SamplingRate.ToString());
                        Console.WriteLine("\tDevice.Channels:");
                        for(int i = 0; i < anInfoReply.ChannelInfoList.Count; i++)
                        {
                            Console.WriteLine("\t\tChannel.Number: " + i.ToString());
                            Console.WriteLine("\t\tChannel.CanMeasureImpedance: " + anInfoReply.ChannelInfoList[i].CanMeasureImpedance);
                            Console.WriteLine("\t\tChannel.CanMeasure: " + anInfoReply.ChannelInfoList[i].CanMeasure);
                            Console.WriteLine("\t\tChannel.MeasureValueMax: " + anInfoReply.ChannelInfoList[i].MeasureValueMax);
                            Console.WriteLine("\t\tChannel.MeasureValueMin: " + anInfoReply.ChannelInfoList[i].MeasureValueMin);
                            Console.WriteLine("\t\tChannel.CanStimulate: " + anInfoReply.ChannelInfoList[i].CanStimulate);
                            Console.WriteLine("\t\tChannel.StimValueMax: " + anInfoReply.ChannelInfoList[i].StimValueMax);
                            Console.WriteLine("\t\tChannel.StimValueMin: " + anInfoReply.ChannelInfoList[i].StimValueMin);
                            Console.WriteLine("\t\tChannel.StimulationUnit: " + anInfoReply.ChannelInfoList[i].StimulationUnit);
                            Console.WriteLine();
                        }
                        break;
                    case ConsoleKey.I:
                        uint channelReq = (uint)randomNumGen.Next(0, 32);
                        bicGetImpedanceReply theImpedReply = deviceClient.bicGetImpedance(new bicGetImpedanceRequest() { Channel = channelReq });
                        Console.WriteLine("Implant Get Impedance, Selected Channel: " + channelReq.ToString() +  ", Impedance: " + theImpedReply.ChannelImpedance.ToString() + theImpedReply.Units);
                        break;
                    case ConsoleKey.T:
                        bicGetTemperatureReply theTempReply = deviceClient.bicGetTemperature(new Empty());
                        Console.WriteLine("Implant Get Temperature: " + theTempReply.Temperature.ToString() + theTempReply.Units);
                        break;
                    case ConsoleKey.H:
                        bicGetHumidityReply theHumidReply = deviceClient.bicGetHumidity(new Empty());
                        Console.WriteLine("Implant Get Humidity: " + theHumidReply.Humidity.ToString() + theHumidReply.Units);
                        break;
                    case ConsoleKey.S:
                        bicSetSensingEnableRequest aSensingReq = new bicSetSensingEnableRequest() { EnableSensing = true };
                        // Can define any reference channels by adding them to the list, for simplicity I'm not doping this currently, but this is why I'm declaring aSensingReq.
                        //aSensingReq.RefChannels.Add(0);
                        Console.WriteLine("Implant Power On Command Result: " + deviceClient.bicSetSensingEnable(aSensingReq).Success);
                        break;
                    case ConsoleKey.N:                        
                        Console.WriteLine("Implant Sensing Off Command Result: " + deviceClient.bicSetSensingEnable(new bicSetSensingEnableRequest() { EnableSensing = false }).Success) ;
                        break;
                    case ConsoleKey.P:
                        Console.WriteLine("Implant Power On Command Result: " + deviceClient.bicSetImplantPower(new bicSetImplantPowerRequest() { PowerEnabled = true }).Success);
                        break;
                    case ConsoleKey.O:
                        Console.WriteLine("Implant Power Off Command Result: " + deviceClient.bicSetImplantPower(new bicSetImplantPowerRequest() { PowerEnabled = false }).Success);
                        break;
                    case ConsoleKey.D1:
                        Console.WriteLine("Stim On Not Implemented");
                        break;
                    case ConsoleKey.D0:
                        Console.WriteLine("Stim Off Not Implemented");
                        break;
                    case ConsoleKey.Q:
                        break;
                    default:
                        writeCommandMenu();
                        break;
                }

            } while (userCommand.Key != ConsoleKey.Q);
            
            var reply3 = deviceClient.bicDispose(new Empty()) ;
            Console.WriteLine("Dispose BIC Response: " + reply3.Success);
            channel.ShutdownAsync().Wait();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }

        static async Task tempMonitorThreadAsync()
        {
            var tempStream = deviceClient.bicTemperatureStream(new Empty());

            while(tempStreamEnabled)
            {
                await tempStream.ResponseStream.MoveNext();
                Console.WriteLine("Implant Stream Temperature: " + tempStream.ResponseStream.Current.Temperature.ToString() + tempStream.ResponseStream.Current.Units);
            }
        }

        static void writeCommandMenu()
        {
            Console.WriteLine("Command Test Menu: ");
            Console.WriteLine("\tg : Get Implant Information");
            Console.WriteLine("\ti : Get Impedance Measurement for a random electrode");
            Console.WriteLine("\tt : Get Temperature");
            Console.WriteLine("\th : Get Humidity");
            Console.WriteLine("\ts : Start Sense Streaming");
            Console.WriteLine("\tn : Stop Sense Streaming");
            Console.WriteLine("\tp : Enable Power to Implant (enabled by default)");
            Console.WriteLine("\to : Disable Power to Implant (enabled by default)");
            Console.WriteLine("\t1 : Start Stimulation (not implemented yet)");
            Console.WriteLine("\t0 : Stop Stimulation (not implemented yet)");
            Console.WriteLine("\tq : Quit Program");
        }
    }
}
