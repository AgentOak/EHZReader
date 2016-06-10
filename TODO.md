List of possible Improvements
=============================

Not actual TODOs, just ideas for future development.


Arduino_eHZ_Sensor
------------------

* Send own version over serial, so a program can determine if it should be updated
* Send more of the SML Data (Unit, Scaler, etc.) instead of letting the program guess
* Some sort of identifier that can be given in the source code, to make multiple Arduinos differentiable by software


EHZReaderServer
---------------

#### Backend

* If there is only one serial port on the system, just connect to it without asking?
* **Support for multiple energy meters**
	* Try connecting to all serial ports available? Timeout and release port when not receiving appropriate data
	* Start even without any working serial port
* Access password
* Actual logging/database
	* Provide historical data over JSON
* Separate /data and Frontend more

#### Frontend

* **Get rid of Google Charts**, replace with something that can be used offline
* **Support for multiple energy meters**
	* Manageable in the Frontend? Admin password?
* Make chart more configurable
* favicon/touch-icon
