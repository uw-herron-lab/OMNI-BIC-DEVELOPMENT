using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;

namespace EMGTriggeredStimTherapyApp
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
    }
}

/* Build dependencies: EMGLib
 * 
 * in order to have the following work in .xam: xmlns:winformchart="clr-namespace:System.Windows.Forms.DataVisualization.Charting;assembly=System.Windows.Forms.DataVisualization"
 * -Add references-
 * -> Assembly
 * --> System.Windows.Forms.DataVisualization
 * --> System.Windows.Forms
 * --> WindowsFormsIntegration
 * 
 * 
 * Also need to add references and download Nug
 
 */