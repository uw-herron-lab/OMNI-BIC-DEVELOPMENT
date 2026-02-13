using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
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
        private Configuration configInfo;
        public class Configuration
        {
            public string filePath {  get; set; }
        }
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
                            ImpedanceOutputConsole.Inlines.Add("Loaded " + fileName + "\n");
                            ImpedanceOutputConsole.Inlines.Add("Save path: " +  configInfo.filePath + "\n");
                            impScroller.ScrollToEnd();
                        }
                        string saveDir = configInfo.filePath + @"\" + DateTime.Now.ToString("yyyy-MM-dd");
                        impBICManager.saveDir = saveDir;
                    }
                }
            }
            catch (Exception ex)
            {
                ImpedanceOutputConsole.Inlines.Add("Unable to load selected file\n");
                ImpedanceOutputConsole.Inlines.Add("Error encoutnered: " + ex.Message + "\n");
                ImpedanceOutputConsole.Inlines.Add("\n");
                impScroller.ScrollToEnd();
            }
            
            
        }

        private void btn_impcheck_Click(object sender, RoutedEventArgs e)
        {
            // Run impedance check
            ImpedanceOutputConsole.Inlines.Add("Starting impedances check. \n");
            impScroller.ScrollToEnd();
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