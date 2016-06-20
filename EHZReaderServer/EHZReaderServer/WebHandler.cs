using System;
using System.Text;
using System.Threading;
using System.Net;
using System.Globalization;

namespace EHZReaderServer
{
    class WebHandler : Module
    {
        private readonly EHZReader ehzReader;

        private readonly HttpListener listener;

        public WebHandler(EHZReader ehzReader, string prefix)
            : base("WebHandler")
        {
            this.ehzReader = ehzReader;

            this.listener = new HttpListener();
            this.listener.Prefixes.Add(prefix);
        }

        protected override void StartModule()
        {
            this.listener.Start();
        }

        protected override void RunModule()
        {
            while (this.listener.IsListening && this.running)
            {
                // Use ThreadPool to process incoming requests using multiple threads (optional performance enhancement)
                ThreadPool.QueueUserWorkItem((c) =>
                {
                    var ctx = c as HttpListenerContext;
                    ctx.Response.KeepAlive = false;
                    try
                    {
                        string rstr = HandleRequest(ctx.Request, ctx.Response);
                        byte[] buf = Encoding.UTF8.GetBytes(rstr);
                        ctx.Response.ContentLength64 = buf.Length;
                        ctx.Response.OutputStream.Write(buf, 0, buf.Length);
                        ctx.Response.OutputStream.Flush();
                    }
                    catch (Exception e)
                    {
                        // Don't let the module crash, the error might relate only to a single request
                        Program.Log(this.name, "Ignoring error while handling http request.", e);
                    }
                    finally
                    {
                        ctx.Response.OutputStream.Close();
                    }
                }, listener.GetContext()); // GetContext() blocks until an incoming connection is available
            }
        }

        protected override void KillModule()
        {
            if (this.listener.IsListening)
            {
                this.listener.Stop();
            }

            this.listener.Close();
        }

        private string HandleRequest(HttpListenerRequest request, HttpListenerResponse response)
        {
            Program.Log(this.name, "Request: From=\"" + request.RemoteEndPoint.Address.ToString() + "\", RawUrl=\"" + request.RawUrl + "\"");

            string content;
            if (request.Url.AbsolutePath.Equals("/data") || request.Url.AbsolutePath.Equals("/data/"))
            {
                response.ContentType = "application/json; charset=utf-8";
                response.Headers.Add("Expires", "0");
                response.StatusCode = 200;
                content = "{ \"mt\": " + ehzReader.GetMeterTotal().ToString(CultureInfo.InvariantCulture) +
                    ", \"m1\": " + ehzReader.GetMeterTariff1().ToString(CultureInfo.InvariantCulture) +
                    ", \"m2\": " + ehzReader.GetMeterTariff2().ToString(CultureInfo.InvariantCulture) +
                    ", \"cp\": " + ehzReader.GetCurrentPower().ToString(CultureInfo.InvariantCulture) + " }";
            }
            else if (request.Url.AbsolutePath.Equals("/") || request.Url.AbsolutePath.Equals("/index.html"))
            {
                response.ContentType = "text/html; charset=utf-8";
                response.StatusCode = 200;
                content = Properties.Resources.index_html
                    .Replace("##DATA_METER_TOTAL##", ehzReader.GetMeterTotal().ToString(CultureInfo.InvariantCulture))
                    .Replace("##DATA_METER_TARIFF_1##", ehzReader.GetMeterTariff1().ToString(CultureInfo.InvariantCulture))
                    .Replace("##DATA_METER_TARIFF_2##", ehzReader.GetMeterTariff2().ToString(CultureInfo.InvariantCulture))
                    .Replace("##DATA_CURRENT_POWER##", ehzReader.GetCurrentPower().ToString(CultureInfo.InvariantCulture))
                    .Replace("##CONF_EHZ_METER_DECIMAL_PLACES##", Properties.Settings.Default.ehzMeterDecimalPlaces.ToString())
                    .Replace("##CONF_EHZ_POWER_DECIMAL_PLACES##", Properties.Settings.Default.ehzPowerDecimalPlaces.ToString())
                    .Replace("##CONF_CHART_UPDATE_INTERVAL##", Properties.Settings.Default.chartUpdateInterval.ToString())
                    .Replace("##CONF_CHART_DATA_COUNT##", Properties.Settings.Default.chartDataCount.ToString())
                    .Replace("##CONF_CHART_LABEL_EVERY##", Properties.Settings.Default.chartLabelEvery.ToString());
            }
            else if (request.Url.AbsolutePath.Equals("/jquery-2.2.4.min.js"))
            {
                response.ContentType = "application/javascript; charset=utf-8";
                response.StatusCode = 200;
                content = Properties.Resources.jquery_2_2_4_min_js;
            }
            else
            {
                response.StatusCode = 404;
                content = null;
            }

            Program.Log(this.name, "Response: Status=" + response.StatusCode + ", Length=" + (content == null ? 0 : content.Length) + ", Type=\"" + response.ContentType + "\"");
            return content;
        }
    }
}
