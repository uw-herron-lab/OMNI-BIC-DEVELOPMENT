// Copyright 2015 gRPC authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

using System;
using Grpc.Core;
using BICgRPC;

namespace GreeterClient
{
    class Program
    {
        public static void Main(string[] args)
        {
            Channel channel = new Channel("127.0.0.1:50051", ChannelCredentials.Insecure);

            var client = new BICManager.BICManagerClient(channel);

            var reply = client.bicInit(new bicInitRequest { LogFileName = "./test.log" });
            Console.WriteLine("Server Response: " + reply.Success);
            Console.WriteLine("Press any key to proceed...");
            Console.ReadKey();

            var reply2 = client.bicGetImplantInfo(new bicGetImplantInfoRequest {  });
            Console.WriteLine("Server Response: " + reply2.DeviceId);
            Console.WriteLine("Press any key to initiate disposal...");
            Console.ReadKey();

            var reply3 = client.bicDispose(new bicNullRequest {  });
            Console.WriteLine("Server Response: " + reply3.Success);
            channel.ShutdownAsync().Wait();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }
    }
}
