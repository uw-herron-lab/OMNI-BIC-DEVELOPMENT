// current version: streamingFix
using System;
using System.IO;
using System.Net.Sockets;
using System.Text;

namespace EMGLib
{
    public class Delsys_Connection
    {
        // TCP/IP Parameters:
        private static TcpClient commandSocket;
        private static NetworkStream commandStream;
        private static StreamReader commandReader;
        private static StreamWriter commandWriter;

        private static int responseTracker;
        public static bool connectionStatus;
        private int connectPort = 50040;

        // Constructor:
        public Delsys_Connection()
        {
            responseTracker = 0;

        }


        public string Main()
        {
            string connectResponse = "Delsys Trigno System Digital Protocol Version 3.6.0";
            string streamResponse = "null";
            try
            {

                ////Establish TCP/IP connection to server using URL entered////
                //Initializes a new instance of the TcpClient class and connects to the specified port (50040) on the specified host (localhost)
                commandSocket = new TcpClient("localhost", connectPort);
                ////Set up communication streams////
                //Returns the NetworkStream (that has to be passed to StreamReader/Writer) used to send and receive data.
                commandStream = commandSocket.GetStream();

                //Implements a TextReader that reads characters from a byte stream in ASCII.
                commandReader = new StreamReader(commandStream, Encoding.ASCII);

                //Implements a TextWriter for writing characters to a stream in ASCII.
                commandWriter = new StreamWriter(commandStream, Encoding.ASCII);

                ////Get initial response from server////
                streamResponse = commandReader.ReadLine();
                //streamResponse[responseTracker] = commandReader.ReadLine();
                //responseTracker++;
                commandReader.ReadLine();   //get extra line terminator



                if (streamResponse != null)
                {
                    connectionStatus = true;
                }
                else
                {
                    Console.WriteLine("INVALID RESPONSE - failed to connect to EMG Delsys via TCP/IP");
                }

            }
            catch
            {
                connectionStatus = false;
            }
            return streamResponse;
        }

        public string SendCommand(string command)
        {
            string streamResponse = "null";
            try
            {
                if (connectionStatus)
                {
                    // Send command: i.e. START or STOP
                    //string packetTerminate = "\r\n\r\n";
                    //commandWriter.WriteLine(command +packetTerminate);
                    commandWriter.WriteLine(command);
                    commandWriter.WriteLine();  //terminate command
                    commandWriter.Flush();  //make sure command is sent immediately (Clears all buffers for the current writer and causes any buffered data to be written to the underlying stream)
                                            //Read the response line and display
                    streamResponse = commandReader.ReadLine();
                    //streamResponse[responseTracker] = commandReader.ReadLine();
                    responseTracker++;
                    commandReader.ReadLine();   //get extra line terminator

                }
                else
                {
                    Console.WriteLine("CANNOT STOP if not connected already.");
                }
            }
            catch
            {
                Console.WriteLine("Could not send command successfully.");
            }

            return streamResponse;
        }

        private void CloseConnection()
        {
            commandReader.Close();
            commandSocket.Close();
        }
    }
}
