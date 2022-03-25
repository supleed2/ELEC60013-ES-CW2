#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <U8g2lib.h>
#include <atomic>
#include <es_can>
#include <knob>
#include <string>

#pragma region Globals(Config values, Variables, Objects, Types, etc.)
// Config values
const uint32_t interval = 10;		 // Display update interval
const uint32_t samplingRate = 22000; // Sampling rate
const uint32_t canID = 0x123;
// Variables
std::atomic<bool> isMainSynth;
std::atomic<int32_t> currentStepSize;
std::atomic<uint8_t> keyArray[7];
std::atomic<uint8_t> octave;
std::atomic<uint8_t> selectedWaveform;
std::atomic<int> latestKey;
QueueHandle_t msgInQ;
uint8_t RX_Message[8] = {0};
std::atomic<bool> bufferAactive;
int32_t bufferA[220];
int32_t bufferB[220];
// Objects
U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0); // Display Driver Object
Knob K0 = Knob(1, 7, 4);						   // Octave Knob Object
Knob K1 = Knob(0, 3);							   // Waveform Knob Object
Knob K2 = Knob(0, 1);							   // Send / Receive Knob Object
Knob K3 = Knob(0, 16);							   // Volume Knob Object
// Program Specific Structures
const int32_t stepSizes[85] = {0, 6384507, 6764150, 7166367, 7592501, 8043975, 8522295, 9029057, 9565952, 10134773, 10737418, 11375898, 12052344, 12769014, 13528299, 14332734, 15185002, 16087950, 17044589, 18058113, 19131904, 20269547, 21474836, 22751797, 24104689, 25538028, 27056599, 28665468, 30370005, 32175899, 34089178, 36116226, 38263809, 40539093, 42949673, 45503593, 48209378, 51076057, 54113197, 57330935, 60740010, 64351799, 68178356, 72232452, 76527617, 81078186, 85899346, 91007187, 96418756, 102152113, 108226394, 114661870, 121480020, 128703598, 136356712, 144464904, 153055234, 162156372, 171798692, 182014374, 192837512, 204304227, 216452788, 229323741, 242960040, 257407196, 272713424, 288929808, 306110469, 324312744, 343597384, 364028747, 385675023, 408608453, 432905576, 458647482, 485920080, 514814392, 545426848, 577859616, 612220937, 648625489, 687194767, 728057495, 771350046};
const char *notes[85] = {"None", "C1", "C1#", "D1", "D1#", "E1", "F1", "F1#", "G1", "G1#", "A1", "A1#", "B1", "C2", "C2#", "D2", "D2#", "E2", "F2", "F2#", "G2", "G2#", "A2", "A2#", "B2", "C3", "C3#", "D3", "D3#", "E3", "F3", "F3#", "G3", "G3#", "A3", "A3#", "B3", "C4", "C4#", "D4", "D4#", "E4", "F4", "F4#", "G4", "G4#", "A4", "A4#", "B4", "C5", "C5#", "D5", "D5#", "E5", "F5", "F5#", "G5", "G5#", "A5", "A5#", "B5", "C6", "C6#", "D6", "D6#", "E6", "F6", "F6#", "G6", "G6#", "A6", "A6#", "B6", "C7", "C7#", "D7", "D7#", "E7", "F7", "F7#", "G7", "G7#", "A7", "A7#", "B7"};
std::atomic<bool> activeNotes[85] = {{0}};
enum waveform {
	SQUARE = 0,
	SAWTOOTH,
	TRIANGLE,
	SINE
};
const unsigned char waveforms[4][18] = {
	{0x7f, 0x10, 0x41, 0x10, 0x41, 0x10, 0x41, 0x10, 0x41,
	 0x10, 0x41, 0x10, 0x41, 0x10, 0x41, 0x10, 0xc1, 0x1f}, // Square Wave
	{0x70, 0x10, 0x58, 0x18, 0x48, 0x08, 0x4c, 0x0c, 0x44,
	 0x04, 0x46, 0x06, 0x42, 0x02, 0x43, 0x03, 0xc1, 0x01}, // Sawtooth Wave
	{0x08, 0x00, 0x1c, 0x00, 0x36, 0x00, 0x63, 0x00, 0xc1,
	 0x00, 0x80, 0x11, 0x00, 0x1b, 0x00, 0x0e, 0x00, 0x04}, // Triangle Wave
	{0x1c, 0x00, 0x36, 0x00, 0x22, 0x00, 0x63, 0x00, 0x41,
	 0x10, 0xc0, 0x18, 0x80, 0x08, 0x80, 0x0d, 0x00, 0x07} // Sine Wave
};
const unsigned char volumes[6][18] = {
	{0x10, 0x02, 0x98, 0x04, 0x1c, 0x05, 0x5f, 0x09, 0x5f,
	 0x09, 0x5f, 0x09, 0x1c, 0x05, 0x98, 0x04, 0x10, 0x02}, // volume max
	{0x10, 0x00, 0x98, 0x00, 0x1c, 0x01, 0x5f, 0x01, 0x5f,
	 0x01, 0x5f, 0x01, 0x1c, 0x01, 0x98, 0x00, 0x10, 0x00}, // volume mid higher
	{0x10, 0x00, 0x18, 0x00, 0x1c, 0x01, 0x5f, 0x01, 0x5f,
	 0x01, 0x5f, 0x01, 0x1c, 0x01, 0x18, 0x00, 0x10, 0x00}, // volume mid lower
	{0x10, 0x00, 0x18, 0x00, 0x1c, 0x00, 0x5f, 0x00, 0x5f,
	 0x00, 0x5f, 0x00, 0x1c, 0x00, 0x18, 0x00, 0x10, 0x00}, // volume low
	{0x10, 0x00, 0x18, 0x00, 0x1c, 0x00, 0x1f, 0x00, 0x5f,
	 0x00, 0x1f, 0x00, 0x1c, 0x00, 0x18, 0x00, 0x10, 0x00}, // volume lowest
	{0x10, 0x00, 0x18, 0x00, 0x5c, 0x04, 0x9f, 0x02, 0x1f,
	 0x01, 0x9f, 0x02, 0x5c, 0x04, 0x18, 0x00, 0x10, 0x00} // mute
};
const unsigned char icon_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0xcc, 0x00, 0xcc, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x01, 0x02, 0x01, 0xfe, 0x01, 0x00, 0x00};
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

