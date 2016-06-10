Diese Softwarel�sung besteht aus 2 Programmen:

Arduino_eHZ_Sensor
******************
Arduino-Programm um Verbrauchsdaten im SML-Format mit Hilfe eines Lesekopfes von einem eHZ einzulesen. Sendet die Daten mit einer serielle Verbindung �ber den USB-Anschluss weiter. Im Programmquellcode sind ausf�hrliche Dokumentationen zur Funktionsweise und zum Aufbau des SML-Formats vorhanden. Das Programm versucht ununterbrochen Daten vom Lesekopf einzulesen, sobald der Arduino eingeschaltet wird. Auch nach Verbindungsabbr�chen wird das Einlesen fortgesetzt, ohne das ein Neustarten des Arduinos notwendig ist.

Das Format jedes Datenpaketes bei der �bertragung ist:
XX:Y;\n
Wobei XX den Typ der Daten angibt, Y die dezimale Darstellung eines maximal 32bit gro�es Integers.
Typen: MT (Meter Total), M1 (Meter Tariff 1), M2 (Meter Tariff 2), CP (Current Power)
Beispiel:
> MT:1873;
> CP:60;

EHZReaderServer
***************
C#-Konsolenprogramm um die Daten vom Arduino einzulesen und per HTTP zur Verf�gung zu stellen. Beinhaltet au�erdem ein HTML-Frontend, welches live den aktuellen Z�hlerstand und den Verbrauch als Graph darstellt.
Das Frontend liegt im Resources/-Ordner des Visual Studio Projekts.

Zum Start gen�gt ein Doppelklick auf die EHZReaderServer.exe, woraufhin die serielle Schnittstelle, an der der Arduino angeschlossen ist, eingetippt werden muss.
Die erwartete serielle Baudrate und die URL, unter der das Frontend erreichbar ist, stehen im Kopf der Programmausgabe.
Diese Einstellungen k�nnen in der EHZReaderServer.exe.config angepasst werden. Ein Neustart des Programms ist hierbei notwendig.

Wie oft das Frontend die aktuellen Daten abfragt und wie viele Werte im Graph angezeigt werden, l�sst sich ebenfalls in besagter Datei konfigurieren.

Die eigentlichen Daten werden unter <adresse>/data bereitgestellt. Diese sind im JSON-Format und liegen als float vor.
Beispiel:
> { "mt": 187.3, "cp": 60.0 }
