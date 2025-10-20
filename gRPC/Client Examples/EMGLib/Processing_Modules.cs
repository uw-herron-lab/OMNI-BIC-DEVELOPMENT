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
using System.Collections.Immutable;

namespace EMGLib
{
    public partial class Processing_Modules
    {
        private int numChannels;

        // IIR filter values \\
        private List<float>[] band_prevInput; // initial previous inputs (zero-padding)
        private List<float>[] band_prevFiltOut; // initial previous outputs (zero-padding)

        // TO DO: should be included in the config file - should they be modified based on the filter output of the calibration?
        // Filter coefficients: **currently copied from bandpass butterworth from python**
        private List<float> band_b = new List<float> { 0.231f, 0f, -0.4626f, 0f, 0.231f }; // numerator coefficients
        private List<float> band_a = new List<float> { 1f, -2.14f, 1.553f, -0.592f, 0.1834f }; // denominator coefficients
        //private float band_gainVal = 0.2313f;
        private float band_gainVal = 0.5f;

        // lowpass filter/evelope values \\
        private List<float>[] low_prevInput; // initial previous inputs (zero-padding)
        private List<float>[] low_prevFiltOut; // initial previous outputs (zero-padding)

		// Filter coefficients: **currently copied from bandpass butterworth from python**

		// Old values
		//private List<float> low_b = new List<float> { 0.0591907f, 0.0591907f }; // numerator coefficients
		//private List<float> low_a = new List<float> { 1f, -0.88161859f }; // denominator coefficients
		//private float low_gainVal = 0.059190703818405445f;

		// trial 1: unchanged gainVal -> update b,a lowcut = 2
		//private List<float> low_b = new List<float> { 0.00313176f, 0.00313176f }; // numerator coefficients
		//private List<float> low_a = new List<float> { 1f, -0.99373647f }; // denominator coefficients
		//private float low_gainVal = 0.0031317642291927056f;

		// trial 2:
		private List<float> low_b = new List<float> { 0.00313176f, 0.00313176f }; // numerator coefficients
		private List<float> low_a = new List<float> { 1f, -0.99373647f }; // denominator coefficients
		private float low_gainVal = 15f;

		public Processing_Modules(int channels)
        {
            numChannels = channels;

            band_prevInput = new List<float>[numChannels];
            band_prevFiltOut = new List<float>[numChannels];

            low_prevInput = new List<float>[numChannels];
            low_prevFiltOut = new List<float>[numChannels];



            // create input and output array for filter
            for (int i = 0; i < numChannels; i++)
            {
                band_prevInput[i] = new List<float> { 0f, 0f, 0f, 0f };
                band_prevFiltOut[i] = new List<float> { 0f, 0f, 0f, 0f };

                low_prevInput[i] = new List<float> { 0f, 0f };
                low_prevFiltOut[i] = new List<float> { 0f, 0f };
            }

        }
        public float[] IIRFilter(float[] currSamp)
        {
            // this is a band pass filter:
            float[] filtTemp = new float[16];

            for (int i = 0; i < 16; i++)
            {
                // 2nd order IIR filter
                //if(currSamp[i] != 0f)
                //{
                //    Console.WriteLine("test");
                //}
                filtTemp[i] = (band_gainVal * band_b[0] * currSamp[i] + band_gainVal * band_b[1] * band_prevInput[i][0] + band_gainVal * band_b[2] * band_prevInput[i][1] + band_gainVal * band_b[3] * band_prevInput[i][2] + band_gainVal * band_b[4] * band_prevInput[i][3]
                    - band_a[1] * band_prevFiltOut[i][0] - band_a[2] * band_prevFiltOut[i][1] - band_a[3] * band_prevFiltOut[i][2] - band_a[4] * band_prevFiltOut[i][3]);

                band_prevFiltOut[i].Insert(0, filtTemp[i]);
                band_prevFiltOut[i].RemoveAt(band_prevFiltOut[i].Count - 1);

                // Store most recent sample at the beginning of history window
                band_prevInput[i].Insert(0, currSamp[i]);
                band_prevInput[i].RemoveAt(band_prevInput[i].Count - 1);
            }


            return filtTemp;
        }

		//public float[] envelopeSignals(float[] currSamp)
		//{
		//    // this is a low pass filter: 
		//    // y[n] = (b0 * x[n] + b1 * x[n-1]) / (1 + a1 * y[n-1])

		//    float[] filtTemp = new float[16];
		//    for (int i = 0; i < 16; i++)
		//    {
		//        //filtTemp[i] = (low_gainVal * low_b[0] * currSamp[i] + low_gainVal * low_b[1] * low_prevInput[i][0] +
		//        //    -low_a[1] * low_prevFiltOut[i][0] - low_a[2] * low_prevFiltOut[i][1] - low_a[3] * low_prevFiltOut[i][2] - low_a[4] * low_prevFiltOut[i][3]);
		//        filtTemp[i] = (low_b[0] * currSamp[i] + low_b[1] * low_prevInput[i][0]) /
		//                      (1 + low_a[1] * low_prevFiltOut[i][0]);

		//        low_prevFiltOut[i].Insert(0, filtTemp[i]);
		//        low_prevFiltOut[i].RemoveAt(low_prevFiltOut[i].Count - 1);

		//        // Store most recent sample at the beginning of history window
		//        low_prevInput[i].Insert(0, currSamp[i]);
		//        low_prevInput[i].RemoveAt(low_prevInput[i].Count - 1);
		//    }

		//    return filtTemp;
		//}

		public float[] envelopeSignals(float[] currSamp)
		{
			// this is a low pass filter: 
			// y[n] = (b0 * x[n] + b1 * x[n-1]) / (1 + a1 * y[n-1])

			float[] filtTemp = new float[16];
			for (int i = 0; i < 16; i++)
			{
				//filtTemp[i] = (low_gainVal * low_b[0] * currSamp[i] + low_gainVal * low_b[1] * low_prevInput[i][0] +
				//    -low_a[1] * low_prevFiltOut[i][0] - low_a[2] * low_prevFiltOut[i][1] - low_a[3] * low_prevFiltOut[i][2] - low_a[4] * low_prevFiltOut[i][3]);
				filtTemp[i] = low_gainVal * (low_b[0] * currSamp[i] + low_b[1] * low_prevInput[i][0]) /
							  (1 + low_a[1] * low_prevFiltOut[i][0]);

				low_prevFiltOut[i].Insert(0, filtTemp[i]);
				low_prevFiltOut[i].RemoveAt(low_prevFiltOut[i].Count - 1);

				// Store most recent sample at the beginning of history window
				low_prevInput[i].Insert(0, currSamp[i]);
				low_prevInput[i].RemoveAt(low_prevInput[i].Count - 1);
			}

			return filtTemp;
		}
	}
}