// Set multiplexer bits to select row
void setRow(const uint8_t rowIdx) {
	digitalWrite(REN_PIN, LOW);
	digitalWrite(RA0_PIN, rowIdx & 0x01);
	digitalWrite(RA1_PIN, rowIdx & 0x02);
	digitalWrite(RA2_PIN, rowIdx & 0x04);
	digitalWrite(REN_PIN, HIGH);
}

// Returns key value (as stepSizes[] index) of highest currently pressed key
uint16_t getTopKey() {
	uint16_t topKey = 0;
	for (uint8_t i = 0; i < 3; i++) {
		for (uint8_t j = 0; j < 4; j++) {
			if (keyArray[i] & (0x1 << j)) {
				topKey = (octave - 1) * 12 + i * 4 + j + 1;
			}
		}
	}
	return topKey;
}

// Interrupt driven routine to send waveform to DAC
void sampleISR() {
	static int32_t phaseAcc = 0;
	phaseAcc += currentStepSize;
	int32_t Vout = phaseAcc >> (32 - K3.getRotation() / 2); // Volume range from (>> 32) to (>> 24), range of 8
	analogWrite(OUTR_PIN, Vout + 128);
}

void CAN_RX_ISR() {
	uint8_t ISR_RX_Message[8];
	uint32_t ISR_rxID;
	CAN_RX(ISR_rxID, ISR_RX_Message);
	xQueueSendFromISR(msgInQ, ISR_RX_Message, nullptr);
}

void generateWaveformBufferTask(void *pvParameters) {
	const TickType_t xFrequency = 50 / portTICK_PERIOD_MS;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	static int32_t waveformBuffers[85][220] = {0};
	while (1) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		// Generate waveforms and write to buffers
		int32_t nextBuffer[220] = {0};
		// Combine waveforms into nextBuffer waveform
		// Write waveform to inactive buffer
		for (uint8_t i = 0; i < 220; i++) {
			if (bufferAactive) {
				bufferB[i] = nextBuffer[i];
			} else {
				bufferA[i] = nextBuffer[i];
			}
		}
	}
}

void decodeTask(void *pvParameters) {
	while (1) {
		xQueueReceive(msgInQ, RX_Message, portMAX_DELAY);
		if (RX_Message[0] == 0x50) { // Pressed
			activeNotes[(RX_Message[1] - 1) * 12 + RX_Message[2]] = true;
			latestKey = (RX_Message[1] - 1) * 12 + RX_Message[2];
			currentStepSize = stepSizes[latestKey];
		} else { // Released
			activeNotes[(RX_Message[1] - 1) * 12 + RX_Message[2]] = false;
			if (latestKey == (RX_Message[1] - 1) * 12 + RX_Message[2]) {
				latestKey = 0;
				currentStepSize = stepSizes[latestKey]; // Atomic Store
			}
		}
	}
}

void keyChangedSendTXMessage(uint8_t octave, uint8_t key, bool pressed) {
	uint8_t TX_Message[8] = {0};
	if (pressed) {
		TX_Message[0] = 0x50; // "P"
	} else {
		TX_Message[0] = 0x52; // "R"
	}
	TX_Message[1] = octave;
	TX_Message[2] = key;
	CAN_TX(canID, TX_Message);
}

