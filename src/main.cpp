#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <U8g2lib.h>
#include <atomic>
#include <string>

volatile std::atomic<int32_t> currentStepSize;
static std::atomic<uint8_t> keyArray[7];

const uint32_t interval = 10;		 // Display update interval
const uint8_t octave = 4;			 // Octave to start on
const uint32_t samplingRate = 44100; // Sampling rate
typedef struct {
	int32_t stepSize;
	std::string note;
} Note;
const Note notes[] = {
	{0, "None"}, {3185014, "C1"}, {3374405, "C1#"}, {3575058, "D1"}, {3787642, "D1#"}, {4012867, "E1"}, {4251484, "F1"}, {4504291, "F1#"}, {4772130, "G1"}, {5055895, "G1#"}, {5356535, "A1"}, {5675051, "A1#"}, {6012507, "B1"}, {6370029, "C2"}, {6748811, "C2#"}, {7150116, "D2"}, {7575284, "D2#"}, {8025734, "E2"}, {8502969, "F2"}, {9008582, "F2#"}, {9544260, "G2"}, {10111791, "G2#"}, {10713070, "A2"}, {11350102, "A2#"}, {12025014, "B2"}, {12740059, "C3"}, {13497622, "C3#"}, {14300233, "D3"}, {15150569, "D3#"}, {16051469, "E3"}, {17005939, "F3"}, {18017164, "F3#"}, {19088521, "G3"}, {20223583, "G3#"}, {21426140, "A3"}, {22700205, "A3#"}, {24050029, "B3"}, {25480118, "C4"}, {26995245, "C4#"}, {28600466, "D4"}, {30301138, "D4#"}, {32102938, "E4"}, {34011878, "F4"}, {36034329, "F4#"}, {38177042, "G4"}, {40447167, "G4#"}, {42852281, "A4"}, {45400410, "A4#"}, {48100059, "B4"}, {50960237, "C5"}, {53990491, "C5#"}, {57200933, "D5"}, {60602277, "D5#"}, {64205876, "E5"}, {68023756, "F5"}, {72068659, "F5#"}, {76354085, "G5"}, {80894335, "G5#"}, {85704562, "A5"}, {90800821, "A5#"}, {96200119, "B5"}, {101920475, "C6"}, {107980982, "C6#"}, {114401866, "D6"}, {121204555, "D6#"}, {128411753, "E6"}, {136047513, "F6"}, {144137319, "F6#"}, {152708170, "G6"}, {161788670, "G6#"}, {171409125, "A6"}, {181601642, "A6#"}, {192400238, "B6"}, {203840951, "C7"}, {215961965, "C7#"}, {228803732, "D7"}, {242409110, "D7#"}, {256823506, "E7"}, {272095026, "F7"}, {288274638, "F7#"}, {305416340, "G7"}, {323577341, "G7#"}, {342818251, "A7"}, {363203285, "A7#"}, {384800476, "B7"}};
#define icon_width 10
#define icon_height 10
const unsigned char icon_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0xcc, 0x00, 0xcc, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x01, 0x02, 0x01, 0xfe, 0x01, 0x00, 0x00};

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

uint16_t getTopKey() {
	uint16_t topKey = 0;
	for (uint8_t i = 0; i < 3; i++) {
		for (uint8_t j = 0; j < 4; j++) {
			if (keyArray[i] & (0x1 << j)) {
				topKey = (octave - 2) * 12 + i * 4 + j + 1;
			}
		}
	}
	return topKey;
}

void sampleISR() {
	static int32_t phaseAcc = 0;
	phaseAcc += currentStepSize;
	int32_t Vout = phaseAcc >> 24;
	analogWrite(OUTR_PIN, Vout + 128);
}

void scanKeysTask(void *pvParameters) {
	const TickType_t xFrequency = 50 / portTICK_PERIOD_MS;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while (1) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		for (uint8_t i = 0; i < 3; i++) {
			setRow(i);
			delayMicroseconds(3);
			keyArray[i] = readCols();
		}
		currentStepSize = notes[getTopKey()].stepSize; // Atomic Store
	}
}

void displayUpdateTask(void *pvParameters) {
	const TickType_t xFrequency = 100 / portTICK_PERIOD_MS;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while (1) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		u8g2.clearBuffer();									  // clear the internal memory
		u8g2.setFont(u8g2_font_profont12_mf);				  // choose a suitable font
		u8g2.drawStr(2, 10, notes[getTopKey()].note.c_str()); // Print the current key
		digitalToggle(LED_BUILTIN);
		u8g2.setCursor(2, 20);
		for (uint8_t i = 0; i < 7; i++) {
			u8g2.print(keyArray[i], HEX);
		}
		u8g2.drawXBM(118, 0, 10, 10, icon_bits);
		u8g2.sendBuffer(); // transfer internal memory to the display
	}
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

	TIM_TypeDef *Instance = TIM1;
	HardwareTimer *sampleTimer = new HardwareTimer(Instance);
	sampleTimer->setOverflow(samplingRate, HERTZ_FORMAT);
	sampleTimer->attachInterrupt(sampleISR);
	sampleTimer->resume();

	TaskHandle_t scanKeysHandle = NULL;
	TaskHandle_t displayUpdateHandle = NULL;
	xTaskCreate(
		scanKeysTask,	// Function that implements the task
		"scanKeys",		// Text name for the task
		64,				// Stack size in words, not bytes
		NULL,			// Parameter passed into the task
		2,				// Task priority
		&scanKeysHandle // Pointer to store the task handle
	);
	xTaskCreate(
		displayUpdateTask,	 // Function that implements the task
		"displayUpdate",	 // Text name for the task
		256,				 // Stack size in words, not bytes
		NULL,				 // Parameter passed into the task
		1,					 // Task priority
		&displayUpdateHandle // Pointer to store the task handle
	);
	vTaskStartScheduler();
}

void loop() {}