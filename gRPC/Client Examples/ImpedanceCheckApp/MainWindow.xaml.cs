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

namespace ImpedanceCheckApp
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>

    public partial class MainWindow : Window
    {
        private ImpedanceCheckApp.ImpedanceBICManager impBICManager;
        private bool connectState = false;

        public MainWindow()
        {
            InitializeComponent();
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            // Establish connection
            impBICManager = new ImpedanceCheckApp.ImpedanceBICManager((int)MainWindow1.Width); // additional parameter that is set to default otherwise?
            connectState = impBICManager.BICConnect();
        }

        private void MainWindow_Closed(object sender, EventArgs e)
        {
            impBICManager.Dispose();
        }

        private void btn_impcheck_Click(object sender, RoutedEventArgs e)
        {
            // Run impedance check
            runImpCheck(connectState);
        }
        private void runImpCheck(bool currConnectState)
        {
            if (currConnectState)
            {
                // Get timestamp
                string timestamp = DateTime.Now.ToString("hh:mm:ss tt");
                impBICManager.performImpCheck();
                List<string> impValues = impBICManager.getImpedances();
                
                // Display impedances to the console window
                ImpedanceOutputConsole.Inlines.Add("Impedance check at: " + timestamp + "\n");
                
                string impedEntry = "";
                for (int channelNum = 0; channelNum < impValues.Count; channelNum++)
                {
                    ImpedanceOutputConsole.Inlines.Add("CH " + (channelNum + 1).ToString() + ": " + impValues[channelNum] + "\n");
                    impedEntry = "CH" + (channelNum + 1).ToString();
                    impedEntry += ", " + impValues[channelNum];
                }
                impScroller.ScrollToEnd();
            }
            else
            {
                ImpedanceOutputConsole.Inlines.Add("Unable to perform impedance check due to connection issues!");
            }
        }
    }
}