// Task to update keyArray values at a higher priority
void scanKeysTask(void *pvParameters) {
	const TickType_t xFrequency = 50 / portTICK_PERIOD_MS;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while (1) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		for (uint8_t i = 0; i < 7; i++) {
			setRow(i);
			uint8_t oldRow = keyArray[i];
			delayMicroseconds(3);
			uint8_t newRow = readCols();
			if (oldRow == newRow) {
				continue;
			} else {
				keyArray[i] = newRow;
				if (i < 3) {
					for (uint8_t j = 0; j < 4; j++) {
						if ((oldRow & (0x1 << j)) ^ (newRow & (0x1 << j))) {
							keyChangedSendTXMessage(octave, i * 4 + j + 1, newRow & (0x1 << j));
						}
					}
				}
			}
		}
		K0.updateRotation(keyArray[4] & 0x4, keyArray[4] & 0x8);
		K1.updateRotation(keyArray[4] & 0x1, keyArray[4] & 0x2);
		K2.updateRotation(keyArray[3] & 0x4, keyArray[3] & 0x8);
		K3.updateRotation(keyArray[3] & 0x1, keyArray[3] & 0x2);
	}
}

// Task containing display graphics and update code
void displayUpdateTask(void *pvParameters) {
	const TickType_t xFrequency = 100 / portTICK_PERIOD_MS;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while (1) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		uint32_t rxID;
		u8g2.clearBuffer();					  // clear the internal memory
		u8g2.setFont(u8g2_font_profont12_mf); // choose a suitable font
		char currentKeys[64] = {0};
		for (uint8_t i = 0; i < 85; i++) {
			if (activeNotes[i]) {
				strcat(currentKeys, notes[i]);
				strcat(currentKeys, " ");
			}
		}
		u8g2.drawStr(2, 10, currentKeys); // Print the currently pressed keys
		u8g2.setCursor(2, 20);
		for (uint8_t i = 0; i < 7; i++) {
			u8g2.print(keyArray[i], HEX);
		};
		u8g2.setCursor(100, 10); // Debug print of received CAN message
		u8g2.print((char)RX_Message[0]);
		u8g2.print(RX_Message[1]);
		u8g2.print(RX_Message[2], HEX);

		// Draw currently selected waveform above knob 1
		u8g2.drawXBM(38, 22, 13, 9, waveforms[K1.getRotation()]);

		// Print Send / Receive State above knob 2
		if (K2.getRotation()) {
			u8g2.drawStr(74, 30, "SEND");
		} else {
			u8g2.drawStr(74, 30, "RECV");
		}

		// Print currently selected volume level above knob 3
		u8g2.setCursor(116, 30);
		u8g2.print(K3.getRotation());

		u8g2.sendBuffer(); // transfer internal memory to the display
		digitalToggle(LED_BUILTIN);
	}
}

// Arduino framework setup function, sets up Pins, Display, UART, CAN and Tasks
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
#pragma region Variables Setup
	isMainSynth = true;
	octave = 4;
#pragma endregion
#pragma region Display Setup
	setOutMuxBit(DRST_BIT, LOW); // Assert display logic reset
	delayMicroseconds(2);
	setOutMuxBit(DRST_BIT, HIGH); // Release display logic reset
	u8g2.begin();
	setOutMuxBit(DEN_BIT, HIGH); // Enable display power supply
#pragma endregion
#pragma region UART Setup
	Serial.begin(115200);
	Serial.println("Hello World");
#pragma endregion
#pragma region CAN Setup
	msgInQ = xQueueCreate(36, 8);
	CAN_Init(true);
	setCANFilter(0x123, 0x7ff);
	CAN_RegisterRX_ISR(CAN_RX_ISR);
	CAN_Start();
#pragma endregion
#pragma region Task Scheduler Setup
	TIM_TypeDef *Instance = TIM1;
	HardwareTimer *sampleTimer = new HardwareTimer(Instance);
	sampleTimer->setOverflow(samplingRate, HERTZ_FORMAT);
	sampleTimer->attachInterrupt(sampleISR);
	sampleTimer->resume();
	TaskHandle_t scanKeysHandle = nullptr;
	TaskHandle_t displayUpdateHandle = nullptr;
	TaskHandle_t decodeHandle = nullptr;
	xTaskCreate(
		scanKeysTask,	// Function that implements the task
		"scanKeys",		// Text name for the task
		64,				// Stack size in words, not bytes
		nullptr,		// Parameter passed into the task
		3,				// Task priority
		&scanKeysHandle // Pointer to store the task handle
	);
	xTaskCreate(
		decodeTask,	  // Function that implements the task
		"decode",	  // Text name for the task
		256,		  // Stack size in words, not bytes
		nullptr,	  // Parameter passed into the task
		2,			  // Task priority
		&decodeHandle // Pointer to store the task handle
	);
	xTaskCreate(
		displayUpdateTask,	 // Function that implements the task
		"displayUpdate",	 // Text name for the task
		256,				 // Stack size in words, not bytes
		nullptr,			 // Parameter passed into the task
		1,					 // Task priority
		&displayUpdateHandle // Pointer to store the task handle
	);
	vTaskStartScheduler();
#pragma endregion
}

void loop() {} // No code in loop, as everything is done in the tasks
