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
 * Short & minimal SML Documentation, written by Jan Erik Petersen
 * ****************************************************************************
 *
 * Every SML valList contains several valListEntries, each consisting of
 * several fields.
 *
 * valList:
 * 7X					X: count of valListEntries
 *    <1. valListEntry>
 *    <2. valListEntry>
 *    ...
 *
 * valListEntry:
 * 7X					X: count of fields (should always be 7)
 *    <1. field>		objName (OBIS identifier)
 *    <2. field>		status
 *    <3. field>		valTime
 *    <4. field>		unit
 *    <5. field>		scaler
 *    <6. field>		value
 *    <7. field>		valueSignature
 *
 * field:
 * XY ZZ ZZ ..	 		X: 0 = String (used OBIS Manufactor ID)
 *						   5 = signed int
 *						   6 = unsigned int
 *						   8 = byte array (length Y doesn't apply, used for OBIS Public Key)
 *						Y: length of the field (including the first byte XY)
 *						   therefore, Y = 1 means the field is empty
 *						   Y = 0 is used to end a list (but not a valListEntry!)
 *						Z: the actual value, length in bytes is Y - 1
 *
 * Example how it looks like assembled:
 *	77
 *		77
 *			07 81 81 C7 82 03 FF			OBIS Manufactor ID	=> "HAG"
 *			01								status (empty)
 *			01								valTime (empty)
 *			01								unit (empty)
 *			01								scaler (empty)
 *			04 48 41 47						value (String3)	= "HAG"
 *			01								valueSignatur (empty)
 *		77
 *			07 01 00 02 08 00 FF			OBIS Meter Total	=> 3584,3 Wh
 *			62 A0							status (uint16)	= A0
 *			01								valTime (empty)
 *			62 1E							unit (uint8)	= Wh
 *			52 FF							scaler (int8)	= -1 => 10^-1 = 0,1
 *			54 00 8C 03						value (int24)	= 35843
 *			01								valueSignature (empty)
 *    ...
 */
/* ****************************************************************************
 * General constants
 * ****************************************************************************/
#define LED_PIN 13 // The LED is used for some informational output
#define PC_BAUDRATE 57600 // Baudrate for PC communication, using 9600 is too slow!!

/* ****************************************************************************
 * EHZ constants are implementation details for the eHZ communication
 * ****************************************************************************/
#define EHZ_RX_PIN 7
#define EHZ_TX_PIN 6 // Not used actually but for the sake of completeness
#define EHZ_INVERSE_LOGIC false
#define EHZ_BAUDRATE 9600
#define EHZ_WAIT_INITIAL_DATA 50 // In ms, how long to wait for new data
#define EHZ_RECEIVE_TIMEOUT 4 // In ms, how long to wait for new data from the eHZ

/* ****************************************************************************
 * SML constants are bytes of the SML file format
 * ****************************************************************************/
static uint8_t SML_HEADER[]	= { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 };
static uint8_t SML_FOOTER[]	= { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A };

/* ****************************************************************************
 * OBIS identifiers - we want to find these in the SML data
 * ****************************************************************************/
static uint8_t OBIS_MANUFACTOR_ID[]	= { 0x77, 0x07, 0x81, 0x81, 0xC7, 0x82, 0x03, 0xFF };
static uint8_t OBIS_DEVICE_ID[]		= { 0x77, 0x07, 0x01, 0x00, 0x00, 0x00, 0x09, 0xFF };
static uint8_t OBIS_METER_TOTAL[]	= { 0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x00, 0xFF };
static uint8_t OBIS_METER_TARIFF1[]	= { 0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x01, 0xFF };
static uint8_t OBIS_METER_TARIFF2[]	= { 0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x02, 0xFF };
static uint8_t OBIS_CURRENT_POWER[]	= { 0x77, 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF };

/* ****************************************************************************
 * API
 * ****************************************************************************/
//String ehzManufactorId;
//String ehzDeviceId;
double ehzMeterTotal;
double ehzMeterTariff1;
double ehzMeterTariff2;
int32_t ehzCurrentPower;

/* ****************************************************************************
 * Implementation
 * ****************************************************************************/
#define EHZ_BUFFER_SIZE 8 // Must be enough for everything we look for in the data
uint8_t ehzBuffer[EHZ_BUFFER_SIZE];

SoftwareSerial *ehz; // Arduino Uno doesn't offer more native serial ports

/**
 * Read one character from the eHZ serial connection.
 * Unlike {@link SoftwareSerial#read()} this is a blocking call:
 * It waits until at least one character is available, and therefore
 * only ever returns actual payload data, unlike {@link SoftwareSerial#read()}
 * which could also return -1 indicating no data is available.
 */
static inline uint8_t ehzBlockingRead()
{
	// In the best case scenario this saves one call to ehz->available()
	int val;
	while ((val = ehz->read()) == -1) {}
	return val;
}

/**
 * Read one character from eHZ serial connection into the buffer.
 * The contents will be shifted from right to left (n-1 to 0).
 *
 * Shifting the bytes from the serial connection into a buffer is VERY
 * error-resistant. Even if the connection is removed for some time,
 * the program is able to recover correct communication by ignoring
 * all data until a valid identifier is found again.
 */
void ehzReadIntoBuffer()
{
	memmove(ehzBuffer, ehzBuffer + 1, EHZ_BUFFER_SIZE - 1);
	ehzBuffer[EHZ_BUFFER_SIZE - 1] = ehzBlockingRead();
}

/**
 * Timing based sucks. But we need to because serial is connectionless.
 * We need to know when the cable to the eHZ is removed so we don't
 * wait for data in an endless loop.
 */
boolean ehzSendsData(unsigned long waitTime)
{
	unsigned long startTime = millis();
	while (startTime + waitTime > millis())
	{
		if (ehz->available())
		{
			return true;
		}
	}
	return false;
}

uint32_t ehzReadFieldInteger()
{
	// Length of the whole field is the second nibble of the first byte of the field
	uint8_t length = (ehzBlockingRead() & 0x0F) - 1;
	if (length == 0)
	{
		// If the field is empty we just return zero
		return 0;
	}

	// Don't store more than 32bits, more isn't needed and harms performance
	for (; length > 4; length--)
	{
		// So we cut off the front bits
		ehzBlockingRead();
	}

	// Actually read the value
	uint32_t value = 0;
	switch (length)
	{
			// intentional fall-through
		case 4:
			value |= ((uint32_t) ehzBlockingRead()) << 24;
		case 3:
			value |= ((uint32_t) ehzBlockingRead()) << 16;
		case 2:
			value |= ((uint32_t) ehzBlockingRead()) << 8;
		case 1:
			value |= ((uint32_t) ehzBlockingRead());
	}

	return value;
}

void setup()
{
	// The LED is used for some informational output
	pinMode(LED_PIN, OUTPUT);
	
	// Initialize communication with PC
	Serial.begin(PC_BAUDRATE);
	while (!Serial) {}

	// Starting serial connection to eHZ
	ehz = new SoftwareSerial(EHZ_RX_PIN, EHZ_TX_PIN, EHZ_INVERSE_LOGIC);
	ehz->begin(EHZ_BAUDRATE);
}

void loop()
{
	// We receive(d) data!
	if (ehzSendsData(EHZ_WAIT_INITIAL_DATA))
	{
		// Did we receive too much already?
		if (ehz->overflow())
		{
			// Flush down all the data we receive
			while (ehzSendsData(EHZ_RECEIVE_TIMEOUT))
			{
				ehz->flush();
			}
		}
		else
		{
			// Turn on the LED to show that we're receiving data
			digitalWrite(LED_PIN, HIGH);

			// Until we find a valid SML header...
			while (memcmp(ehzBuffer, SML_HEADER, sizeof(SML_HEADER)) != 0)
			{
				// ... check if the eHZ still sends data
				if (!ehzSendsData(EHZ_RECEIVE_TIMEOUT))
				{
					// If not, the connection is possibly broken
					return;
				}
				
				// ... else, read into the buffer
				ehzReadIntoBuffer();
			}

			// As long as the eHZ sends data
			while (ehzSendsData(EHZ_RECEIVE_TIMEOUT))
			{
				// Read byte one for one into the buffer
				ehzReadIntoBuffer();
				// And check every sequence for an SML/OBIS identifier

				// If we reached the end of the file...
				if (memcmp(ehzBuffer, SML_FOOTER, sizeof(SML_FOOTER)) == 0)
				{
					// Skip all other data following
					while (ehzSendsData(EHZ_RECEIVE_TIMEOUT))
					{
						ehz->flush();
					}
					
					Serial.println("SML End found.");
				}
				/*else if (memcmp(ehzBuffer, OBIS_MANUFACTOR_ID, EHZ_BUFFER_SIZE) == 0)
				{
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzManufactorId = ehzReadFieldString();
					ehzReadFieldInteger();
				}
				else if (memcmp(ehzBuffer, OBIS_DEVICE_ID, EHZ_BUFFER_SIZE) == 0)
				{
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzDeviceId = ehzReadFieldString();
					ehzReadFieldInteger();
				}*/
				else if (memcmp(ehzBuffer, OBIS_METER_TOTAL, EHZ_BUFFER_SIZE) == 0)
				{
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger(); // Scaler is ignored to minimize filesize
					ehzMeterTotal = ((double) ehzReadFieldInteger()) / 10.; // The 5th field is the value
					ehzReadFieldInteger();
					
					Serial.print("Meter Total: "); // Output the value for debugging purposes
					Serial.print(ehzMeterTotal);
					Serial.println(" Wh");
				}
				else if (memcmp(ehzBuffer, OBIS_METER_TARIFF1, EHZ_BUFFER_SIZE) == 0)
				{
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger(); // Scaler is ignored to minimize filesize
					ehzMeterTariff1 = ((double) ehzReadFieldInteger()) / 10.; // The 5th field is the value
					ehzReadFieldInteger();
					
					Serial.print("Meter Tariff 1: "); // Output the value for debugging purposes
					Serial.print(ehzMeterTariff1);
					Serial.println(" Wh");
				}
				else if (memcmp(ehzBuffer, OBIS_METER_TARIFF2, EHZ_BUFFER_SIZE) == 0)
				{
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger(); // Scaler is ignored to minimize filesize
					ehzMeterTariff2 = ((double) ehzReadFieldInteger()) / 10.; // The 5th field is the value
					ehzReadFieldInteger();
					
					Serial.print("Meter Tariff 2: "); // Output the value for debugging purposes
					Serial.print(ehzMeterTariff2);
					Serial.println(" Wh");
				}
				else if (memcmp(ehzBuffer, OBIS_CURRENT_POWER, EHZ_BUFFER_SIZE) == 0)
				{
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger(); // Scaler is ignored to minimize filesize
					int16_t rawEhzCurrentPower = ehzReadFieldInteger(); // The 5th field is the value
					// eHZ sends the current power inverted for some reason
					ehzCurrentPower = ~rawEhzCurrentPower + 1;
					ehzReadFieldInteger();
					
					Serial.print("Current Power: "); // Output the value for debugging purposes
					Serial.print(ehzCurrentPower);
					Serial.println(" W");
				}
			}

			// Turn off the LED to show we're done receiving data
			digitalWrite(LED_PIN, LOW);
		}
	}
}
