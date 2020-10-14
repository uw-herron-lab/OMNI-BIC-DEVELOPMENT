﻿// Copyright 2015 gRPC authors.
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

            var client = new Greeter.GreeterClient(channel);
            String user = "aNewSummitProjectName";

            var reply = client.SayHello(new HelloRequest { Name = user });
            Console.WriteLine("Server Response: " + reply.Message);
            Console.WriteLine("Press any key to proceed...");
            Console.ReadKey();
            var reply2 = client.SayHelloAgain(new HelloRequest { Name = user });
            Console.WriteLine("Server Response: " + reply2.Message);

            
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
            channel.ShutdownAsync().Wait();
        }
    }
}
