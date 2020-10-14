using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.IO.Ports;

namespace BICSharpNetConsoleTest
{
    class BIcCppWrapper
    {
        [DllImport("BICCPPWrapper.dll", SetLastError = false, ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern void bicCppWrapper();

        [DllImport("BICCPPWrapper.dll", SetLastError = false, ExactSpelling = true, CallingConvention = CallingConvention.Cdecl)]
        public static extern int initialize();
    }
}
