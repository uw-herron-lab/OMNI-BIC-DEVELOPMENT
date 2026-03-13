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
		private List<float> band_b = new List<float> { 0.23130797944426512f, 0.0f, -0.46261595888853024f, 0.0f, 0.23130797944426512f }; // numerator coefficients
		private List<float> band_a = new List<float> { 1.0f, -2.1400945982631905f, 1.5528893358210751f, -0.5922328575740207f, 0.18337778320123216f }; // denominator coefficients
																																//private float band_gainVal = 0.9f;
		//private float band_gainVal = 0.23130797944426512f;
		private float band_gainVal = 1f;

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

		private List<float> low_b = new List<float> { 0.0009446918438401507f, 0.0018893836876803015f, 0.0009446918438401507f }; // numerator coefficients
		private List<float> low_a = new List<float> { 1.0f, -1.911197067426073f, 0.9149758348014336f }; // denominator coefficients
		// second order cutoff 20Hz
		//private List<float> low_b = new List<float> { 0.000944, 1.96518336e-05f, 9.82591682e-06f }; // numerator coefficients
		//private List<float> low_a = new List<float> { 1f, -1.99111429f, 0.9911536f }; // denominator coefficients
		//private float low_gainVal = 0.0009446918438401507f;
		private float low_gainVal = 2f;

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
        public float[] IIRFilter(float[] currSamp, int i)
        {
            // 2nd order IIR filter
            // this is a band pass filter:
            float[] filtTemp = new float[16];
			// for now, to save on computation time, set all non essential channels to 0 instead of having them go thru filtering
			for (int ch = 0; ch < 16; ch++)
			{
				filtTemp[ch] = 0f;
			}

            //for (int i = 0; i < 16; i++)
            //{

            filtTemp[i] = (band_gainVal * band_b[0] * currSamp[i] + band_gainVal * band_b[1] * band_prevInput[i][0] + band_gainVal * band_b[2] * band_prevInput[i][1] + band_gainVal * band_b[3] * band_prevInput[i][2] + band_gainVal * band_b[4] * band_prevInput[i][3]
                - band_a[1] * band_prevFiltOut[i][0] - band_a[2] * band_prevFiltOut[i][1] - band_a[3] * band_prevFiltOut[i][2] - band_a[4] * band_prevFiltOut[i][3]);

            band_prevFiltOut[i].Insert(0, filtTemp[i]);
            band_prevFiltOut[i].RemoveAt(band_prevFiltOut[i].Count - 1);

            // Store most recent sample at the beginning of history window
            band_prevInput[i].Insert(0, currSamp[i]);
            band_prevInput[i].RemoveAt(band_prevInput[i].Count - 1);
            //}
            filtTemp[1] = currSamp[1];

            return filtTemp;
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

        public float[] envelopeSignals(float[] currSamp, int i)
		{
			// this is the low pass filter coefficient: 
			// y[n] = (b0 * x[n] + b1 * x[n-1]) / (1 + a1 * y[n-1])

			float[] filtTemp = new float[16];
            // for now, to save on computation time, set all non essential channels to 0 instead of having them go thru filtering
            for (int ch = 0; ch < 16; ch++)
            {
                filtTemp[ch] = 0f;
            }
   //         for (int i = 0; i < 16; i++)
			//{
			filtTemp[i] = low_gainVal * (low_b[0] * currSamp[i] + low_b[1] * low_prevInput[i][0] + low_b[2] * low_prevInput[i][1]) -
							(low_a[1] * low_prevFiltOut[i][0] + low_a[2] * low_prevFiltOut[i][1]);

			low_prevFiltOut[i].Insert(0, filtTemp[i]);
			low_prevFiltOut[i].RemoveAt(low_prevFiltOut[i].Count - 1);

			// Store most recent sample at the beginning of history window
			low_prevInput[i].Insert(0, currSamp[i]);
			low_prevInput[i].RemoveAt(low_prevInput[i].Count - 1);
			//}

			return filtTemp;
		}
	}
}
