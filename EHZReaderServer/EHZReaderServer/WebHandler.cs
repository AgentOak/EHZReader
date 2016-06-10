using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Net;

namespace EHZReaderServer
{
    class WebHandler
    {
        private readonly EHZReader ehzReader;
        private volatile WebServer webServer;

        public WebHandler(EHZReader ehzReader)
        {
            this.ehzReader = ehzReader;
        }

        public bool Start()
        {
            Program.Log("WebHandler", "Starting WebHandler... ");

            if (webServer != null)
            {
                Program.Log("WebHandler", "ERROR: WebHandler is already running!");
                return false;
            }

            try
            {
                webServer = new WebServer(WebServe, "http://" + Program.WEBSERVER_ADDRESS + "/");
                webServer.Run();
            }
            catch (Exception e)
            {
                Program.Log("WebHandler", "Error trying to start web server", e);
                return false;
            }

            Program.Log("WebHandler", "OK.");
            return true;
        }

        public String WebServe(HttpListenerRequest request, HttpListenerResponse response)
        {
            Program.Log("WebHandler", "Received request from " + request.RemoteEndPoint.Address.ToString() + " for \"" + request.RawUrl + "\"");

            string content;
            if (request.Url.AbsolutePath.Equals("/data") || request.Url.AbsolutePath.Equals("/data/"))
            {
                response.ContentType = "application/json; charset=utf-8";
                response.Headers.Add("Expires", "0");
                response.StatusCode = 200;
                content = "{ \"mt\": " + ehzReader.GetMeterTotal() + ", \"cp\": " + ehzReader.GetCurrentPower() + " }";
            }
            else if (request.Url.AbsolutePath.Equals("/") || request.Url.AbsolutePath.Equals("/index.html"))
            {
                response.ContentType = "text/html; charset=utf-8";
                response.StatusCode = 200;
                content = EHZReaderServer.Properties.Resources.index_html;
            }
            else if (request.Url.AbsolutePath.Equals("/jquery-2.1.4.min.js"))
            {
                response.ContentType = "text/html; charset=utf-8";
                response.StatusCode = 200;
                content = EHZReaderServer.Properties.Resources.jquery_2_1_4_min_js;
            }
            else
            {
                response.StatusCode = 404;
                content = null;
            }

            Program.Log("WebHandler", "Handled with status " + response.StatusCode);
            return content;
        }
    }
}
