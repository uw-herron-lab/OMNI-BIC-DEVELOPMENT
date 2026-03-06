// version: streamingFix branch
using System;
using System.Collections.Concurrent;
using System.IO;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using System.Linq;
using System.Windows;
using System.Collections.Generic;

namespace EMGLib
{
    public class EMG_Streaming
    {
        // TCP/IP parameters
        TcpClient emgSocket;
        NetworkStream emgStream;
        BinaryReader emgReader;

        // EMG parameters
        private int emgDataPort = 50043; // Delsys TCP Port
        private int numberOfChannels = 16; // Number of Trigno Delsys EMGs corresponding to number of channels
        private int bytesPerChannel = 4; // Port 50043 processes 4-byte float per sample per channel
        private int bytesPerSample;

        // For Plotting 
        private int numSamplesToPlot = 2000;
        private List<float>[] r_emgDataToPlot; // raw data
        private List<float>[] f_emgDataToPlot; // filt data
        private Object plotDataLock = new Object();
        private Object plotFiltLock = new object();

        private List<double>[] threshDataToPlot;
        private List<int>[] stimDataToPlot;

        public bool streamingStatus = false;
        public long stimulatorTimestamp = 0;

        // for logging
        StreamWriter emgSW;
        StreamWriter emgFiltSW;
        StreamWriter emgEnvelopedSW;
        StreamWriter emgTimestampSW;
        public string currPart;
        public bool logging = false;
        public int currTrial = 0;
        string file_extension;

        // for plotting
        private BlockingCollection<float[]> rawSamplesQueueForPlot = new BlockingCollection<float[]>();
        private BlockingCollection<float[]> filtSamplesQueueForPlot = new BlockingCollection<float[]>();


        // related to filter
        private BlockingCollection<emgPacket> rawSamplesQueueForProcc = new BlockingCollection<emgPacket>();
        public Processing_Modules _processingMod;

        // related to stim
        public Stim_Modules _stimMod;
        //private BlockingCollection<double[]> threshSamplesQueueForPlot = new BlockingCollection<double[]>();
        //private BlockingCollection<int[]> stimSamplesQueueForPlot = new BlockingCollection<int[]>();

        public bool _generateStim = false;
        public bool _stimEnabled = false;

        private static long streamStart_timestamp;

        // calibration bool, if true stim is disabled, if false stim is enabled
        public bool calibrationOn = false;

        private struct emgPacket
        {
            public emgPacket(float[] rawSamples, long timestamp)
            {
                samples = rawSamples;
                stamp = timestamp;
            }

            public float[] samples { get; }
            public long stamp { get; }

        }

        public EMG_Streaming()
        {
            bytesPerSample = numberOfChannels * bytesPerChannel;
            r_emgDataToPlot = new List<float>[numSamplesToPlot];
            f_emgDataToPlot = new List<float>[numSamplesToPlot];

            threshDataToPlot = new List<double>[numSamplesToPlot];
            stimDataToPlot = new List<int>[numSamplesToPlot];

            file_extension = $"{DateTime.Now:yyyy-MM-dd_HH-mm-ss}.csv";

            _processingMod = new Processing_Modules(numberOfChannels);
            _stimMod = new Stim_Modules(numberOfChannels);


            // create a non-null array of lists 
            for (int i = 0; i < numSamplesToPlot; i++)
            {
                float[] data = new float[numberOfChannels];
                double[] t = new double[numberOfChannels];
                int[] s = new int[numberOfChannels];
                for (int ch = 0; ch < numberOfChannels; ch++)
                {
                    data[ch] = 0f;
                    t[ch] = 0f;
                    s[ch] = 0;
                }
                r_emgDataToPlot[i] = new List<float>(data);
                f_emgDataToPlot[i] = new List<float>(data);
                threshDataToPlot[i] = new List<double>(t);
                stimDataToPlot[i] = new List<int>(s);
            }


        }

        public void emgDataPort_Connect()
        {

            emgSocket = new TcpClient();
            //emgSocket.ReceiveBufferSize = bytesPerSample; ////***
            emgSocket.Connect("localhost", emgDataPort);
            emgSocket.NoDelay = true; // sending small amounts of data is inefficient, however, pushes for immediate response

        }
        public void emgDataPort_Diconnect()
        {

            emgReader.Dispose();
            emgStream.Dispose();
            emgSocket.Dispose();

            emgSW.Dispose();
            emgTimestampSW.Dispose();

            emgFiltSW.Dispose();
            if (!calibrationOn)
            {
                //emgEnvelopedSW.Dispose();
            }
        }

