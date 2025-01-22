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
        private RealtimeGraphing.BICManager aBICManager;
        private bool connectState = false;

        // Logging
        FileStream impedFileStream;
        StreamWriter impedFileWriter;

        public MainWindow()
        {
            InitializeComponent();
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {

            // Establish connection
            aBICManager = new RealtimeGraphing.BICManager((int)MainWindow1.Width, true); // additional parameter that is set to default otherwise?
            connectState = aBICManager.BICConnect();

            // Perform impedance check upon startup
            performImpCheck(connectState);
        }

        private void MainWindow_Closed(object sender, EventArgs e)
        {
            aBICManager.Dispose();
        }

        private void btn_repeatcheck_Click(object sender, RoutedEventArgs e)
        {
            ImpedanceOutputConsole.Inlines.Add("Repeating impedance check... \n");

            // Dispose
            aBICManager.Dispose();
            connectState = false;

            // Re-establish connection
            aBICManager = new RealtimeGraphing.BICManager((int)MainWindow1.Width, true); // additional parameter that is set to default otherwise?
            connectState = aBICManager.BICConnect();

            // Repeat impedance check
            performImpCheck(connectState);

        }
        private void performImpCheck(bool currConnectState)
        {
            if (currConnectState)
            {
                string timestamp = DateTime.Now.ToString("hh:mm:ss tt");

                // Perform impedance check and log the impedance values
                string impedFilePath = "./impedances" + DateTime.Now.ToString("_MMddyyyy_HHmmss") + ".csv";
                List<string> impValues = aBICManager.getImpedances();
                impedFileStream = new FileStream(impedFilePath, FileMode.Create, FileAccess.Write, FileShare.None, 4096, FileOptions.Asynchronous);
                impedFileWriter = new StreamWriter(impedFileStream);

                // Display impedances to the console window
                ImpedanceOutputConsole.Inlines.Add("Impedance check at: " + timestamp + "\n");
                
                string impedEntry = "";
                for (int channelNum = 0; channelNum < impValues.Count; channelNum++)
                {
                    ImpedanceOutputConsole.Inlines.Add("CH " + (channelNum + 1).ToString() + ": " + impValues[channelNum] + "\n");
                    impedEntry = "CH" + (channelNum + 1).ToString();
                    impedEntry += ", " + impValues[channelNum];
                    impedFileWriter.WriteLine(impedEntry);
                }
                impScroller.ScrollToEnd();

                // Close impedance logging items
                impedFileWriter.Flush();
                impedFileWriter.Dispose();
                impedFileStream.Dispose();
            }
            else
            {
                ImpedanceOutputConsole.Inlines.Add("Unable to perform impedance check due to connection issues!");
            }
        }
    }
}