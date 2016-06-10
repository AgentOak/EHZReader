#include <SoftwareSerial.h>
#include <SML.h>
/**
 * Arduino_eHZ_Sensor
 *
 * Implements the DIN EN62056-21 Protocol and SML file format
 * to read power data from energy meters on Arduino Uno.
 * For other energy meters and arduinos changes might be necessary.
 * Modify pins according to your hardware configuration.
 *
 * Copyright (c) 2014 Dennis Boysen, Fabian Dellwing, Jan Erik Petersen
 * Licensed under the GNU General Public License v3.0.
 */
/* ****************************************************************************
 * General constants
 * ****************************************************************************/
#define LED_PIN 13 // The LED is used for some informative output
#define LED_ERROR_BLINKTIME 100 // How long the LED should be on/off
#define PC_BAUDRATE 9600 // Baudrate for PC communication

/* ****************************************************************************
 * EHZ constants are implementation details for the eHZ communication
 * ****************************************************************************/
#define EHZ_RX_PIN 7
#define EHZ_TX_PIN 6
#define EHZ_INVERSE_LOGIC false
static int EHZ_BAUDRATES[] = { 300, 600, 1200, 2400, 4800, 9600, 19200 }; // Baudrates the EHZ supports, by the ID the EHZ sends.
#define EHZ_SERIAL_INIT_WAIT 5000 // Time to wait for serial communication to eHZ to initialize
#define EHZ_IDENTIFICATION_WAIT 5000 // Time to wait for the first signals from the eHZ

/* ****************************************************************************
 * MSG constants are bytes of the DIN EN62056-21 protocol
 * ****************************************************************************/
#define MSG_START 0x2F // /
#define MSG_TRANSMISSION_REQUEST 0x3F // ?
#define MSG_END 0x21 // !
static char MSG_COMPLETION[] = { 0x0D, 0x0A }; // <CR><LF>
#define MSG_FRAME_START 0x02 // <STX>
#define MSG_FRAME_END 0x03 // <ETX>

/* ****************************************************************************
 * SML constants are bytes of the SML file format
 * ****************************************************************************/
static char SML_START_ESCAPE[] = { 0x1B, 0x1B, 0x1B, 0x1B };

/* ****************************************************************************
 * Implementation
 * ***************************************************************************/
SoftwareSerial *ehz;

/**
 * Fail the initialization with a given error message.
 * Prints the error to the PC Serial connection. Halts the execution of the
 * program. Lets the LED blink.
 */
void initFail(String message)
{
	Serial.println("#########################################");
	Serial.print("Initialization fail: Error after ");
	Serial.print(millis());
	Serial.println("ms");
	Serial.println(message);
	while(true)
	{
		digitalWrite(LED_PIN, HIGH);
		delay(LED_ERROR_BLINKTIME);
		digitalWrite(LED_PIN, LOW);
		delay(LED_ERROR_BLINKTIME);
	}
}

/**
 * Reads one character from the eHZ serial connection.
 * Unlike {@link SoftwareSerial#read()} this is a blocking call:
 * It waits until at least one character is available.
 */
uint8_t ehzBlockingRead()
{
	while (ehz->available() <= 0) {}
	return ehz->read();
}

void setup()
{
	// The LED is used for some informative output
	pinMode(LED_PIN, OUTPUT);

	// Initialize communication with PC
	Serial.begin(PC_BAUDRATE);
	while (!Serial) {}
	Serial.println("Thank you for using Arduino_eHZ_Sensor!");

	// Starting serial connection to eHZ
	pinMode(EHZ_RX_PIN, INPUT);
	pinMode(EHZ_TX_PIN, OUTPUT);
	ehz = new SoftwareSerial(EHZ_RX_PIN, EHZ_TX_PIN, EHZ_INVERSE_LOGIC);
	ehz->begin(EHZ_BAUDRATES[0]);
	delay(EHZ_SERIAL_INIT_WAIT);

	/* ************************************************************************
	 * eHZ handshake
	 * ************************************************************************/
	// Send SIGN ON
	ehz->write(MSG_START);
	ehz->write(MSG_TRANSMISSION_REQUEST);
	ehz->write(MSG_END);
	ehz->write(MSG_COMPLETION);

	// Wait for response from the eHZ
	long startTime = millis();
	while (ehz->available() <= 0)
	{
		if (millis() > startTime + EHZ_IDENTIFICATION_WAIT)
		{
			initFail("The eHZ didn't send any data. Is the eHZ cable connected?");
		}
	}

	// Receive IDENTIFICATION
	/*if (ehzBlockingRead() != MSG_START)
	{
		initFail("eHZ didn't send expected IDENTIFICATION after SIGN ON");
	}
	char manufactorId[] = { ehzBlockingRead(), ehzBlockingRead(), ehzBlockingRead() };
	char baudrateSent = ehzBlockingRead();
	if (baudrateSent > sizeof(EHZ_BAUDRATES))
	{
		initFail("eHZ sent unknown baudrate identifier");
	}
	int baudrate = EHZ_BAUDRATES[baudrateSent];*/

	// TODO: Finish handshake

	Serial.println("Initialization done, connection to eHZ established.");

	// TODO: Calls to other groups initializations?
}

#define BUFSIZE 16
char buffer[BUFSIZE];

void loop()
{
	/* ************************************************************************
	 * Continuously reading sensor data and forwarding in to PC serial console
	 * ************************************************************************/
	
	if (ehz->available() > 0)
	{
		Serial.print(ehz->available());
		Serial.print(" ::>");
		while (ehz->available() > 0)
		{
			Serial.print(" ");
			Serial.print(ehzBlockingRead());
		}
		Serial.println();
	}
	// TODO: Read EN62056-21 packets, parse SML, inform listeners, write to serial
	
	/*
	 * DEBUG 1
	for (int i = 0; i < BUFSIZE; i++) {
		char c = ehzBlockingRead();
		buffer[i] = c;
		Serial.print(String(c, HEX));
	}
	Serial.println();
	for (int i = 0; i < BUFSIZE; i++) {
		Serial.print(buffer[i]);
	}
	Serial.println();
	Serial.println();*/
	
	/* 
	 * DEBUG 2
	ehz->write(MSG_START);
	ehz->write(MSG_TRANSMISSION_REQUEST);
	ehz->write(MSG_END);
	ehz->write(MSG_COMPLETION);
	
	long startTime = millis();
	Serial.print(startTime);
	Serial.print(" -> ");
	while (millis() < startTime + 1000)
	{
		if (ehz->available() > 0)
		{
			uint8_t c = ehzBlockingRead();
			Serial.print(" 0x");
			Serial.print(String(c, HEX));
		}
	}
	Serial.println();*/
}

/* ****************************************************************************
 * API
 * ****************************************************************************/
struct energyMeterData
{
	long drawnPower;
	long currentPower;
};

/* ****************************************************************************
 * Test using the API
 * ****************************************************************************/
void test_sensorListener()
{
	
}