using System;
using System.Collections.Generic;
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

namespace EvokedPotentialsApp
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>

    public partial class MainWindow : Window
    {
        private RealtimeGraphing.BICManager aBICManager;
        private bool phasicStimState = false;
        private bool openStimState = false;
        private int numChannels = 34;
        private Configuration configInfo;

        public class Channel
        {
            public string Name { get; set; }
            public bool IsSelected { get; set; }
        }

        private System.Timers.Timer neuroChartUpdateTimer;
        public List<Channel> channelList { get; set; }

        public class Configuration
        {
            public int stimChannel { get; set; } = 1; // remove default later
            public int returnChannel { get; set; } = 2; // remove default later
            public int numChannels { get; set; } = 30;
            public uint numPulses { get; set; } = 50;
            public uint stimPeriod { get; set; } = 1000000; // uS
            public int stimAmplitude { get; set; } = 5400; // uV
            public uint stimDuration { get; set; } = 250;
            public int jitterMax { get; set; } = 300; // uS // TODO: add jitter to pulse method (add to pause?) 
            public bool monopolar { get; set; } = false;
            public double stimThreshold { get; set; }


        }

        public MainWindow()
        {
            InitializeComponent();

            // Add items to check list box
            channelList = new List<Channel>();
            for (int i = 1; i < numChannels; i++)
            {
                if (i == 33)
                {
                    channelList.Add(new Channel { IsSelected = false, Name = "Filtered Channel" });
                }
                else
                {
                    channelList.Add(new Channel { IsSelected = false, Name = i.ToString() });
                }
            }
            this.DataContext = this;
            OutputConsole.Inlines.Add("Application started...\n");

        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            string seriesName;
            aBICManager = new RealtimeGraphing.BICManager(neuroStreamChart.Width);
            aBICManager.BICConnect();

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
            for (int i = 1; i < numChannels; i++)
            {
                if (i == 33)
                {
                    seriesName = "Filtered Channel";
                }
                else
                {
                    seriesName = "Channel " + i.ToString();
                }
                neuroStreamChart.Series.Add(
                new System.Windows.Forms.DataVisualization.Charting.Series
                {
                    Name = seriesName,
                    Color = colors_list[i - 1],
                    ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.FastLine
                });

                // when loading window, make legend invisible
                neuroStreamChart.Series[i - 1].IsVisibleInLegend = false;
            }

            neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    // disable buttons
                    btn_start.IsEnabled = false; // open loop stim button
                    btn_diagnostic.IsEnabled = false; // diagnostics button
                    btn_stop.IsEnabled = false; // stop stim button

                    // temporary- hide diagnostic buttons
                    btn_diagnostic.Visibility = Visibility.Hidden;
                }));

            // Start update timer
            neuroChartUpdateTimer = new System.Timers.Timer(200);
            neuroChartUpdateTimer.Elapsed += neuroChartUpdateTimer_Elapsed;
            neuroChartUpdateTimer.Start();
        }

        private void neuroChartUpdateTimer_Elapsed(object sender, ElapsedEventArgs e)
        {
            // grab latest data
            List<double>[] neuroData = aBICManager.getData();

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
                if (valConvert)
                {
                    selectedChannels.Add(chanVal);
                }
                else
                {
                    selectedChannels.Add(33);
                }
            }

            // update plot with newest data for selected channels
            for (int i = 0; i < selectedChannels.Count; i++)
            {
                if (selectedChannels[i] == 33)
                {
                    chanString = "Filtered Channel";
                }
                else
                {
                    chanString = "Channel " + selectedChannels[i].ToString();
                }
                neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    neuroStreamChart.Series[chanString].Points.DataBindY(neuroData[selectedChannels[i] - 1]);
                }));
            }
        }

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
                            //OutputConsole.Inlines.Add("Source channel: " + configInfo.sourceChannel + "\n");
                            //OutputConsole.Inlines.Add("Destination channel: " + configInfo.destChannel + "\n");
                            OutputConsole.Inlines.Add("Number of pulses: " + configInfo.numPulses + "\n");
                            OutputConsole.Inlines.Add("Stim period: " + (configInfo.stimPeriod) + " us\n");
                            OutputConsole.Inlines.Add("Stim Pulse Amplitude: " + configInfo.stimAmplitude + " uA\n");
                            OutputConsole.Inlines.Add("Random jitter between 0-: " + configInfo.jitterMax + " us\n");
                            //OutputConsole.Inlines.Add("Filter Coefficient [B]: ");
                            //for (int i = 0; i < configInfo.filterCoefficients_B.Count; i++)
                            //{
                            //    OutputConsole.Inlines.Add(configInfo.filterCoefficients_B[i] + " ");
                            //}
                            //OutputConsole.Inlines.Add("\nFilter Coefficients [A]: ");
                            //for (int i = 0; i < configInfo.filterCoefficients_A.Count; i++)
                            //{
                            //    OutputConsole.Inlines.Add(configInfo.filterCoefficients_A[i] + " ");
                            //}
                            OutputConsole.Inlines.Add("\n");
                            Scroller.ScrollToEnd();
                        }

                        neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                        delegate
                        {
                            // enable buttons after a config has been successfully loaded
                            btn_start.IsEnabled = true; // open loop stim button
                            btn_load.IsEnabled = true; // load config button
                            btn_diagnostic.IsEnabled = true; // diagnostics button
                            btn_stop.IsEnabled = true; // stop stim button
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
        private void btn_start_Click(object sender, RoutedEventArgs e)
        {
            ThreadPool.QueueUserWorkItem(a =>
            {
                // Keep the time for console output writring
                string timeStamp = DateTime.Now.ToString("h:mm:ss tt");

                // start open loop stim and update status
                try
                {
                    uint interPulseInterval = configInfo.stimPeriod - (5 * configInfo.stimDuration) - 3500; // worth revisiting the 3500
                    aBICManager.enableEvokedPotentialStimulation(configInfo.monopolar, (uint)configInfo.stimChannel - 1, (uint)configInfo.returnChannel - 1, configInfo.stimAmplitude, configInfo.stimDuration, 4, interPulseInterval, configInfo.stimThreshold, configInfo.numPulses);
                    //aBICManager.enableOpenLoopStimulation(true, configInfo.monopolar, (uint)configInfo.stimChannel - 1, (uint)configInfo.returnChannel - 1, configInfo.stimAmplitude, configInfo.stimDuration, 4, configInfo.stimPeriod - (5 * configInfo.stimDuration) - 3500, configInfo.stimThreshold);
                }
                catch
                {
                    // Exception occured, gRPC command did not succeed, do not update UI button elements
                    Application.Current.Dispatcher.Invoke(new Action(() =>
                    {
                        OutputConsole.Inlines.Add("Open loop stimulation NOT started: " + timeStamp + ", load new configuration\n");
                        Scroller.ScrollToEnd();
                    }));
                    return;
                }

                openStimState = true;

                // Succesfully enabled open loop stim, update UI elements
                neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    // disable buttons
                    btn_start.IsEnabled = false; // open loop stim button
                    btn_load.IsEnabled = false; // load config button
                    btn_diagnostic.IsEnabled = false; // diagnostics button
                }));

                // notify user of open loop stimulation starting
                Application.Current.Dispatcher.Invoke(new Action(() =>
                {
                    OutputConsole.Inlines.Add("Open loop stimulation started: " + timeStamp + "\n");
                    Scroller.ScrollToEnd();
                }));
            });
        }

        private void btn_start_Click(object sender, RoutedEventArgs e)
        { }

        private void btn_stop_Click(object sender, RoutedEventArgs e)
        {
            ThreadPool.QueueUserWorkItem(a =>
            {
                if (phasicStimState)
                {
                    // disable beta and open loop stim
                    aBICManager.enableDistributedStim(false, configInfo.monopolar, (uint)configInfo.stimChannel - 1, (uint)configInfo.returnChannel - 1, (uint)configInfo.senseChannel - 1, configInfo.stimAmplitude, configInfo.stimDuration, 4, configInfo.filterCoefficients_B, configInfo.filterCoefficients_A, configInfo.stimThreshold);
                    phasicStimState = false;
                }
                if (openStimState)
                {
                    aBICManager.enableOpenLoopStimulation(false, configInfo.monopolar, (uint)configInfo.stimChannel - 1, (uint)configInfo.returnChannel - 1, configInfo.stimAmplitude, configInfo.stimDuration, 1, 20000, configInfo.stimThreshold);
                    openStimState = false;
                }
            });

            // enable previously disabled buttons
            neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    btn_start.IsEnabled = true;
                    btn_load.IsEnabled = true;
                    btn_diagnostic.IsEnabled = true;
                }));

            string timeStamp = DateTime.Now.ToString("h:mm:ss tt");
            OutputConsole.Inlines.Add("All stimulation stopped: " + timeStamp + "\n");
            Scroller.ScrollToEnd();
        }

        private void btn_diagnostic_Click(object sender, RoutedEventArgs e)
        {
            // start diagnostic pattern
            OutputConsole.Inlines.Add("Performing diagnostics...\n");
            Scroller.ScrollToEnd();
        }

        private void ListBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            OutputConsole.Inlines.Add("Channel was selected/de-selected"); // not going through this function.. selecting doesn't invoke this function 
            Scroller.ScrollToEnd();

            // grab the most recent data
            List<double>[] neuroData = aBICManager.getData();

            // look for the selected items in the listbox
            List<int> selectedChannels = new List<int>();
            string chanString = "";
            int chanVal;
            bool valConvert = false;

            // get a list of selected channels
            var selected = from item in channelList
                           where item.IsSelected == true
                           select item.Name.ToString();

            // get list of selected channels and convert from string to int type
            foreach (String item in selected)
            {
                valConvert = Int32.TryParse(item, out chanVal);
                if (valConvert)
                {
                    selectedChannels.Add(chanVal);
                }
                else
                {
                    selectedChannels.Add(33);
                }
            }

            // clear the current legend
            neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
            delegate
            {
                foreach (var series in neuroStreamChart.Series)
                {
                    series.IsVisibleInLegend = false;
                    series.Enabled = false;
                }
            }));

            // Plot newly selected channels and show new legend
            for (int i = 0; i < selectedChannels.Count; i++)
            {
                chanString = "Channel " + selectedChannels[i].ToString();
                neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    neuroStreamChart.Series[chanString].Points.DataBindY(neuroData[selectedChannels[i] - 1]);
                    neuroStreamChart.Series[chanString].IsVisibleInLegend = true;
                    neuroStreamChart.Series[chanString].Enabled = true;
                }));
            }
        }
        private void MainWindow_Closed(object sender, EventArgs e)
        {
            neuroChartUpdateTimer.Dispose();
            aBICManager.Dispose();
        }

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
                if (valConvert)
                {
                    selectedChannels.Add(chanVal);
                }
                else
                {
                    selectedChannels.Add(33);
                }
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

            // update legend for newest selection of channels
            for (int i = 0; i < selectedChannels.Count; i++)
            {
                if (selectedChannels[i] == 33)
                {
                    chanString = "Filtered Channel";
                }
                else
                {
                    chanString = "Channel " + selectedChannels[i].ToString();
                }
                neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    neuroStreamChart.Series[chanString].IsVisibleInLegend = true;
                    neuroStreamChart.Series[chanString].Enabled = true;
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