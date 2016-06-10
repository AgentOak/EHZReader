#include <SoftwareSerial.h>
/**
 * Arduino EHZ363LA SML Reader
 * Copyright (C) 2014  Dennis Boysen, Fabian Dellwing, Jan Erik Petersen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/* ****************************************************************************
 * General constants
 * ****************************************************************************/
#define LED_PIN 13 // The LED is used for some informative output
#define PC_BAUDRATE 9600 // Baudrate for PC communication

/* ****************************************************************************
 * EHZ constants are implementation details for the eHZ communication
 * ****************************************************************************/
#define EHZ_RX_PIN 7
#define EHZ_TX_PIN 6 // Not used actually but for the sake of completeness
#define EHZ_INVERSE_LOGIC false
#define EHZ_BAUDRATE 9600

/* ****************************************************************************
 * SML constants are bytes of the SML file format
 * ****************************************************************************/
static uint8_t SML_HEADER[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 };
static uint8_t SML_FOOTER[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A };

/* ****************************************************************************
 * Implementation
 * ***************************************************************************/
#define EHZ_BUFFER_SIZE 8 // SML Header size is 8, OBIS identifiers are 6
uint8_t ehzBuffer[EHZ_BUFFER_SIZE];

SoftwareSerial *ehz; // Arduino Uno doesn't offer more native serial ports

/**
 * Read one character from the eHZ serial connection.
 * Unlike {@link SoftwareSerial#read()} this is a blocking call:
 * It waits until at least one character is available.
 */
uint8_t ehzBlockingRead()
{
	while (ehz->available() <= 0) {}
	return ehz->read();
}

/**
 * Read one character from eHZ serial connection into the buffer.
 * The contents will be shifted from right to left (n-1 to 0), that is,
 * the new data will be placed in buffer[0],
 * and the leftmost (buffer[n-1]) is discarded.
 */
void ehzReadIntoBuffer()
{
	for (int i = 1; i < EHZ_BUFFER_SIZE; i++)
	{
		ehzBuffer[i - 1] = ehzBuffer[i];
	}
	ehzBuffer[EHZ_BUFFER_SIZE- 1] = ehzBlockingRead();
}

/**
 * Checks if the newest contents of the buffer match the given data.
 */
boolean ehzBufferEquals(uint8_t value[])
{
	for (int i = sizeof(value) - 1; i >= 0; i--)
	{
		if (ehzBuffer[i + (EHZ_BUFFER_SIZE - sizeof(value))] != value[i])
		{
			return false;
		}
	}
	return true;
}

void setup()
{
	// The LED is used for some informative output
	pinMode(LED_PIN, OUTPUT);

	// Initialize communication with PC
	Serial.begin(PC_BAUDRATE);
	while (!Serial) {}
	Serial.println("Arduino EHZ363LA SML Reader");
	Serial.println("Copyright (C) 2014  Dennis Boysen, Fabian Dellwing, Jan Erik Petersen");
	Serial.println();

	// Starting serial connection to eHZ
	ehz = new SoftwareSerial(EHZ_RX_PIN, EHZ_TX_PIN, EHZ_INVERSE_LOGIC);
	ehz->begin(EHZ_BAUDRATE);

	// Finally
	Serial.println("Ready to accept sensor data.");

	// TODO: Calls to other groups initializations before eHZ communication?
	// The eHZ could overflow the buffer if the initilizations of the other
	// groups take too long
}

/*
 * Shifting the bytes from the serial connection into a buffer is VERY
 * error-resistant. Even if the connection is removed for some time,
 * the program is able to recover correct communication by ignoring
 * all data until a valid SML header is found again.
 */
void loop()
{
	// Read into the buffer...
	ehzReadIntoBuffer();

	// ...until it is a valid SML header!
	if (ehzBuffer[0] == 0x1B && ehzBuffer[1] == 0x1B &&
	        ehzBuffer[2] == 0x1B && ehzBuffer[3] == 0x1B &&
	        ehzBuffer[4] == 0x01 && ehzBuffer[5] == 0x01 &&
	        ehzBuffer[6] == 0x01 && ehzBuffer[7] == 0x01)
	{
		// Turn on the LED to show that we're receiving an SML packet
		digitalWrite(LED_PIN, HIGH);
		
		// Print to the console that we've found an SML header
		long startTime = millis();
		Serial.print("===> SML HEADER bei ");
		Serial.print(startTime);
		Serial.println("ms");

		// Find the data we need until we reach the end of the SML file
		int i = 0;
		while (!(ehzBuffer[0] == 0x1B && ehzBuffer[1] == 0x1B &&
		         ehzBuffer[2] == 0x1B && ehzBuffer[3] == 0x1B &&
		         ehzBuffer[4] == 0x1A))
		{
			i++;
			ehzReadIntoBuffer();
			
			// Check if we have the data we want
			if (i == 169)
			{
				Serial.println("Bei 188!");
				Serial.println((long) (ehzBuffer[0] << 8 | ehzBuffer[1]));
			}
			//Serial.print(String(ehzBuffer[0], HEX));
		}

		// Print to the console that we're done parsing the SML packet
		long endTime = millis();
		Serial.println();
		Serial.print("===> SML FOOTER bei ");
		Serial.print(endTime);
		Serial.println("ms");
		
		// Statistics!
		Serial.print("Übertragung dauerte ");
		Serial.print(endTime - startTime);
		Serial.println("ms");
		Serial.println();
		
		// Turn off the LED to show we're done parsing the SML packet
		digitalWrite(LED_PIN, LOW);
	}
}
