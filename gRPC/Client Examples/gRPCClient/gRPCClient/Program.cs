using System;
using Grpc.Core;
using BICgRPC;
using System.Threading.Tasks;

namespace BICgRPC_ConsoleTest
{
    class Program
    {
        // Clients
        static BICBridgeService.BICBridgeServiceClient bridgeClient;
        static BICDeviceService.BICDeviceServiceClient deviceClient;
        static BICInfoService.BICInfoServiceClient infoClient;
        static string DeviceName;

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

            // testing infoSservice
            infoClient = new BICInfoService.BICInfoServiceClient(channel);

            // Ensure a device was found
            if(scanDevicesReply.Name == "")
            {
                Console.WriteLine("No devices found, press a key to exit program.");
                Console.ReadKey();
                return;
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

            // Task pointers for streaming methods
            Task temperMonitor = null;
            Task humidityMonitor = null;
            Task connectionMonitor = null;
            Task powerMonitor = null;
            Task errorMonitor = null;
            Task neuroMonitor = null;

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
                        bicGetImplantInfoReply anInfoReply = deviceClient.bicGetImplantInfo(new bicGetImplantInfoRequest() { DeviceAddress = DeviceName, UpdateCachedInfo = true });
                        Console.WriteLine("Received Device Info:");
                        Console.WriteLine("\tDevice.Address: " + DeviceName);
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
                        bicGetImpedanceReply theImpedReply = deviceClient.bicGetImpedance(new bicGetImpedanceRequest() { DeviceAddress = DeviceName, Channel = channelReq });
                        Console.WriteLine("Implant Get Impedance, Selected Channel: " + channelReq.ToString() +  ", Impedance: " + theImpedReply.ChannelImpedance.ToString() + theImpedReply.Units);
                        break;
                    case ConsoleKey.T:
                        bicGetTemperatureReply theTempReply = deviceClient.bicGetTemperature(new RequestDeviceAddress() { DeviceAddress = DeviceName });
                        Console.WriteLine("Implant Get Temperature: " + theTempReply.Temperature.ToString() + theTempReply.Units);
                        if(temperMonitor == null || temperMonitor.IsCompleted)
                        {
                            // Start up the stream
                            temperMonitor = Task.Run(temperMonitorTaskAsync);
                        }
                        else
                        {
                            // Stop the stream
                            deviceClient.bicTemperatureStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = false });
                        }
                        break;
                    case ConsoleKey.H:
                        bicGetHumidityReply theHumidReply = deviceClient.bicGetHumidity(new RequestDeviceAddress() { DeviceAddress = DeviceName });
                        Console.WriteLine("Implant Get Humidity: " + theHumidReply.Humidity.ToString() + theHumidReply.Units);
                        if (humidityMonitor == null || humidityMonitor.IsCompleted)
                        {
                            // Start up the stream
                            humidityMonitor = Task.Run(humidMonitorTaskAsync);
                        }
                        else
                        {
                            // Stop the stream
                            deviceClient.bicHumidityStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = false });
                        }
                        break;
                    case ConsoleKey.F:
                        // infoService
                        VersionNumberResponse verNumber = infoClient.VersionNumber(new VersionNumberRequest());
                        SupportedDevicesResponse suppDevices = infoClient.SupportedDevices(new SupportedDevicesRequest());
                        InspectRepositoryResponse repos = infoClient.InspectRepository(new InspectRepositoryRequest());
                        Console.WriteLine("Testing infoService:");
                        Console.WriteLine("\tInfo.VersionNumber: " + verNumber.VersionNumber.ToString());
                        Console.WriteLine("\tInfo.SupportedDevices: " + suppDevices.SupportedDevices.ToString());
                        Console.WriteLine("\tInfo.InspectRepository: " + repos.ToString());
                        break;
                    case ConsoleKey.E:
                        if (errorMonitor == null || errorMonitor.IsCompleted)
                        {
                            // Start up the stream
                            errorMonitor = Task.Run(errorMonitorTaskAsync);
                        }
                        else
                        {
                            // Stop the stream
                            deviceClient.bicErrorStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = false });
                        }
                        break;
                    case ConsoleKey.C:
                        if (connectionMonitor == null || connectionMonitor.IsCompleted)
                        {
                            // Start up the stream
                            connectionMonitor = Task.Run(connectionMonitorTaskAsync);
                        }
                        else
                        {
                            // Stop the stream
                            deviceClient.bicConnectionStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = false });
                        }
                        break;
                    case ConsoleKey.L:
                        if (powerMonitor == null || powerMonitor.IsCompleted)
                        {
                            // Start up the stream
                            powerMonitor = Task.Run(powerMonitorTaskAsync);
                        }
                        else
                        {
                            // Stop the stream
                            deviceClient.bicPowerStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = false });
                        }
                        break;
                    case ConsoleKey.S:
                        if (neuroMonitor == null || neuroMonitor.IsCompleted)
                        {
                            // Start up the stream
                            neuroMonitor = Task.Run(neuralMonitorTaskAsync);
                        }
                        else
                        {
                            deviceClient.bicNeuralStream(new bicNeuralSetStreamingEnable() { DeviceAddress = DeviceName, Enable = false });
                        }
                        break;
                    case ConsoleKey.P:
                        Console.WriteLine("Implant Power On Command Result: " + deviceClient.bicSetImplantPower(new bicSetImplantPowerRequest() { DeviceAddress = DeviceName, PowerEnabled = true }));
                        break;
                    case ConsoleKey.O:
                        Console.WriteLine("Implant Power Off Command Result: " + deviceClient.bicSetImplantPower(new bicSetImplantPowerRequest() { DeviceAddress = DeviceName, PowerEnabled = false }));
                        break;
                    case ConsoleKey.D1:
                        // Create a waveform defintion request 
                        bicStimulationFunctionDefinitionRequest aNewWaveform = new bicStimulationFunctionDefinitionRequest() { DeviceAddress = DeviceName };
                        // Create a pulse function
                        StimulationFunctionDefinition pulseFunction = new StimulationFunctionDefinition() { FunctionName = "pulseFunction", 
                            StimPulse = new stimPulseFunction() { Amplitude = { 1000, 0, 0, 0 }, DZ0Duration = 10, DZ1Duration = 2550, PulseWidth = 400, Repetitions = (uint)randomNumGen.Next(1,10), SourceElectrodes = { 0 }, SinkElectrodes = { 32 } } };
                        // Create a pause function
                        StimulationFunctionDefinition pauseFunction = new StimulationFunctionDefinition() { FunctionName = "pauseFunction",
                            Pause = new pauseFunction() { Duration = 30000 } };
                            
                        // Load functions into waveform, then into command
                        aNewWaveform.Functions.Add(pulseFunction);
                        aNewWaveform.Functions.Add(pauseFunction);
                        deviceClient.bicDefineStimulationWaveform(aNewWaveform);
                        deviceClient.bicStartStimulation(new bicStartStimulationRequest() { DeviceAddress = DeviceName, FunctionName = "aWaveForm" });
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
            
            var reply3 = deviceClient.bicDispose(new RequestDeviceAddress() { DeviceAddress = DeviceName }) ;
            Console.WriteLine("Dispose BIC Response: " + reply3.ToString());
            channel.ShutdownAsync().Wait();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }

        static async Task temperMonitorTaskAsync()
        {
            var stream = deviceClient.bicTemperatureStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = true });
            while(await stream.ResponseStream.MoveNext())
            {
                Console.WriteLine("Implant Stream Temperature: " + stream.ResponseStream.Current.Temperature.ToString() + stream.ResponseStream.Current.Units);
            }
            Console.WriteLine("(Temperature Monitor Task Exited)");
        }

        static async Task humidMonitorTaskAsync()
        {
            var stream = deviceClient.bicHumidityStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = true });
            while (await stream.ResponseStream.MoveNext())
            {
                Console.WriteLine("Implant Stream Humidity: " + stream.ResponseStream.Current.Humidity.ToString() + stream.ResponseStream.Current.Units);
            }
            Console.WriteLine("(Humidity Monitor Task Exited)");
        }

        static async Task connectionMonitorTaskAsync()
        {
            var stream = deviceClient.bicConnectionStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = true });
            while (await stream.ResponseStream.MoveNext())
            {
                Console.WriteLine("Implant Stream Connectivity Event: " + stream.ResponseStream.Current.ConnectionType + " " + stream.ResponseStream.Current.IsConnected.ToString());
            }
            Console.WriteLine("(Connection Monitor Task Exited)");
        }

        static async Task errorMonitorTaskAsync()
        {
            var stream = deviceClient.bicErrorStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = true });
            while (await stream.ResponseStream.MoveNext())
            {
                Console.WriteLine("Implant Stream Error Event: " + stream.ResponseStream.Current.Message);
            }
            Console.WriteLine("(Error Monitor Task Exited)");
        }

        static async Task powerMonitorTaskAsync()
        {
            var stream = deviceClient.bicPowerStream(new bicSetStreamEnable() { DeviceAddress = DeviceName, Enable = true });
            while (await stream.ResponseStream.MoveNext())
            {
                Console.WriteLine("Implant Stream Power Event: " + stream.ResponseStream.Current.Parameter.ToString() + ":" + stream.ResponseStream.Current.Value.ToString() + stream.ResponseStream.Current.Units);
            }
            Console.WriteLine("(Power Monitor Task Exited)");
        }

        static async Task neuralMonitorTaskAsync()
        {
            var stream = deviceClient.bicNeuralStream(new bicNeuralSetStreamingEnable() { DeviceAddress = DeviceName, Enable = true, BufferSize = 100 });
            while (await stream.ResponseStream.MoveNext())
            {
                Console.WriteLine("Implant Stream Neural Samples Received: " + stream.ResponseStream.Current.Samples.Count);
            }
            Console.WriteLine("(Neural Monitor Task Exited)");
        }

        static void writeCommandMenu()
        {
            Console.WriteLine("Command Test Menu: ");
            Console.WriteLine("\tg : Get Implant Information");
            Console.WriteLine("\ti : Get Impedance Measurement for a random electrode");
            Console.WriteLine("\tt : Get Temperature and toggle Temperature Streaming");
            Console.WriteLine("\th : Get Humidity and toggle Humidity Streaming");
            Console.WriteLine("\tf : Testing infoService");
            Console.WriteLine("\te : Toggle Error Streaming");
            Console.WriteLine("\tc : Toggle Connection Streaming");
            Console.WriteLine("\tl : Toggle Power State Streaming");
            Console.WriteLine("\ts : Toggle Neural Sense Streaming");
            Console.WriteLine("\tp : Enable Power to Implant (enabled by default)");
            Console.WriteLine("\to : Disable Power to Implant (enabled by default)");
            Console.WriteLine("\t1 : Start Stimulation - random number (1-9) 1mA pulses");
            Console.WriteLine("\t0 : Stop Stimulation (not implemented yet)");
            Console.WriteLine("\tq : Quit Program");
        }
    }
}
