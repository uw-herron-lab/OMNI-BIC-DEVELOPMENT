using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Forms.DataVisualization;
using System.Timers;
using System.Threading;

namespace StimTherapyApp
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>

    public partial class MainWindow : Window
    {
        private RealtimeGraphing.BICManager aBICManager;
        //private bool phasicStimState = false;
        //private bool openStimState = false;
        private uint userStimChannel;
        private uint userSenseChannel;
        private int numChannels = 34;
        public class Channel
        {
            public string Name { get; set; }
            public bool IsSelected { get; set; }
        }

        private System.Timers.Timer neuroChartUpdateTimer;
        public List<Channel> channelList { get; set; }

        public MainWindow()
        {
            InitializeComponent();

            // Add items to check list box
            channelList = new List<Channel>();
            for (int i = 1; i < numChannels; i++)
            {
                if ( i == 33)
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
                    Color = colors_list[i-1],
                    ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.FastLine
                });;
                // when loading window, make legend invisible
                neuroStreamChart.Series[i-1].IsVisibleInLegend = false;
            }

            neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    // disable buttons
                    btn_beta.IsEnabled = false; // beta stim button; have to use method invoker
                    btn_openloop.IsEnabled = false; // open loop stim button
                    btn_diagnostic.IsEnabled = false; // diagnostics button
                    btn_stop.IsEnabled = false; // stop stim button

                    // temporary- hide open loop and diagnostic buttons
                    btn_openloop.Visibility = Visibility.Hidden;
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
            
            // open dialog box to select file with patient-specific settings
            var fileD = new Microsoft.Win32.OpenFileDialog();
            bool? loadFile = fileD.ShowDialog();
            if (loadFile == true)
            {
                // grab the path of the selected file
                string fileName = fileD.FileName;
                // check that the file exists
                if (File.Exists(fileName))
                {
                    int senseChannelIndex = 1;
                    int stimChannelIndex = 2;

                    String[] lines = File.ReadAllLines(fileName);
                    // skip the header/labels of each column
                    lines = lines.Skip(1).ToArray();
                    // grab first string- corresponding to first row of info (i.e. stim type,stim channel,sense channel,amplitude,frequency)
                    string test = lines[0];
                    string[] testlist = test.Split(','); // split based off commas
                    // save stimulation and sensing info
                    userStimChannel = UInt32.Parse(testlist[stimChannelIndex]);
                    userSenseChannel = UInt32.Parse(testlist[senseChannelIndex]);

                    // read in CSV file and get the type of stim and stimulation and sensing channel of choice
                    // let user know a file has been loaded and what the chosen stimulation and sensing channels are
                    OutputConsole.Inlines.Add("Loaded " + fileName + "\n");
                    OutputConsole.Inlines.Add("Stim Channel: " + userStimChannel + "\n");
                    OutputConsole.Inlines.Add("Sense Channel: " + userSenseChannel + "\n");
                    Scroller.ScrollToEnd();
                }
                neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    // enable buttons after a config has been successfully loaded
                    btn_beta.IsEnabled = true; // beta stim button; have to use method invoker
                    btn_openloop.IsEnabled = true; // open loop stim button
                    btn_load.IsEnabled = true; // load config button
                    btn_diagnostic.IsEnabled = true; // diagnostics button
                    btn_stop.IsEnabled = true; // stop stim button
                }));
            }
        }
        private void btn_beta_Click(object sender, RoutedEventArgs e)
        {
            ThreadPool.QueueUserWorkItem(a =>
            {
                // Keep the time for console output writring
                string timeStamp = DateTime.Now.ToString("h:mm:ss tt");

                // start phase triggered stim and update status
                try
                {
                    aBICManager.enableDistributedStim(true, userStimChannel - 1, userSenseChannel - 1);
                }
                catch
                {
                    // Exception occured, gRPC command did not succeed, do not update UI button elements
                    Application.Current.Dispatcher.Invoke(new Action(() =>
                    {
                        OutputConsole.Inlines.Add("Beta triggered stimulation NOT started: " + timeStamp + ", load new configuration\n");
                        Scroller.ScrollToEnd();
                    }));
                    return;
                }

                // Succesfully enabled distributed, update UI elements
                neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    // disable buttons
                    btn_beta.IsEnabled = false; // beta stim button; have to use method invoker
                    btn_openloop.IsEnabled = false; // open loop stim button
                    btn_load.IsEnabled = false; // load config button
                    btn_diagnostic.IsEnabled = false; // diagnostics button
                }));

                // notify user of beta stimulation starting
                Application.Current.Dispatcher.Invoke(new Action(() =>
                {
                    OutputConsole.Inlines.Add("Beta triggered stimulation started: " + timeStamp + "\n");
                    Scroller.ScrollToEnd();
                }));
            });
        }

        private void btn_openloop_Click(object sender, RoutedEventArgs e)
        {
            ThreadPool.QueueUserWorkItem(a =>
            {
                // start phase triggered stim and update status
                //openStimState = true;

                neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    // disable buttons
                    btn_beta.IsEnabled = false; // beta stim button; have to use method invoker
                    btn_openloop.IsEnabled = false; // open loop stim button
                    btn_load.IsEnabled = false; // load config button
                    btn_diagnostic.IsEnabled = false; // diagnostics button
                }));
            });

            // notify user of open loop stim starting
            string timeStamp = DateTime.Now.ToString("h:mm:ss tt");
            OutputConsole.Inlines.Add("Open loop stimulation started: " + timeStamp + "\n");
            Scroller.ScrollToEnd();
        }

        private void btn_stop_Click(object sender, RoutedEventArgs e)
        {
            ThreadPool.QueueUserWorkItem(a =>
            {
                // disable beta and open loop stim
                aBICManager.enableDistributedStim(false, userStimChannel-1, userSenseChannel-1);

                // update stim statuses
                //phasicStimState = false;
                //openStimState = false;
            });

            // enable previously disabled buttons
            neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    btn_beta.IsEnabled = true;
                    btn_openloop.IsEnabled = true;
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

            Console.WriteLine(yMinVal);
            if (yMinVal < neuroStreamChart.ChartAreas[0].AxisX.Maximum)
            {
                neuroStreamChart.ChartAreas[0].AxisY.Minimum = yMinVal;
            }
        }

        private void y_max_TextChanged(object sender, TextChangedEventArgs e)
        {
            double yMaxVal = 0;
            bool valEntry = double.TryParse(y_max.Text, out yMaxVal);
            Console.WriteLine(yMaxVal);
            if (yMaxVal > neuroStreamChart.ChartAreas[0].AxisX.Minimum)
            {
                neuroStreamChart.ChartAreas[0].AxisY.Maximum = yMaxVal;
            }
        }
    }
}