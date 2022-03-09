// https://github.com/adamb314/ThreadHandler

#include <Arduino.h>
#include <U8g2lib.h>

#pragma region Config Values
const uint32_t interval = 10; // Display update interval
#pragma endregion

#pragma region Pin Definitions
// Row select and enable
const int RA0_PIN = D3;
const int RA1_PIN = D6;
const int RA2_PIN = D12;
const int REN_PIN = A5;
// Matrix input and output
const int C0_PIN = A2;
const int C1_PIN = D9;
const int C2_PIN = A6;
const int C3_PIN = D1;
const int OUT_PIN = D11;
// Audio analogue out
const int OUTL_PIN = A4;
const int OUTR_PIN = A3;
// Joystick analogue in
const int JOYY_PIN = A0;
const int JOYX_PIN = A1;
// Output multiplexer bits
const int DEN_BIT = 3;
const int DRST_BIT = 4;
const int HKOW_BIT = 5;
const int HKOE_BIT = 6;
#pragma endregion

U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0); // Display driver object

// Function to set outputs using key matrix
void setOutMuxBit(const uint8_t bitIdx, const bool value) {
	digitalWrite(REN_PIN, LOW);
	digitalWrite(RA0_PIN, bitIdx & 0x01);
	digitalWrite(RA1_PIN, bitIdx & 0x02);
	digitalWrite(RA2_PIN, bitIdx & 0x04);
	digitalWrite(OUT_PIN, value);
	digitalWrite(REN_PIN, HIGH);
	delayMicroseconds(2);
	digitalWrite(REN_PIN, LOW);
}

// Function to read keys from key matrix
uint32_t readKeys() {
	uint32_t keys = 0;
	for (uint8_t i = 0; i < 3; i++) {
		digitalWrite(REN_PIN, LOW);
		digitalWrite(RA0_PIN, i & 0x01);
		digitalWrite(RA1_PIN, i & 0x02);
		digitalWrite(RA2_PIN, i & 0x04);
		digitalWrite(REN_PIN, HIGH);
		delayMicroseconds(5);
		keys |= !digitalRead(C0_PIN) << (i * 4);
		keys |= !digitalRead(C1_PIN) << (i * 4 + 1);
		keys |= !digitalRead(C2_PIN) << (i * 4 + 2);
		keys |= !digitalRead(C3_PIN) << (i * 4 + 3);
	}
	digitalWrite(REN_PIN, LOW);
	return keys;
}

// Read key values in currently set row
uint8_t readCols() {
	uint8_t row = 0;
	row |= !digitalRead(C0_PIN) << 0;
	row |= !digitalRead(C1_PIN) << 1;
	row |= !digitalRead(C2_PIN) << 2;
	row |= !digitalRead(C3_PIN) << 3;
	return row;
}

// Set current row
void setRow(const uint8_t rowIdx) {
	digitalWrite(REN_PIN, LOW);
	digitalWrite(RA0_PIN, rowIdx & 0x01);
	digitalWrite(RA1_PIN, rowIdx & 0x02);
	digitalWrite(RA2_PIN, rowIdx & 0x04);
	digitalWrite(REN_PIN, HIGH);
}

void setup() {
#pragma region Pin Setup
	pinMode(RA0_PIN, OUTPUT);
	pinMode(RA1_PIN, OUTPUT);
	pinMode(RA2_PIN, OUTPUT);
	pinMode(REN_PIN, OUTPUT);
	pinMode(OUT_PIN, OUTPUT);
	pinMode(OUTL_PIN, OUTPUT);
	pinMode(OUTR_PIN, OUTPUT);
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(C0_PIN, INPUT);
	pinMode(C1_PIN, INPUT);
	pinMode(C2_PIN, INPUT);
	pinMode(C3_PIN, INPUT);
	pinMode(JOYX_PIN, INPUT);
	pinMode(JOYY_PIN, INPUT);
#pragma endregion
#pragma region Display Setup
	setOutMuxBit(DRST_BIT, LOW); // Assert display logic reset
	delayMicroseconds(2);
	setOutMuxBit(DRST_BIT, HIGH); // Release display logic reset
	u8g2.begin();
	setOutMuxBit(DEN_BIT, HIGH); // Enable display power supply
#pragma endregion

	// Initialise UART
	Serial.begin(115200);
	Serial.println("Hello World");
}

void loop() {
	static uint32_t next = millis();
	static uint8_t keyArray[7];
	for (uint8_t i = 0; i < 3; i++) {
		setRow(i);
		delayMicroseconds(3);
		keyArray[i] = readCols();
	}
	for (uint8_t i = 3; i < 7; i++) {
		setRow(i);
		delayMicroseconds(3);
		keyArray[i] = readCols();
	}

	if (millis() > next) {
		next += interval;
		u8g2.clearBuffer();					  // clear the internal memory
		u8g2.setFont(u8g2_font_profont12_mf); // choose a suitable font
		u8g2.drawStr(2, 10, "Hello World!");  // write something to the internal memory
		digitalToggle(LED_BUILTIN);
		u8g2.setCursor(2, 20);
		for (uint8_t i = 0; i < 7; i++) {
			u8g2.print(keyArray[i], HEX);
		}
		u8g2.sendBuffer(); // transfer internal memory to the display
	}
}