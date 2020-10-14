using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace BICSharpNetConsoleTest
{
    class Program
    {

        static void Main(string[] args)
        {
            for(int i = 0; i < 10; i++)
            {
                Console.WriteLine("Response: " + BIcCppWrapper.initialize().ToString());
            }
            Console.ReadKey();
        }
    }
}
