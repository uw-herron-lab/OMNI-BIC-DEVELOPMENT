using MotorEvokedPotentialsApp.Properties;
using System;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using System.Timers;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Forms.DataVisualization;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace MotorEvokedPotentialsApp
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>

    public partial class MainWindow : Window
    {
        private MotorEvokedPotentialsApp.BICManagerMEP aBICManagerMEP;
        private int numChannels = 32;
        private Configuration configInfo;
        private int stimChannel = 1;
        private int returnChannel = 9;
        private uint numPulses = 10;                    // number of pulses per train to deliver
        private uint numTrains = 10;                    // number of trains to deliver
        private uint interPulseInterval = 2000;         // time interval between pulses [us]
        private uint interTrainInterval = 100000;       // time interval between trains [us]
        private uint baselinePeriod = 100000;           // pre-stimulus period for calculating average [us]
        private int stimAmplitude = -3000;              // pulse amplitude of initial phase of stimulation pulse [uA]
        private uint stimDuration = 250;                // pulse duration of initial phase of stimulation pulse [us]
        private int jitterMax = 300000;                 // upper limit of jitter [us]
        private bool monopolar = false;                 // stimulation configuration
        private double stimThreshold = 100;             // threshold for determining stimulation onset for calculating average [uV]
        private bool connectState = false;

        private bool stopStimClicked = false; // set to true when stop is clicked. Flag to exit from for loops that would send more stimulation
        private CancellationTokenSource cancellationTokenSource;

        public class Channel
        {
            public string Name { get; set; }
            public bool IsSelected { get; set; }
        }

        // Random class used for jitter
        private static readonly Random random = new Random();
        private static readonly object syncLock = new object();
        public static int RandomNumber(int min, int max)
        {
            lock (syncLock)
            {
                return random.Next(min, max);
            }
        }

        private System.Timers.Timer neuroChartUpdateTimer;
        public List<Channel> channelList { get; set; }
        public List<int> channelNumList { get; set; }

        public class Configuration
        {
            public int numChannels { get; set; }
            public int stimChannel { get; set; }
            public int returnChannel { get; set; }
            public uint numPulses { get; set; }
            public uint numTrains { get; set; }
            public uint interPulseInterval { get; set; }
            public uint interTrainInterval { get; set; }
            public uint baselinePeriod { get; set; }
            public int stimAmplitude { get; set; } // uV
            public uint stimDuration { get; set; }
            public int jitterMax { get; set; } // uS
            public bool monopolar { get; set; }
            public double stimThreshold { get; set; }
        }

        public MainWindow()
        {
            InitializeComponent();

            // Add items to check list box
            channelList = new List<Channel>();
            channelNumList = new List<int>();

            for (int i = 1; i <= numChannels; i++)
            {
                channelList.Add(new Channel { IsSelected = false, Name = i.ToString() });
                channelNumList.Add(i);
            }

            this.DataContext = this;
            OutputConsole.Inlines.Add("Application started...\n");
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            string seriesName;
            aBICManagerMEP = new MotorEvokedPotentialsApp.BICManagerMEP(neuroStreamChart.Width, interTrainInterval, baselinePeriod); // initialize buffer to the width we need! or maybe whichever is bigger. might need to do something later for the display, if buffer is diff length
            aBICManagerMEP.BICConnect();

            var colors_list = new System.Drawing.Color[]
            {
                System.Drawing.Color.Blue,
                System.Drawing.Color.Red,
                System.Drawing.Color.Green,
                System.Drawing.Color.Aqua,
                System.Drawing.Color.DarkOrchid,
                System.Drawing.Color.DeepPink,
                System.Drawing.Color.Orange,
                System.Drawing.Color.Plum,
                System.Drawing.Color.LightSteelBlue,
                System.Drawing.Color.Maroon,
                System.Drawing.Color.OrangeRed,
                System.Drawing.Color.Gold,
                System.Drawing.Color.LightCoral,
                System.Drawing.Color.Gray,
                System.Drawing.Color.Navy,
                System.Drawing.Color.OliveDrab,
                System.Drawing.Color.Salmon,
                System.Drawing.Color.SandyBrown,
                System.Drawing.Color.Magenta,
                System.Drawing.Color.Purple,
                System.Drawing.Color.RoyalBlue,
                System.Drawing.Color.Black,
                System.Drawing.Color.CadetBlue,
                System.Drawing.Color.Crimson,
                System.Drawing.Color.DarkCyan,
                System.Drawing.Color.DarkMagenta,
                System.Drawing.Color.Tan,
                System.Drawing.Color.Tomato,
                System.Drawing.Color.ForestGreen,
                System.Drawing.Color.RosyBrown,
                System.Drawing.Color.MediumPurple,
                System.Drawing.Color.Sienna,
                System.Drawing.Color.Indigo
            };

            neuroStreamChart.Series.Clear();
            avgsChart.Series.Clear();
            for (int i = 1; i <= numChannels; i++)
            {
                seriesName = "Channel " + i.ToString();
                neuroStreamChart.Series.Add(
                new System.Windows.Forms.DataVisualization.Charting.Series
                {
                    Name = seriesName,
                    Color = colors_list[i - 1],
                    ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.FastLine
                });

                avgsChart.Series.Add(
                new System.Windows.Forms.DataVisualization.Charting.Series
                {
                    Name = seriesName,
                    Color = colors_list[i - 1],
                    ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.FastLine
                });

                // when loading window, make legend invisible
                neuroStreamChart.Series[i - 1].IsVisibleInLegend = false;
                avgsChart.Series[i - 1].IsVisibleInLegend = false;
            }

            neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    // disable buttons
                    btn_start.IsEnabled = false; // open loop stim button
                    btn_stop.IsEnabled = false;
                }));

            // Start update timer
            neuroChartUpdateTimer = new System.Timers.Timer(200);
            neuroChartUpdateTimer.Elapsed += neuroChartUpdateTimer_Elapsed;
            neuroChartUpdateTimer.Start();

            aBICManagerMEP.disconnected += onDisconnected;
        }

        private void neuroChartUpdateTimer_Elapsed(object sender, ElapsedEventArgs e)
        {
            // grab latest data
            List<double>[] neuroData = aBICManagerMEP.getData();

            // look for the selected items in the listbox
            List<int> selectedChannels = new List<int>();
            string chanString = "";
            int chanVal;
            bool valConvert = false;

            // get a list of selected channels
            var selected = from item in channelList
                           where item.IsSelected == true
                           select item.Name.ToString();

            // convert from string to int type
            foreach (String item in selected)
            {
                valConvert = Int32.TryParse(item, out chanVal);
                selectedChannels.Add(chanVal);
            }

            // update plot with newest data for selected channels
            for (int i = 0; i < selectedChannels.Count; i++)
            {
                chanString = "Channel " + selectedChannels[i].ToString();
                neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    neuroStreamChart.Series[chanString].Points.DataBindY(neuroData[selectedChannels[i] - 1]);
                }));
            }
        }

        /// <summary>
        /// Plot latest data in running averages chart.
        /// </summary>
        private void updateAvgsChart()
        {
            // grab latest data
            List<double>[] avgsData = aBICManagerMEP.getAvgsData();

            // look for the selected items in the listbox
            List<int> selectedChannels = new List<int>();
            string chanString = "";
            int chanVal;
            bool valConvert = false;

            // get a list of selected channels
            var selected = from item in channelList
                           where item.IsSelected == true
                           select item.Name.ToString();

            // convert from string to int type
            foreach (String item in selected)
            {
                valConvert = Int32.TryParse(item, out chanVal);
                selectedChannels.Add(chanVal);
            }

            // update plot with newest data for selected channels
            for (int i = 0; i < selectedChannels.Count; i++)
            {
                chanString = "Channel " + selectedChannels[i].ToString();
                avgsChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    avgsChart.Series[chanString].Points.DataBindY(avgsData[selectedChannels[i] - 1]);
                }));
            }
        }

        private void onDisconnected(List<string> connectionUpdate)
        {
            if (connectionUpdate.Any())
            {
                ThreadPool.QueueUserWorkItem(a =>
                {
                    Application.Current.Dispatcher.Invoke(new Action(() =>
                    {
                        // Notify user about disconnection event
                        OutputConsole.Inlines.Add("Disconnection at " + connectionUpdate[0] + " connection. ");
                        OutputConsole.Inlines.Add("Check connections then use the 'Reconnect' button to reestablish connection!\n");
                        connectState = false;
                        Scroller.ScrollToEnd();
                    }));
                    return;
                });
            }
        }
        /// <summary>
        /// Read in config file
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btn_load_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                // open dialog box to select file with patient-specific settings
                var fileD = new Microsoft.Win32.OpenFileDialog();
                bool? loadFile = fileD.ShowDialog();
                if (loadFile == true)
                {
                    string fileName = fileD.FileName;
                    if (File.Exists(fileName))
                    {
                        // load in .json file and read in stimulation parameters
                        using (StreamReader fileReader = new StreamReader(fileName))
                        {
                            string configJson = fileReader.ReadToEnd();
                            configInfo = System.Text.Json.JsonSerializer.Deserialize<Configuration>(configJson);

                            OutputConsole.Inlines.Add("Loaded " + fileName + "\n");
                            OutputConsole.Inlines.Add("Stim channel: " + String.Join(", ", configInfo.stimChannel) + "\n");
                            OutputConsole.Inlines.Add("Return channel: " + String.Join(", ", configInfo.returnChannel) + "\n");
                            OutputConsole.Inlines.Add("Number of pulses: " + configInfo.numPulses + "\n");
                            OutputConsole.Inlines.Add("Number of trains: " + configInfo.numTrains + "\n");
                            OutputConsole.Inlines.Add("Interpulse interval: " + configInfo.interPulseInterval + "\n");
                            OutputConsole.Inlines.Add("Intertrain interval: " + configInfo.interTrainInterval + "\n");
                            OutputConsole.Inlines.Add("Stim duration: " + (configInfo.stimDuration) + " us\n");
                            OutputConsole.Inlines.Add("Stim amplitude: " + configInfo.stimAmplitude + " uA\n");
                            OutputConsole.Inlines.Add("Stim configuration: " + configInfo.monopolar + "\n");
                            Scroller.ScrollToEnd();
                        }

                        stimChannel = configInfo.stimChannel;
                        returnChannel = configInfo.returnChannel;
                        numPulses = configInfo.numPulses;
                        numTrains = configInfo.numTrains;
                        interPulseInterval = configInfo.interPulseInterval;
                        interTrainInterval = configInfo.interTrainInterval;
                        baselinePeriod = configInfo.baselinePeriod;
                        stimAmplitude = configInfo.stimAmplitude;
                        stimDuration = configInfo.stimDuration;
                        jitterMax = configInfo.jitterMax;
                        monopolar = configInfo.monopolar;
                        stimThreshold = configInfo.stimThreshold;

                        neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                            delegate
                            {
                                btn_load.IsEnabled = true;
                            }));
                    }
                }
            }
            catch (Exception theException)
            {
                OutputConsole.Inlines.Add("Unable to load selected file\n");
                OutputConsole.Inlines.Add("Error encoutnered: " + theException.Message + "\n");
                OutputConsole.Inlines.Add("\n");
                Scroller.ScrollToEnd();
            }
        }

        /// <summary>
        /// Send stimulation for all queued source/destination channels, and update UI
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void btn_start_Click(object sender, RoutedEventArgs e)
        {
            cancellationTokenSource = new CancellationTokenSource();

            stopStimClicked = false;

            ThreadPool.QueueUserWorkItem(a =>
                {
                    // Keep the time for console output writing
                    string timeStamp = DateTime.Now.ToString("h:mm:ss tt");

                    // start stim and update status
                    try
                    {
                        enableEvokedPotentialPulses(monopolar, (uint)stimChannel, (uint)returnChannel, stimAmplitude, stimDuration, 4, interPulseInterval, stimThreshold, numPulses, jitterMax);
                    }
                    catch
                    {
                        // Exception occured, gRPC command did not succeed, do not update UI button elements
                        Application.Current.Dispatcher.Invoke(new Action(() =>
                        {
                            OutputConsole.Inlines.Add("Evoked Potential stimulation NOT started: " + timeStamp + ", load new configuration\n");
                            Scroller.ScrollToEnd();
                        }));
                        return;
                    }

                    /*// update UI on first iteration
                    if (i == 0)
                    {
                        // Succesfully enabled stim, update UI elements
                        neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                        delegate
                        {
                            // disable buttons
                            btn_start.IsEnabled = false; // stim button
                            btn_load.IsEnabled = false; // load config button
                            btn_stop.IsEnabled = true;
                        }));
                    }*/

                    // notify user of stimulation starting
                    Application.Current.Dispatcher.Invoke(new Action(() =>
                    {
                        OutputConsole.Inlines.Add("Evoked potential stimulation started: " + timeStamp + "\n");
                        OutputConsole.Inlines.Add("Source channel: " + stimChannel + "\nDestination channel: " + returnChannel + "\n");
                        Scroller.ScrollToEnd();
                    }));

                });

            // Wait until end of condition before continuing to next condition (waiting for numPulses pulses for current source/destination condition), or wait until Stop is clicked.
            try
            {
                await Task.Delay((int)((interTrainInterval + jitterMax) / 1000 * numPulses), cancellationTokenSource.Token);
            }
            catch (TaskCanceledException)
            {
                // Cancelled if stop_stim_clicked
                return;
            }

            
            // notify user and update UI
            Application.Current.Dispatcher.Invoke(new Action(() =>
            {
                OutputConsole.Inlines.Add("Evoked potential stimulation completed.\n");
                Scroller.ScrollToEnd();
            }));
            stop_stim_UI_update();
        }

        /// <summary>
        /// Send all numPulses stimulation pulses for a single source/destination channel condition
        /// </summary>
        /// <param name="monopolar"></param>
        /// <param name="stimChannel"></param>
        /// <param name="returnChannel"></param>
        /// <param name="stimAmplitude"></param>
        /// <param name="stimDuration"></param>
        /// <param name="chargeBalancePWRatio"></param>
        /// <param name="interPulseInterval"></param>
        /// <param name="stimThreshold"></param>
        /// <param name="numPulses"></param>
        /// <param name="jitterMax"></param>
        private async void enableEvokedPotentialPulses(bool monopolar, uint stimChannel, uint returnChannel, double stimAmplitude, uint stimDuration, uint chargeBalancePWRatio, uint interPulseInterval, double stimThreshold, uint numPulses, int jitterMax)
        {
            // clear running totals and num pulses before starting each condition
            aBICManagerMEP.zeroAvgsBuffers();
            aBICManagerMEP.currNumPulses = 0;

            for (int i = 0; i < numPulses; i++)
            {
                if (stopStimClicked)
                {
                    return;
                }
                else
                {
                    aBICManagerMEP.enableStimulationPulse(monopolar, stimChannel - 1, returnChannel - 1, numPulses, numTrains, interPulseInterval, interTrainInterval, stimAmplitude, stimDuration, 4, stimThreshold);

                    int randomJitter = RandomNumber(0, jitterMax);
                    await Task.Delay((int)((interTrainInterval + randomJitter) / 1000));

                    // Increment currNumPulses and update avgs chart after pulse completed
                    aBICManagerMEP.currNumPulses++;
                    updateAvgsChart();
                }
            }
        }

        /// <summary>
        /// Enable/disable relevant buttons when stimulation completes.
        /// </summary>
        private void stop_stim_UI_update()
        {
            neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                   delegate
                   {
                       btn_start.IsEnabled = true;
                       btn_stop.IsEnabled = false;
                       btn_load.IsEnabled = true;
                   }));
        }

        /// <summary>
        /// When stop is clicked, set stopStimClicked flag to true, cancel the timer for the end of the condition, 
        /// update UI.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btn_stop_Click(object sender, RoutedEventArgs e)
        {
            stopStimClicked = true; // We use this flag to stop sending pulses for current condition, and to stop from cycling to next condition. 
            aBICManagerMEP.stopEvokedPotentialStimulation();
            cancellationTokenSource.Cancel();
            string timeStamp = DateTime.Now.ToString("h:mm:ss tt");
            Application.Current.Dispatcher.Invoke(new Action(() =>
            {
                OutputConsole.Inlines.Add("Evoked potential stimulation cancelled at " + timeStamp + "\n");
                Scroller.ScrollToEnd();
            }));
            stop_stim_UI_update();
        }

        private void btn_disconnect_Click(object sender, RoutedEventArgs e)
        {
            neuroChartUpdateTimer.Stop();

            OutputConsole.Inlines.Add("Disconnecting...\n");
            aBICManagerMEP.Dispose();  // Shut down connection
            connectState = false;
            OutputConsole.Inlines.Add("Disconnection successful!\n");
        }

        private void btn_reconnect_Click(object sender, RoutedEventArgs e)
        {
            OutputConsole.Inlines.Add("Reconnecting...\n");
            aBICManagerMEP = new MotorEvokedPotentialsApp.BICManagerMEP(neuroStreamChart.Width, interTrainInterval, baselinePeriod);
            connectState = aBICManagerMEP.BICConnect();    // Try to reestablish connection

            if (connectState)
            {
                OutputConsole.Inlines.Add("Reconnection successful!\n");
            }
            else
            {
                OutputConsole.Inlines.Add("Reconnection unsuccessful!\n");
            }
            neuroChartUpdateTimer.Start();
            aBICManagerMEP.disconnected += onDisconnected;
        }

        private void MainWindow_Closed(object sender, EventArgs e)
        {
            neuroChartUpdateTimer.Dispose();
            aBICManagerMEP.Dispose();
        }

        /// <summary>
        /// When channels are selected/deselected, update display
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void CheckBox_Changed(object sender, RoutedEventArgs e)
        {
            // look for the selected items in the listbox
            List<int> selectedChannels = new List<int>();
            string chanString = "";
            int chanVal;
            bool valConvert = false;

            // get a list of selected channels
            var selected = from item in channelList
                           where item.IsSelected == true
                           select item.Name.ToString();

            // convert from string to int type
            foreach (String item in selected)
            {
                valConvert = Int32.TryParse(item, out chanVal);
                selectedChannels.Add(chanVal);
            }

            // reset current legend
            neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
            delegate
            {
                foreach (var series in neuroStreamChart.Series)
                {
                    series.IsVisibleInLegend = false;
                    series.Enabled = false;
                }
            }));

            avgsChart.Invoke(new System.Windows.Forms.MethodInvoker(
            delegate
            {
                foreach (var series in avgsChart.Series)
                {
                    series.IsVisibleInLegend = false;
                    series.Enabled = false;
                }
            }));

            // update legend for data streaming chart for newest selection of channels
            for (int i = 0; i < selectedChannels.Count; i++)
            {
                chanString = "Channel " + selectedChannels[i].ToString();
                neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    neuroStreamChart.Series[chanString].IsVisibleInLegend = true;
                    neuroStreamChart.Series[chanString].Enabled = true;
                }));
            }

            // update legend for averages chart with newest selection of channels
            for (int i = 0; i < selectedChannels.Count; i++)
            {
                chanString = "Channel " + selectedChannels[i].ToString();
                avgsChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    avgsChart.Series[chanString].IsVisibleInLegend = true;
                    avgsChart.Series[chanString].Enabled = true;
                }));
            }
        }

        private void y_min_TextChanged(object sender, TextChangedEventArgs e)
        {
            double yMinVal = 0;
            bool valEntry = double.TryParse(y_min.Text, out yMinVal);
            if (valEntry)
            {
                if (yMinVal < neuroStreamChart.ChartAreas[0].AxisY.Maximum)
                {
                    neuroStreamChart.ChartAreas[0].AxisY.Minimum = yMinVal;
                }
            }
        }

        private void y_max_TextChanged(object sender, TextChangedEventArgs e)
        {
            double yMaxVal = 0;
            bool valEntry = double.TryParse(y_max.Text, out yMaxVal);
            if (valEntry)
            {
                if (yMaxVal > neuroStreamChart.ChartAreas[0].AxisY.Minimum)
                {
                    neuroStreamChart.ChartAreas[0].AxisY.Maximum = yMaxVal;
                }
            }
        }
    }
}