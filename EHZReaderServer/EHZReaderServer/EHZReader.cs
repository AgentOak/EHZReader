using System;
using System.IO.Ports;

namespace EHZReaderServer
{
    class EHZReader : Module
    {
        private readonly SerialPort serialPort;

        private volatile float lastMeterTotal = -1; // MT; Wh
        private volatile float lastMeterTariff1 = -1; // M1; Wh
        private volatile float lastMeterTariff2 = -1; // M2; Wh
        private volatile float lastCurrentPower = -1; // CP; W

        public EHZReader(string portname, int baudrate)
            : base("EHZReader")
        {
            this.serialPort = new SerialPort(portname, baudrate, Parity.None, 8, StopBits.One);
        }

        /**
         * Return the last meter total value received from the Arduino. If no values have been received yet, returns -1.
         */
        public double GetMeterTotal()
        {
            return this.lastMeterTotal;
        }

        /**
         * Return the last meter tariff 1 value received from the Arduino. If no values have been received yet, returns -1.
         */
        public double GetMeterTariff1()
        {
            return this.lastMeterTariff1;
        }

        /**
         * Return the last meter tariff 2 value received from the Arduino. If no values have been received yet, returns -1.
         */
        public double GetMeterTariff2()
        {
            return this.lastMeterTariff2;
        }

        /**
         * Return the last current power value received from the Arduino. If no values have been received yet, returns -1.
         */
        public double GetCurrentPower()
        {
            return this.lastCurrentPower;
        }

        protected override void StartModule()
        {
            this.serialPort.Open();
        }

        protected override void RunModule()
        {
            while (this.serialPort.IsOpen && this.running)
            {
                // Might throw an exception. Do not catch it, to make the module crash (This is intended, because the
                // serial port possibly has gone away and the user might have to choose a different serial port).
                string data = this.serialPort.ReadLine();

                // Minimum is 5, like "XX:0;"
                if (data.Length < 5)
                {
                    Program.Log(this.name, "Ignoring unknown data with length less than 5 received from serial port");
                    return;
                }

                // Determine which type the data is
                string type = data.Substring(0, 3);
                try
                {
                    switch (type)
                    {
                        case "MT:":
                            this.lastMeterTotal = Int64.Parse(data.Substring(3).Split(';')[0]) / Properties.Settings.Default.ehzMeterDivisor;
                            Program.Log(this.name, "Received data: MT=" + this.lastMeterTotal);
                            break;
                        case "M1:":
                            this.lastMeterTariff1 = Int64.Parse(data.Substring(3).Split(';')[0]) / Properties.Settings.Default.ehzMeterDivisor;
                            Program.Log(this.name, "Received data: M1=" + this.lastMeterTariff1);
                            break;
                        case "M2:":
                            this.lastMeterTariff2 = Int64.Parse(data.Substring(3).Split(';')[0]) / Properties.Settings.Default.ehzMeterDivisor;
                            Program.Log(this.name, "Received data: M2=" + this.lastMeterTariff2);
                            break;
                        case "CP:":
                            this.lastCurrentPower = Int64.Parse(data.Substring(3).Split(';')[0]) / Properties.Settings.Default.ehzPowerDivisor;
                            Program.Log(this.name, "Received data: CP=" + this.lastCurrentPower);
                            break;
                        default:
                            // Just ignore unknown data, because the Arduino program might have been enhanced to support more data types
                            Program.Log(this.name, "Ignoring unknown data starting with \"" + type + "\" received from serial port");
                            break;
                    }
                }
                catch (Exception e)
                {
                    // Same here, ignore malformed data and hope there actually is some data that we can process
                    Program.Log(this.name, "Ignoring error trying to parse data from serial port", e);
                }
            }
        }

        protected override void KillModule()
        {
            if (this.serialPort.IsOpen)
            {
                this.serialPort.Close();
            }
        }
    }
}
