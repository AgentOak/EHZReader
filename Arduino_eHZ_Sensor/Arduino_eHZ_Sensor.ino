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
 *    <3. field>		valTime (usually empty)
 *    <4. field>		unit
 *    <5. field>		scaler
 *    <6. field>		value
 *    <7. field>		valueSignature (usually empty)
 *
 * field:
 * XY ZZ ZZ ..	 		X: 0 = String
 *						   5 = signed int
 *						   6 = unsigned int
 *						Y: length of the field (including the first byte XY)
 *						   therefore, Y = 1 means the field is empty
 *						   Y = 0 is used to end a list (but not a valListEntry!)
 *						Z: the actual value, length in bytes is Y - 1
 *
 * Example how it looks like assembled:
 * 77
 *    77
 *       07 81 81 C7 82 03 FF             OBIS 129-129:199.130.3*255 = Manufactor Id
 *       01                               status = empty
 *       01                               valTime = empty
 *       01                               unit = empty
 *       01								  scaler = empty
 *       04 48 41 47                      value = "HAG"
 *       01								  valueSignature = empty
 *    77
 *       07 01 00 01 08 00 FF             OBIS 1-0:1.8.0*255 = Meter Total
 *       63 01 82                         status (uint16)
 *       01                               valTime = empty
 *       62 1E                            unit (uint8) = Wh
 *       52 FF                            scaler (int8) = -1
 *       56 00 01 29 71 4F                value (int40) = 19493967
 *       01                               valueSignature = empty
 *    ...
 */
/* ****************************************************************************
 * General constants
 * ****************************************************************************/
#define LED_PIN 13 // The LED is used for some informational output
#define PC_BAUDRATE 57600 // Baudrate for PC communication

/* ****************************************************************************
 * EHZ constants are implementation details for the eHZ communication
 * ****************************************************************************/
#define EHZ_RX_PIN 7
#define EHZ_TX_PIN 6 // Not used actually but for the sake of completeness
#define EHZ_INVERSE_LOGIC false
#define EHZ_BAUDRATE 9600
#define EHZ_RECEIVE_TIMEOUT 2 // In ms, How long it should take at most to receive another byte if the eHZ is sending right now

/* ****************************************************************************
 * SML constants are bytes of the SML file format
 * ****************************************************************************/
static uint8_t SML_HEADER[]	= { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 };
static uint8_t SML_FOOTER[]	= { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A };

/* ****************************************************************************
 * OBIS identifiers - we want these
 * ****************************************************************************/
static uint8_t OBIS_MANUFACTOR_ID[]	= { 0x77, 0x07, 0x81, 0x81, 0xC7, 0x82, 0x03, 0xFF };
//static uint8_t OBIS_DEVICE_ID[]		= { 0x77, 0x07, 0x01, 0x00, 0x00, 0x00, 0x09, 0xFF };
static uint8_t OBIS_METER_TOTAL[]	= { 0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x00, 0xFF };
static uint8_t OBIS_METER_TARIFF1[]	= { 0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x01, 0xFF };
static uint8_t OBIS_METER_TARIFF2[]	= { 0x77, 0x07, 0x01, 0x00, 0x02, 0x08, 0x02, 0xFF };
static uint8_t OBIS_CURRENT_POWER[]	= { 0x77, 0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xFF };
//static uint8_t OBIS_PUBLIC_KEY[]	= { 0x77, 0x07, 0x81, 0x81, 0xC7, 0x82, 0x05, 0xFF };

/* ****************************************************************************
 * Implementation
 * ****************************************************************************/
#define EHZ_BUFFER_SIZE 8 // Must be enough for everything we look for in the data
uint8_t ehzBuffer[EHZ_BUFFER_SIZE];

SoftwareSerial *ehz; // Arduino Uno doesn't offer more native serial ports

/* ****************************************************************************
 * API
 * ****************************************************************************/
//String manufactorId;
//String deviceId;
//int32_t meterTotal;
//int32_t meterTariff1;
//int32_t meterTariff2;
//int32_t currentPower;
//String publicKey;

#define DATA_METER_TOTAL 0
#define DATA_METER_TARIFF1 1
#define DATA_METER_TARIFF2 2
#define DATA_CURRENT_POWER 3
double currentData[4];

/**
 * Read one character from the eHZ serial connection.
 * Unlike {@link SoftwareSerial#read()} this is a blocking call:
 * It waits until at least one character is available, and therefore
 * only ever returns actual payload data, unlike {@link SoftwareSerial#read()}
 * which could also return -1 indicating no data is available.
 */
uint8_t ehzBlockingRead()
{
	//while (ehz->available() <= 0) {}
	//return ehz->read();

	// In the best case scenario this saves one call to ehz->available()
	int val;
	while ((val = ehz->read()) == -1) {}
	return val;
}

/*
 * Shifting the bytes from the serial connection into a buffer is VERY
 * error-resistant. Even if the connection is removed for some time,
 * the program is able to recover correct communication by ignoring
 * all data until a valid SML header is found again.
 */

/**
 * Read {@param bytes} characters from eHZ serial connection into the buffer.
 * The contents will be shifted from right to left (n-1 to 0).
 */
