using System;
using System.Net;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace EHZReaderServer
{
    // Copied from , slightly modified
        public class WebServer
        {
            private readonly HttpListener _listener = new HttpListener();
            private readonly Func<HttpListenerRequest, HttpListenerResponse, string> _responderMethod;

            public WebServer(string[] prefixes, Func<HttpListenerRequest, HttpListenerResponse, string> method)
            {
                // URI prefixes are required, for example 
                // "http://localhost:8080/index/".
                if (prefixes == null || prefixes.Length == 0)
                    throw new ArgumentException("prefixes is null or empty");

                // A responder method is required
                if (method == null)
                    throw new ArgumentException("method is null");

                foreach (string s in prefixes)
                    _listener.Prefixes.Add(s);

                _responderMethod = method;
                _listener.Start();
            }

            public WebServer(Func<HttpListenerRequest, HttpListenerResponse, string> method, params string[] prefixes)
                : this(prefixes, method) { }

            public void Run()
            {
                ThreadPool.QueueUserWorkItem((o) =>
                {
                    try
                    {
                        while (_listener.IsListening)
                        {
                            ThreadPool.QueueUserWorkItem((c) =>
                            {
                                var ctx = c as HttpListenerContext;
                                ctx.Response.KeepAlive = false;
                                try
                                {
                                    string rstr = _responderMethod(ctx.Request, ctx.Response);
                                    byte[] buf = Encoding.UTF8.GetBytes(rstr);
                                    ctx.Response.ContentLength64 = buf.Length;
                                    ctx.Response.OutputStream.Write(buf, 0, buf.Length);
                                    ctx.Response.OutputStream.Flush();
                                }
                                catch { } // suppress any exceptions
                                finally
                                {
                                    // always close the stream
                                    ctx.Response.OutputStream.Close();
                                }
                            }, _listener.GetContext());
                        }
                    }
                    catch { } // suppress any exceptions
                });
            }

            public void Stop()
            {
                _listener.Stop();
                _listener.Close();
            }
        }
}
