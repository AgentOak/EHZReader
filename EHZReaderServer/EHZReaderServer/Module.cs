using System;
using System.Threading;

namespace EHZReaderServer
{
    abstract class Module : IDisposable
    {
        public readonly string name;
        private readonly Thread thread;

        /**
         * Internal status flag, indicates wether the module should keep running or stop its execution.
         */
        protected volatile bool running;

        public Module(string name)
        {
            this.name = name;
            this.thread = new Thread(Run);
            this.thread.Name = name;
            this.thread.IsBackground = true;
        }

        /**
         * Start the execution of this module. Returns false if an error occured.
         */
        public bool Start()
        {
            Program.Log(this.name, "Starting...");

            this.running = true;

            try
            {
                StartModule();
            }
            catch (Exception e)
            {
                Program.Log(this.name, "Failed to initialize.", e);
                return false;
            }

            this.thread.Start();

            Program.Log(this.name, "Running.");
            return true;
        }

        /**
         * Initialize all the resources needed by this module for proper operation.
         */
        protected abstract void StartModule();

        /**
         * Check if this module is still running.
         */
        public bool IsAlive()
        {
            return this.thread.IsAlive;
        }

        private void Run()
        {
            try
            {
                RunModule();
            }
            catch (Exception e)
            {
                // Supress exception if shutting down
                if (this.running)
                {
                    Program.Log(this.name, "Unexpected error occured.", e);
                    return;
                }
            }

            Program.Log(this.name, "Stopped.");
            return;
        }

        /**
         * Do the actual work. Should return when running becomes false, otherwise only in case of an error.
         * Only catch Exceptions to ignore them, otherwise let them get through, because Module will handle them appropriately.
         */
        protected abstract void RunModule();

        /**
         * Stop the module at all costs, suppressing errors. IsAlive() should always return false after Kill().
         */
        public void Kill()
        {
            this.running = false;

            // Uninitialize resources
            try
            {
                KillModule();
            }
            catch
            {
                // Suppress any error while shutting down
            }

            // Only deal with the thread if it actually lived at some point
            if (IsAlive())
            {
                // Wait at most 2s for the thread to exit on its own
                if (!this.thread.Join(2000))
                {
                    // Thread failed to terminate, forcefully kill it now
                    this.thread.Abort();
                }
            }
        }

        /**
         * Shut down the module, releasing all resources and making the RunModule() method stop.
         */
        protected abstract void KillModule();

        /**
         * We can implement IDisposable, so why not?
         */
        public void Dispose()
        {
            this.Kill();
        }
    }
}
