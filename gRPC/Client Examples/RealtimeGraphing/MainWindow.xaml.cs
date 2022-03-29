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

namespace RealtimeGraphing
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private BICManager aBICManager;
        private Timer graphUpdateTimer;

        public MainWindow()
        {
            InitializeComponent();
        }

        public void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            aBICManager = new BICManager(neuroDataChart.Width);
            aBICManager.BICConnect();

            // Update the chart controls
            neuroDataChart.Series.Clear();
            neuroDataChart.Series.Add(
                new System.Windows.Forms.DataVisualization.Charting.Series
                {
                    Name = "Channel 1",
                    Color = System.Drawing.Color.Blue,
                    ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.FastLine
                });
            /**
            neuroDataChart.Series.Add(
                new System.Windows.Forms.DataVisualization.Charting.Series
                {
                    Name = "Channel 2",
                    Color = System.Drawing.Color.Red,
                    ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.FastLine
                });
            neuroDataChart.Series.Add(
                new System.Windows.Forms.DataVisualization.Charting.Series
                {
                    Name = "Channel 3",
                    Color = System.Drawing.Color.Green,
                    ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.Line
                });
            **/
            neuroDataChart.Series.Add(
                new System.Windows.Forms.DataVisualization.Charting.Series
                {
                    Name = "Filtered Channel",
                    Color = System.Drawing.Color.Black,
                    ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.Line
                });
            neuroDataChart.Series.Add(
                new System.Windows.Forms.DataVisualization.Charting.Series
                {
                    Name = "Stim Channel",
                    Color = System.Drawing.Color.DarkOrange,
                    ChartType = System.Windows.Forms.DataVisualization.Charting.SeriesChartType.Line
                });

            // Start the update timer
            graphUpdateTimer = new Timer(200);
            graphUpdateTimer.Elapsed += graphUpdateTimer_Elapsed;
            graphUpdateTimer.Start();
        }

        private void graphUpdateTimer_Elapsed(object sender, ElapsedEventArgs e)
        {
            List<double>[] theData = aBICManager.getData();

            // Clear the chart to prep it for updating
            neuroDataChart.Invoke(new System.Windows.Forms.MethodInvoker(
                delegate { 
                    neuroDataChart.Series[0].Points.DataBindY(theData[0]);
                    neuroDataChart.Series[1].Points.DataBindY(theData[1]);
                    neuroDataChart.Series[2].Points.DataBindY(theData[2]);
                    neuroDataChart.Series[3].Points.DataBindY(theData[32]); // addition of filtered data
                    neuroDataChart.Series[4].Points.DataBindY(theData[5]); // addition of stim channel
                }));
        }

        private void MainWindow_Closed(object sender, EventArgs e)
        {
            graphUpdateTimer.Dispose();
            aBICManager.Dispose();
        }
    }
}
