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
using System.Windows.Shapes;

namespace StimTherapyApp
{
    /// <summary>
    /// Interaction logic for ImpedWindow.xaml
    /// </summary>
    public partial class ImpedWindow : Window
    {
        public ImpedWindow(string[] impedVals)
        {
            InitializeComponent();

            // Display the impedance values for each BIC channel
            for (int i = 1; i < impedVals.Count()+1; i++)
            {
                ImpedOutput.Inlines.Add("CH " + i.ToString() + ": " + impedVals[i-1].ToString() + "\n");
            }
        }
    }
}
