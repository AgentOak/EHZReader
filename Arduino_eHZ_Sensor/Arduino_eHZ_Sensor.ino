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
#define LED_HALT_BLINKRATE 100 // How long the LED should blink
#define PC_BAUDRATE 9600 // Baudrate for PC communication

/* ****************************************************************************
 * EHZ constants are implementation details for the eHZ communication
 * ****************************************************************************/
#define EHZ_RX_PIN 7
#define EHZ_TX_PIN 6 // Not used actually but for the sake of completeness
#define EHZ_INVERSE_LOGIC false
#define EHZ_BAUDRATE 9600

/* ****************************************************************************
 * Implementation
 * ***************************************************************************/
#define SML_BUFFER_SIZE 8 // SML Header size is 8, OBIS identifiers are 6
char smlBuffer[SML_BUFFER_SIZE];

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
void readIntoBuffer()
{
	for (int i = 1; i < SML_BUFFER_SIZE ; i++)
	{
		smlBuffer[i - 1] = smlBuffer[i];
	}
	smlBuffer[SML_BUFFER_SIZE - 1] = ehzBlockingRead();
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
	Serial.println("eHZ sensor initialized. Ready to accept data.");

	// TODO: Calls to other groups initializations before eHZ communication?
	// The eHZ could spam the buffer if the initilizations of the other groups
	// take too long
}

void loop()
{
	/*
	 * Shifting the bytes from the serial connection into a buffer is VERY
	 * error-resistant. Even if the connection is removed for some time,
	 * the program is able to recover correct communication by ignoring
	 * all data until an SML header is found.
	 */
	// Read and shift the bytes...
	readIntoBuffer();

	// ...until it is a valid SML header!
	if (smlBuffer[0] == 0x1B && smlBuffer[1] == 0x1B &&
	        smlBuffer[2] == 0x1B && smlBuffer[3] == 0x1B &&
	        smlBuffer[4] == 0x01 && smlBuffer[5] == 0x01 &&
	        smlBuffer[6] == 0x01 &&  smlBuffer[7] == 0x01)
	{
		Serial.println("SML HEADER");
		
		// Parse the SML
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
		Serial.println();
	}
}

/* ****************************************************************************
 * API
 * ****************************************************************************/
struct energyMeterData
{
	long drawnPower;
	long currentPower;
};
