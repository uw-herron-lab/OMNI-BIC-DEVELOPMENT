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
            neuroDataChart.Series.Add(
                new System.Windows.Forms.DataVisualization.Charting.Series
                {
                    Name = "Channel 1"
                });

            // Start the update timer
            graphUpdateTimer = new Timer(500);
            graphUpdateTimer.Elapsed += graphUpdateTimer_Elapsed;
            graphUpdateTimer.Start();
        }

        private void graphUpdateTimer_Elapsed(object sender, ElapsedEventArgs e)
        {
            List<double>[] theData = aBICManager.getData();

            // Clear the chart to prep it for updating
            neuroDataChart.Invoke(new System.Windows.Forms.MethodInvoker(delegate { neuroDataChart.Series[0].Points.DataBindY(theData[2]); }));
        }

        private void MainWindow_Closed(object sender, EventArgs e)
        {
            graphUpdateTimer.Dispose();
            aBICManager.Dispose();
        }
    }
}
