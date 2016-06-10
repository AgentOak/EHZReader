using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO.Ports;
using System.Net;

namespace EHZReaderServer
{
    class Program
    {
        public static readonly string VERSION = "1.0";
        public static readonly int EHZREADER_BAUDRATE = 57600;
        public static readonly string WEBSERVER_ADDRESS = "localhost:8061";

        static void Main(string[] args)
        {
            Console.WriteLine("###############################################################################");
            Console.WriteLine("### BS-Niebuell eHZ Reader Server v" + VERSION + " (c) 2015 Jan Erik Petersen");
            Console.WriteLine("###############################################################################");
            Console.WriteLine("### Expects data from serial port like this:");
            Console.WriteLine("### - " + EHZREADER_BAUDRATE + " Baud");
            Console.WriteLine("### - Reads data line-wise, terminated by \\n");
            Console.WriteLine("### - Possible lines are: \"MT:12730;\" or \"CP:61;\"");
            Console.WriteLine("###############################################################################");
            Console.WriteLine("### API is:");
            Console.WriteLine("### - http://" + WEBSERVER_ADDRESS + "/data");
            Console.WriteLine("### - Returns JSON \"{ \"mt\": 12730, \"cp\": 61 }\"");
            Console.WriteLine("### - mt is Meter Total in tenths of Wh");
            Console.WriteLine("### - cp is Current Power usage in W");
            Console.WriteLine("### - cp and mt are initially -1 (no data from serial port received yet)");
            Console.WriteLine("###############################################################################");
            Console.WriteLine();

            if (!HttpListener.IsSupported)
            {
                Program.Log("Main", "HttpListener is not supported on this platform.");
                Program.Log("Main", "You need Windows XP SP2, Server 2003 or later!");
            }
            else
            {
                if (SerialPort.GetPortNames().Length == 0)
                {
                    Program.Log("Main", "No serial ports found! Remember to install Arduino Drivers");
                    Program.Log("Main", "to be able to communicate with Arduino over serial!");
                }
                else
                {
                    Console.WriteLine("Available serial ports: ");
                    SerialPort.GetPortNames().ToList().ForEach(port => Console.WriteLine("- " + port));
                    string portname = null;
                    while (true)
                    {
                        Console.Write("Select port > ");
                        portname = Console.ReadLine();
                        if (SerialPort.GetPortNames().Contains(portname))
                        {
                            break;
                        }
                        Program.Log("Main", "Invalid port!");
                    }
                    Console.WriteLine();

                    EHZReader ehzReader = new EHZReader(portname);
                    if (!ehzReader.Start())
                    {
                        Program.Log("Main", "ERROR: Couldn't start EHZReader.");
                    }
                    else
                    {
                        WebHandler webHandler = new WebHandler(ehzReader);
                        if (!webHandler.Start())
                        {
                            Program.Log("Main", "ERROR: Couldn't start WebHandler.");
                        }
                        else
                        {
                            // Everything running, wait endlessly, do not terminate
                            while (true)
                            {
                                Console.ReadLine();
                            }
                        }
                    }
                }
            }

            Program.Log("Main", "Application terminated, press enter to exit.");
            Console.ReadLine();
        }

        public static void Log(String module, String text, Exception e = null)
        {
            Console.WriteLine(String.Format("{0:u} [{1}] {2}", DateTime.Now, module, text));
            if (e != null)
            {
                Console.WriteLine(e.ToString());
            }
        }
    }
}
