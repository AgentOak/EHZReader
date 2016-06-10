using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO.Ports;
using System.Threading;

namespace EHZReaderServer
{
    class EHZReader
    {
        private readonly SerialPort serialPort;
        private volatile Thread readerThread;
        private volatile int lastMT = -1;
        private volatile int lastCP = -1;

        public EHZReader(string portname)
        {
            this.serialPort = new SerialPort(portname, Program.EHZREADER_BAUDRATE, Parity.None, 8, StopBits.One);
        }

        public int GetMeterTotal()
        {
            return this.lastMT;
        }

        public int GetCurrentPower()
        {
            return this.lastCP;
        }

        public bool Start()
        {
            Program.Log("EHZReader", "Starting EHZReader... ");

            if (readerThread != null)
            {
                Program.Log("EHZReader", "ERROR: EHZReader is already running!");
                return false;
            }

            try
            {
                serialPort.Open();
                if (!serialPort.IsOpen)
                {
                    Program.Log("EHZReader", "ERROR: Couldn't open serial port! (IsOpen=false)");
                    return false;
                }
            }
            catch (Exception e)
            {
                Program.Log("EHZReader", "ERROR: Couldn't open serial port! (Exception)", e);
                return false;
            }

            readerThread = new Thread(ReadData);
            readerThread.Start();

            Program.Log("EHZReader", "OK.");
            return true;
        }

        private void ReadData()
        {
            while (true)
            {
                string data = serialPort.ReadLine();
                // Minimum is 5, see "XY:0;"
                if (data.Length < 5)
                {
                    Program.Log("EHZReader", "Ignoring unknown data with length less than 5 received from serial port");
                    continue;
                }
                string type = data.Substring(0, 3);
                try
                {
                    switch (type)
                    {
                        case "MT:":
                            lastMT = Int32.Parse(data.Substring(3).Split(';')[0]);
                            Program.Log("EHZReader", "Received data: MT=" + lastMT);
                            break;
                        case "CP:":
                            lastCP = Int32.Parse(data.Substring(3).Split(';')[0]);
                            Program.Log("EHZReader", "Received data: CP=" + lastCP);
                            break;
                        default:
                            Program.Log("EHZReader", "Ignoring unknown data starting with \"" + type + "\" received from serial port");
                            break;
                    }
                }
                catch (Exception e)
                {
                    Program.Log("EHZReader", "Ignoring error trying to parse data from serial port", e);
                }
            }
        }
    }
}
