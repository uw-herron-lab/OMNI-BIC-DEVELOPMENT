using System;
using System.Collections.Generic;
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
        private bool phasicStimState = false;
        private bool openStimState = false;
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
            for (int i = 1; i < 33; i++)
            {
                channelList.Add(new Channel { IsSelected = false, Name = i.ToString() });
            }
            this.DataContext = this;
            OutputConsole.Inlines.Add("Application started...\n");
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            aBICManager = new RealtimeGraphing.BICManager(neuroStreamChart.Width);
            aBICManager.BICConnect();

            neuroStreamChart.Series.Clear();
            for (int i = 1; i < 33; i++)
            {
                neuroStreamChart.Series.Add(
                new System.Windows.Forms.DataVisualization.Charting.Series
                {
                    Name = "Channel " + i.ToString(),
                    //Color = System.Drawing.Color.Blue,
                    ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.FastLine
                });
            }

            // Start update timer
            neuroChartUpdateTimer = new System.Timers.Timer(100);
            neuroChartUpdateTimer.Elapsed += neuroChartUpdateTimer_Elapsed;
            neuroChartUpdateTimer.Start();
        }

        private void neuroChartUpdateTimer_Elapsed(object sender, ElapsedEventArgs e)
        {
            List<double>[] neuroData = aBICManager.getData();

            // look for the selected items in the listbox
            List<int> selectedChannels = new List<int>();
            string chanString = "";
            // get a list of selected channels
            var selected = from item in channelList
                           where item.IsSelected == true
                           select item.Name.ToString();

            // convert from string to int type
            foreach (String item in selected)
                selectedChannels.Add(Int32.Parse(item)); // create int list of selected channels

            neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
            delegate
            {
                foreach (var series in neuroStreamChart.Series)
                {
                    series.IsVisibleInLegend = false;
                    series.Enabled = false;
                }
            }));
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

        private void btn_load_Click(object sender, RoutedEventArgs e)
        {
            // open dialog box to select file with patient-specific settings
            var fileD = new Microsoft.Win32.OpenFileDialog();
            bool? loadFile = fileD.ShowDialog();
            if (loadFile == true)
            {
                // let user know a file has been loaded
                string fileName = fileD.FileName;
                OutputConsole.Inlines.Add("Loaded " + fileName + "\n");
                Scroller.ScrollToEnd();
            }

        }
        private void btn_beta_Click(object sender, RoutedEventArgs e)
        {
            ThreadPool.QueueUserWorkItem(a =>
            {
                // start phase triggered stim and update status
                aBICManager.enableDistributedStim(true);
                phasicStimState = true;

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
            // notify user of beta stimulation starting
            string timeStamp = DateTime.Now.ToString("h:mm:ss tt");
            OutputConsole.Inlines.Add("Beta triggered stimulation started: " + timeStamp + "\n");
            Scroller.ScrollToEnd();
        }

        private void btn_openloop_Click(object sender, RoutedEventArgs e)
        {
            ThreadPool.QueueUserWorkItem(a =>
            {
                // start phase triggered stim and update status
                openStimState = true;

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
                aBICManager.enableDistributedStim(false);

                // update stim statuses
                phasicStimState = false;
                openStimState = false;

                neuroStreamChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate
                {
                    // enable previously disabled buttons
                    btn_beta.IsEnabled = true;
                    btn_openloop.IsEnabled = true;
                    btn_load.IsEnabled = true;
                    btn_diagnostic.IsEnabled = true;
                }));
            });
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
    }
}