void ehzReadIntoBuffer(uint8_t bytes)
{
	// TODO: memcpy/memmove?
	for (int i = bytes; i < EHZ_BUFFER_SIZE; i++)
	{
		ehzBuffer[i - bytes] = ehzBuffer[i];
	}
	for (int i = EHZ_BUFFER_SIZE - bytes; i < EHZ_BUFFER_SIZE; i++)
	{
		ehzBuffer[i] = ehzBlockingRead();
	}
}

/**
 * Checks if the newest contents of the buffer match the given data.
 */
static inline boolean ehzBufferEquals(uint8_t value[], size_t length)
{
	return memcmp(ehzBuffer + (EHZ_BUFFER_SIZE - length), value, length) == 0;
	/*for (int i = 0; i < length; i++)
	{
		if (ehzBuffer[i + (EHZ_BUFFER_SIZE - length)] != value[i])
		{
			return false;
		}
	}
	return true;*/
}

boolean ehzSendsData()
{
	unsigned long startTime = millis();
	while (startTime + EHZ_RECEIVE_TIMEOUT > millis())
	{
		if (ehz->available())
		{
			return true;
		}
	}
	return false;
}

int32_t ehzReadWithLength()
{
	uint8_t length = (ehzBlockingRead() & 0x0F) - 1;

	int32_t value = 0; // It will return 0 if length is 0

	// We don't have wide enough datatypes to store more than 32bits
	switch (length)
	{
		case 8:
			ehzBlockingRead();
		case 7:
			ehzBlockingRead();
		case 6:
			ehzBlockingRead();
		case 5:
			ehzBlockingRead();
		case 4:
			value |= ((int32_t) ehzBlockingRead()) << 24;
		case 3:
			value |= ((int32_t) ehzBlockingRead()) << 16;
		case 2:
			value |= ((int32_t) ehzBlockingRead()) << 8;
		case 1:
			value |= ((int32_t) ehzBlockingRead());
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
	Serial.println("Arduino EHZ363LA SML Reader");
	Serial.println("Copyright (C) 2014  Dennis Boysen, Fabian Dellwing, Jan Erik Petersen");
	Serial.println();

	// Starting serial connection to eHZ
	ehz = new SoftwareSerial(EHZ_RX_PIN, EHZ_TX_PIN, EHZ_INVERSE_LOGIC);
	ehz->begin(EHZ_BAUDRATE);

	// Finally
	Serial.println("[INFO] Ready to accept sensor data.");

	// TODO: Calls to other groups initializations before eHZ communication?
	// The eHZ could overflow the buffer if the initializations of the other
	// groups take too long
}

void loop()
{
	// We received data!
	if (ehzSendsData())
	{
		// Did we receive too much already?
		if (ehz->overflow())
		{
			Serial.print("eHZ rx Buffer overflow, flushing data...");

			// Then the data is completely useless, discard it completely
			while (ehzSendsData())
			{
				ehz->flush();
			}
			return;
		}
		else
		{
			// Until we find a valid SML header...
			while (!ehzBufferEquals(SML_HEADER, sizeof(SML_HEADER)))
			{
				// ...read into the buffer
				ehzReadIntoBuffer(1);
			}

			// Turn on the LED to show that we're receiving an SML packet
			digitalWrite(LED_PIN, HIGH);

			// TODO: Remove debugging information
			long startTime = millis();
			Serial.print("===> SML HEADER bei ");
			Serial.print(startTime);
			Serial.println("ms");

			// Find the data we need until we reach the end of the SML file or the eHZ stops sending
			while (!ehzBufferEquals(SML_FOOTER, sizeof(SML_FOOTER)))
			{
				ehzReadIntoBuffer(1);

				uint8_t currentOBISData;

				if (ehzBufferEquals(OBIS_METER_TOTAL, sizeof(OBIS_METER_TOTAL)))
				{
					currentOBISData = DATA_METER_TOTAL;
				}
				else if (ehzBufferEquals(OBIS_METER_TARIFF1, sizeof(OBIS_METER_TARIFF1)))
				{
					currentOBISData = DATA_METER_TARIFF1;
				}
				else if (ehzBufferEquals(OBIS_METER_TARIFF2, sizeof(OBIS_METER_TARIFF2)))
				{
					currentOBISData = DATA_METER_TARIFF2;
				}
				else if (ehzBufferEquals(OBIS_CURRENT_POWER, sizeof(OBIS_CURRENT_POWER)))
				{
					currentOBISData = DATA_CURRENT_POWER;
				}
				else
				{
					continue;
				}

				ehzReadWithLength();
				ehzReadWithLength();
				ehzReadWithLength();
				int8_t scaler = ehzReadWithLength(); // 5th field is scaler
				int32_t value = ehzReadWithLength(); // 6th field is value
				ehzReadWithLength();
				
				if (currentOBISData == DATA_CURRENT_POWER)
				{
					currentData[currentOBISData] = ((double) ~((int16_t) value));
				}
				else
				{
					currentData[currentOBISData] = ((double) value) / 10;
				}

				// TODO: Remove debugging information
				Serial.print("Found data ");
				Serial.print(currentOBISData);
				Serial.print(" with value ");
				Serial.println(currentData[currentOBISData]);
			}

			// TODO: Remove debugging information
			long endTime = millis();
			Serial.print("===> ENDE bei ");
			Serial.print(endTime);
			Serial.print("ms (Dauerte ");
			Serial.print(endTime - startTime);
			Serial.println("ms)");

			// Turn off the LED to show we're done parsing the SML packet
			digitalWrite(LED_PIN, LOW);
		}
	}
}
