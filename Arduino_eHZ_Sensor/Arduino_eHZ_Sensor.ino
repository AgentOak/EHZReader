#include <SoftwareSerial.h>
/* ****************************************************************************
 * General constants
 * ****************************************************************************/
#define LED_PIN 13 // The LED is used for some informational output

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
int32_t ehzMeterTotal; // divide by 10! Wh
int32_t ehzMeterTariff1; // divide by 10! 10 Wh
int32_t ehzMeterTariff2; // divide by 10! 10 Wh
int32_t ehzCurrentPower; // W

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
		return 0;
	}

	// We don't have wide enough datatypes to store more than 32bits
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

	// Let the other groups setup their stuff
	G1_setup();
	//G2_setup();
	//G3_setup();
	//G5_setup();

	// Starting serial connection to eHZ
	ehz = new SoftwareSerial(EHZ_RX_PIN, EHZ_TX_PIN, EHZ_INVERSE_LOGIC);
	ehz->begin(EHZ_BAUDRATE);
}

void loop()
{
	// Make the sure the eHZ SoftwareSerial is listening
	if (!ehz->isListening())
	{
		ehz->listen();
	}

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
					
					// Notify all listeners about the new data
					G1_notify();
					//G2_notify();
					//G3_notify();
					//G5_notify();
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
					ehzReadFieldInteger();
					ehzMeterTotal = ehzReadFieldInteger();
					ehzReadFieldInteger();
				}
				else if (memcmp(ehzBuffer, OBIS_METER_TARIFF1, EHZ_BUFFER_SIZE) == 0)
				{
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzMeterTariff1 = ehzReadFieldInteger();
					ehzReadFieldInteger();
				}
				else if (memcmp(ehzBuffer, OBIS_METER_TARIFF2, EHZ_BUFFER_SIZE) == 0)
				{
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzMeterTariff2 = ehzReadFieldInteger();
					ehzReadFieldInteger();
				}
				else if (memcmp(ehzBuffer, OBIS_CURRENT_POWER, EHZ_BUFFER_SIZE) == 0)
				{
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					ehzReadFieldInteger();
					int16_t rawEhzCurrentPower = ehzReadFieldInteger();
					// eHZ sends the current power inverted for some reason
					ehzCurrentPower = ~rawEhzCurrentPower + 1;
					ehzReadFieldInteger();
				}
			}

			// Turn off the LED to show we're done receiving data
			digitalWrite(LED_PIN, LOW);
		}
	}
	
	//G1_loop();
	//G2_loop();
	//G3_loop();
	//G5_loop();
}

void G1_setup()
{
	Serial.begin(57600);
}
 
void G1_notify()
{
	Serial.print("MT:");
	Serial.print(ehzMeterTotal);
	Serial.println(";");
	Serial.print("CP:");
	Serial.print(ehzCurrentPower);
	Serial.println(";");
}