        public void StreamEMG(CancellationToken token, string saveDir)
        {
            // establish transmission
            streamStart_timestamp = DateTime.Now.Ticks;
            emgStream = emgSocket.GetStream();
            emgReader = new BinaryReader(emgStream);

            // setup logging

            string filename = currPart + "_RawFormattedEMGData_" + file_extension;
            string stamp_filename = currPart + "_TimestampEMG_" + file_extension;
            if (calibrationOn)
            {
                saveDir = Path.Combine(saveDir, "Calibration");
            }

            try
            {
                if (!Directory.Exists(saveDir))
                {
                    System.IO.Directory.CreateDirectory(saveDir);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("EMG Streamer, Directory Exception - " + ex.Message.ToString());
            }

            emgSW = new StreamWriter(Path.Combine(saveDir, filename));
            string emgLog_label = "EMG1";
            for (int i = 1; i < numberOfChannels; i++)
            {
                emgLog_label = string.Join(",", emgLog_label, "EMG" + (i + 1).ToString());
            }
            emgLog_label = string.Join(",", emgLog_label, "Timestamp", "Trial num");
            emgSW.WriteLine(emgLog_label);
            emgSW.Flush();
            // timestamp log file could be used for easier interpolation of timestamps if needed
            // (since there are repeats of the same timestamp for about 25 samples)
            emgTimestampSW = new StreamWriter(Path.Combine(saveDir, stamp_filename));
            emgTimestampSW.WriteLine("Timestamp of EMG bytes retrieved");
            emgTimestampSW.Flush();

            while (!token.IsCancellationRequested)
            {
                // beging streaming bytes from the API
                try
                {
                    byte[] sampleBuffer;
                    long formattedTimestamp;
                    int bytesAvailable = emgSocket.Available; // max EMG samples per frame is 27, equaling 1728 bytes. which is the fasted data can be received. 
                                                              // max data that can be held for transmission is 65536 bytes, 1024 samples, if not attempted to receive fast enough
                    float[] unpackedSamp = new float[numberOfChannels];

                    if (bytesAvailable > bytesPerSample)
                    {
                        sampleBuffer = new byte[bytesPerSample];
                        emgReader.Read(sampleBuffer, 0, bytesPerSample); // reads total bytes for each sample i.e. 64
                        formattedTimestamp = DateTime.Now.Ticks; // timestamps the bytes read


                        int indTracker = 0; // used to process the total bytes of all channels for each sample

                        // convert bytes to floating point values
                        unpackedSamp[0] = BitConverter.ToSingle(sampleBuffer.Skip(indTracker).Take(bytesPerChannel).ToArray(), 0);
                        indTracker = indTracker + 4;
                        string emgLog = unpackedSamp[0].ToString();
                        for (int i = 1; i < numberOfChannels; i++)
                        {
                            unpackedSamp[i] = BitConverter.ToSingle(sampleBuffer.Skip(indTracker).Take(bytesPerChannel).ToArray(), 0);
                            emgLog = string.Join(",", emgLog, unpackedSamp[i]);
                            indTracker = indTracker + 4;

                        }
                        if (logging)
                        {
                            emgLog = string.Join(",", emgLog, formattedTimestamp, currTrial);
                            emgSW.WriteLine(emgLog);
                            emgTimestampSW.WriteLine(formattedTimestamp);

                        }

                        // add unpacked data to queue for prepping to plot
                        emgPacket rawSampBuff = new emgPacket(unpackedSamp, formattedTimestamp);
                        rawSamplesQueueForProcc.Add(rawSampBuff);
                        lock (plotDataLock)
                        {
                            rawSamplesQueueForPlot.Add(unpackedSamp);
                        }

                    }

                }
                catch (Exception e)
                {
                    emgSW.Flush();
                    emgTimestampSW.Flush();
                    Console.WriteLine("EMG Streamer - " + e.Message);
                }
            }
            // flush outside of while loop to avoid taking up processing time
            emgTimestampSW.Flush();
            emgSW.Flush();
        }

        // TO DO: change the following method to filter and log filtered and algo related info
        public void filtEMGstream(CancellationToken token, string saveDir)
        {
            
			if (calibrationOn)
			{
				saveDir = Path.Combine(saveDir, "Calibration");
			}
			string filename = currPart + "_FiltEMGData_" + file_extension;
            emgFiltSW = new StreamWriter(Path.Combine(saveDir, filename));
			emgFiltSW.WriteLine(string.Join(",", "raw signal", "TTL signal", "raw timestamp", "filt signal", "filt timestamp", "env signal", "env timestamp", "MTS on", "send stim", "movement detected", "movement timestamp", "stimulator timestamp", "percentage", "threshold", "max MVC", "Trial"));
			emgFiltSW.Flush();


            float[] filtSamples = new float[numberOfChannels];
            float[] envelopedSamples = new float[numberOfChannels];
            while (!token.IsCancellationRequested)
            {
                try
                {
                    int rawSamplesAvailable = rawSamplesQueueForProcc.Count;
                    if (rawSamplesAvailable > 0)
                    {
                        emgPacket rawSampPacket = new emgPacket();
                        // theoretically this should take the data as it becomes available quickly,
                        // but this process might create a latency that later on should be fixed -> maybe should be incorporated into the same thread for raw data being logged?
                        // or maybe the most recent data available should be processed for stimulation and the rest, if not processed already, should be ignored. but this process is needed for plotting
                        rawSampPacket = rawSamplesQueueForProcc.Take(); // take raw samples
                        float[] rawSamples = rawSampPacket.samples;
                        long timestampForAllSamples = rawSampPacket.stamp;
                        // filter data
                        filtSamples = _processingMod.IIRFilter(rawSamples); // bandpass filter raw samples
                        long bandpassFiltTS = DateTime.Now.Ticks;
                        envelopedSamples = _processingMod.envelopeSignals(_stimMod.rectifySignals(filtSamples)); // envelope bandpass filtered samples
                        long envFiltTS = DateTime.Now.Ticks;
                        // FILE SAVED FOR CALIBRATION DATA VS STIM DATA ARE DIFFERENT, SINCE CALIBRATION DATA WILL NOT INCLUDE STIM VALUES
                        // ADD CHECK BOX TO UI INDICATE WHETHER CALIBRATION
                        // movement detection
                        // add raw, filtered, and movement detection to queue

                        //_stimMod.stimEnabled = _stimEnabled; // currenly doesn't do anything

                        (int[] movementDetected, long[] movementDetectedTimestamp) = _stimMod.triggerStim(envelopedSamples, 0);
                        //long movementDetectedTS = DateTime.Now.Ticks;

                        // TO DO: add lock to this 
                        _generateStim = _stimMod.generateStim;
                        //rawSamplesQueue.Add(emgSamples);
                        lock (plotFiltLock)
                        {
                            filtSamplesQueueForPlot.Add(filtSamples);

                        }
                        //threshSamplesQueueForPlot.Add(_stimMod.thresh);
                        //stimSamplesQueueForPlot.Add(movementDetected);

                        // TO DO: maybe do this in a different thread?
                        if (logging)
                        {

                            for (int i = 0; i < filtSamples.Length;)
                            {
								for (int ch = 0; ch < 16; ch++) // TO DO: can replace this to not have to loop through all channels
								{
									// only stores EMG1 at index i, and the TTL signal which is EMG2 at i+1

									if (ch == 0)
									{
										emgFiltSW.WriteLine(string.Join(",", rawSamples[i], rawSamples[i+1], timestampForAllSamples, filtSamples[i], bandpassFiltTS, envelopedSamples[i], envFiltTS, _stimEnabled, _generateStim, movementDetected[i], movementDetectedTimestamp[i], stimulatorTimestamp, _stimMod.percent, _stimMod.thresh[0], _stimMod.maxSig[0], currTrial));
									}
                                    i++;
								}
								//emgFiltSW.WriteLine(string.Join(",", $"{i + 1}", filtSamples[i].ToString(), timestampForAllSamples, bandpassFiltTS));
                                //emgEnvelopedSW.WriteLine(string.Join(",", $"{i + 1}", envelopedSamples[i].ToString(), _stimEnabled, _generateStim, movementDetected[i], movementDetectedTimestamp[i], timestampForAllSamples, _stimMod.percent, _stimMod.thresh[i]));
                            }

                        }
                    }
                }
                catch (Exception ex)
                {
                    emgFiltSW.Flush();
                    if (!calibrationOn)
                    {
                        //emgEnvelopedSW.Flush();
                    }
                    Console.WriteLine("Filt function thread - " + ex.Message);
                }
            }
            emgFiltSW.Flush();
            if (!calibrationOn)
            {
                //emgEnvelopedSW.Flush();
            }
        }
        // TO DO: add another method for prepping filtered data for plotting
        public void prepRawForPlot(CancellationToken token)
        {

            // check how much data is available
            // check how much data is in the returned list for plotting (if it's empty, only return when it's no longer empty)
            while (!token.IsCancellationRequested)
            {
                try
                {
                    // check how much data is available
                    int samplesAvailable = rawSamplesQueueForPlot.Count;

                    // for however much data is available, remove that much data from the beginning of the returned list
                    // turn all the available data into lists, concatenate that with the returned list,
                    if (samplesAvailable < numSamplesToPlot && samplesAvailable != 0)
                    {
                        // for every sample not available in the numSamplesToPlot
                        int numSamplesToBuffer = numSamplesToPlot - samplesAvailable;
                        List<float>[] buffer = new List<float>[numSamplesToBuffer];
                        buffer[0] = r_emgDataToPlot[numSamplesToPlot - numSamplesToBuffer];
                        int indTracker = 1;
                        lock (plotDataLock)
                        {
                            for (int b = numSamplesToPlot - numSamplesToBuffer + 1; b < numSamplesToPlot; b++)
                            {
                                buffer[indTracker] = r_emgDataToPlot[b];
                                indTracker++;
                            }
                            for (int b = 0; b < numSamplesToBuffer; b++)
                            {
                                r_emgDataToPlot[b] = buffer[b];
                            }
                            for (int i = numSamplesToBuffer; i < numSamplesToPlot; i++)
                            {
                                float[] data = rawSamplesQueueForPlot.Take();
                                r_emgDataToPlot[i] = new List<float>(data);
                            }
                        }

                    }
                    else if (samplesAvailable >= numSamplesToPlot)
                    {
                        lock (plotDataLock)
                        {
                            for (int i = 0; i < numSamplesToPlot; i++)
                            {
                                float[] data = rawSamplesQueueForPlot.Take();
                                r_emgDataToPlot[i] = new List<float>(data);
                            }
                        }
                    }
                }
                catch (Exception e)
                {
                    Console.WriteLine("Prep for Flot - " + e.Message);
                }
            }
        }

        public void prepFiltForPlot(CancellationToken token)
        {

            // check how much data is available
            // check how much data is in the returned list for plotting (if it's empty, only return when it's no longer empty)
            while (!token.IsCancellationRequested)
            {
                try
                {
                    // check how much data is available
                    int samplesAvailable = filtSamplesQueueForPlot.Count;

                    // for however much data is available, remove that much data from the beginning of the returned list
                    // turn all the available data into lists, concatenate that with the returned list,
                    if (samplesAvailable < numSamplesToPlot && samplesAvailable != 0)
                    {
                        // for every sample not available in the numSamplesToPlot
                        int numSamplesToBuffer = numSamplesToPlot - samplesAvailable;
                        List<float>[] buffer = new List<float>[numSamplesToBuffer];
                        buffer[0] = f_emgDataToPlot[numSamplesToPlot - numSamplesToBuffer];
                        int indTracker = 1;
                        lock (plotFiltLock)
                        {
                            for (int b = numSamplesToPlot - numSamplesToBuffer + 1; b < numSamplesToPlot; b++)
                            {
                                buffer[indTracker] = f_emgDataToPlot[b];
                                indTracker++;
                            }
                            for (int b = 0; b < numSamplesToBuffer; b++)
                            {
                                f_emgDataToPlot[b] = buffer[b];
                            }
                            for (int i = numSamplesToBuffer; i < numSamplesToPlot; i++)
                            {
                                float[] data = filtSamplesQueueForPlot.Take();
                                f_emgDataToPlot[i] = new List<float>(data);
                            }
                        }

                    }
                    else if (samplesAvailable >= numSamplesToPlot)
                    {
                        lock (plotFiltLock)
                        {
                            for (int i = 0; i < numSamplesToPlot; i++)
                            {
                                float[] data = filtSamplesQueueForPlot.Take();
                                f_emgDataToPlot[i] = new List<float>(data);
                            }
                        }
                    }
                }
                catch (Exception e)
                {
                    Console.WriteLine("Prep for Flot - " + e.Message);
                }
            }
        }
        public List<float>[] getRawData()
        {
            List<float>[] emgDataToPlotBuffer = new List<float>[numSamplesToPlot];
            lock (plotDataLock)
            {
                r_emgDataToPlot.CopyTo(emgDataToPlotBuffer, 0);
            }
            return emgDataToPlotBuffer;
        }
        // TO DO: this needs to be implemented
        public List<float>[] getFiltData()
        {
            List<float>[] emgDataToPlotBuffer = new List<float>[numSamplesToPlot];
            lock (plotFiltLock)
            {
                f_emgDataToPlot.CopyTo(emgDataToPlotBuffer, 0);
            }
            return emgDataToPlotBuffer;
        }
        // TO DO:
        //public (List<double>[] thresh, List<int>[] stim) getMovementStimData()
        //{
        //    List<double>[] plotThreshData = plotThreshDataQueue.Take();
        //    List<int>[] plotStimData = plotStimDataQueue.Take();

        //    return (plotThreshData, plotStimData);
        //}

        private double elapsedTime(long sampleStamp)
        {
            double elapsed_time_per_sample;
            elapsed_time_per_sample = (sampleStamp - streamStart_timestamp) * 100 % 1e9 / 1e9;

            return elapsed_time_per_sample;

        }
    }
}