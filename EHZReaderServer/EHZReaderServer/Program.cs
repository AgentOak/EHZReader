using System;
using System.Linq;
using System.IO.Ports;
using System.Net;
using System.Threading;

namespace EHZReaderServer
{
    class Program
    {
        const string VERSION = "1.2.1";

        static void Main(string[] args)
        {
            Console.WriteLine("###############################################################################");
            Console.WriteLine("### BS-Niebuell eHZ Reader Server");
            Console.WriteLine("### v" + VERSION + " (2016-06-10)");
            Console.WriteLine("### (c) 2015-2016 Jan Erik Petersen");
            Console.WriteLine("###############################################################################");
            Console.WriteLine("### Settings (Change in EHZReaderServer.exe.config):");
            Console.WriteLine("### - Serial Baudrate: " + Properties.Settings.Default.serialBaudrate);
            Console.WriteLine("### - Webserver Address: " + Properties.Settings.Default.webserverPrefix);
            Console.WriteLine("###############################################################################");
            Console.WriteLine();

            // HttpListener is used by the WebHandler
            if (!HttpListener.IsSupported)
            {
                Program.Log("Main", "HttpListener is not supported on this platform.");
                Program.Log("Main", "You need Windows XP SP2, Server 2003 or later!");
                Console.ReadLine();
                Environment.Exit(100);
            }

            if (SerialPort.GetPortNames().Length == 0)
            {
                Program.Log("Main", "No serial ports found! Install Arduino Drivers to");
                Program.Log("Main", "be able to communicate with Arduinos over serial!");
                Console.ReadLine();
                Environment.Exit(101);
            }

            // Let the user choose the serial port
            // This is not a compiled option, because the serial port can change frequently
            Console.WriteLine("Available serial ports: ");
            SerialPort.GetPortNames().ToList().ForEach(port => Console.WriteLine("- " + port));
            Console.WriteLine();

            // Loop until the user inputs a valid port
            string portname = null;
            while (true)
            {
                Console.Write("Select port: ");
                portname = Console.ReadLine();
                if (SerialPort.GetPortNames().Contains(portname))
                {
                    break;
                }
                Console.WriteLine("Invalid port (Capitalization matters!)");
            }
            Console.WriteLine();

            // Build the modules
            EHZReader ehzReader = new EHZReader(portname, Properties.Settings.Default.serialBaudrate);
            WebHandler webHandler = new WebHandler(ehzReader, Properties.Settings.Default.webserverPrefix);

            // Start them (order matters!), in case of an error fall to Exit() below
            if (ehzReader.Start() && webHandler.Start())
            {
                // Wait until one of the modules stops working, possibly endlessly
                // Application can be closed by pressing Ctrl-C in the console or closing the window
                while (ehzReader.IsAlive() && webHandler.IsAlive())
                {
                    Thread.Sleep(500);
                }
            }

            // Make sure no more threads are running so the application can terminate cleanly
            ehzReader.Kill();
            webHandler.Kill();

            Program.Log("Main", "Application terminated, press enter to exit.");
            Console.ReadLine();
            Environment.Exit(1);
        }

        /**
         * Output a text to the console, formatted with date and module name.
         */
        public static void Log(string module, string text, Exception e = null)
        {
            Console.WriteLine(string.Format("{0:u} [{1}] {2}", DateTime.Now, module, text));
            if (e != null)
            {
                Console.WriteLine(e.ToString());
            }
        }
    }
}
