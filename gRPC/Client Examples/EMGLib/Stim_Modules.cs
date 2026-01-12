// current version: streamingFix
using System;
using System.Collections.Generic;
using System.Diagnostics.Eventing.Reader;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace EMGLib
{
    public class Stim_Modules
    {
        public bool stimEnabled = false;
        public bool generateStim = false;
        public float percent;
        int numberOfChannels;

        // Calibration and threshold related
        public float[] maxSig;
        public float[] thresh;

        public Stim_Modules(int numChannels)
        {
            numberOfChannels = numChannels;
            maxSig = new float[numChannels];
            thresh = new float[numChannels];
        }

        public void setThresh()
        {
            // calculate threshold for each channel
            for (int ch = 0; ch < numberOfChannels; ch++)
            {
                thresh[ch] = maxSig[ch] * percent / 100;
                Console.WriteLine("max sig/thresh: " + maxSig[ch].ToString()
                    + "/" + thresh[ch].ToString());
            }
        }

        public float[] rectifySignals(float[] signal)
        {
            float[] rectifiedSignal = new float[signal.Length];
            for (int ch = 0; ch < signal.Length; ch++)
            {
                rectifiedSignal[ch] = Math.Abs(signal[ch]);
            }
            return rectifiedSignal;
        }

        // the name of this method is misleading, it should be changed to e.g. identifyMovement, 
        public (int[] movementDetected, long[] movementDetectedTimestamp) triggerStim(float[] signal, int ch, float[] thresh)
        {
            
            // movement not detected = 0, movement detected = 1
            int[] stimulate = new int[thresh.Length];
            long[] movementDetectedTimestamp = new long[thresh.Length];
            generateStim = false;
            // TO DO: check to make sure stim isn't set to true for channels other than 1
            //for (int ch = 0; ch < thresh.Length; ch++)
            //{
                if (signal[ch] >= thresh[ch] & signal[ch] != 0)
                //if (1 >= thresh[ch] & signal[ch] != 0)

                {
                    // timestamp for when signal above threshold was detected
                    movementDetectedTimestamp[ch] = DateTime.Now.Ticks;
                    stimulate[ch] = 1;

                    generateStim = true;
                    /*need to change the way filtered data is being outputted, 
					    * and also need to implement loading of filter coefficients with calibration data. 
					    * for some reason signal[ch] is not currently greater than thresh[ch]
                        */

                }
                else
                {
                    movementDetectedTimestamp[ch] = DateTime.Now.Ticks;
                    stimulate[ch] = 0;
                    //NEW: this is redundant but just in case for testing
                    generateStim = false;

                }
            //}

            return (stimulate, movementDetectedTimestamp);
            
        }
    }
}
