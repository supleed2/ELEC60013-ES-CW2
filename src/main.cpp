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
const uint32_t samplingRate = 44100; // Sampling rate
const uint32_t canID = 0x123;
// Variables
std::atomic<bool> isMainSynth;
std::atomic<int32_t> currentStepSize;
std::atomic<uint8_t> keyArray[7];
std::atomic<uint8_t> octave;
std::atomic<uint8_t> selectedWaveform;
std::atomic<int> latestKey;
std::atomic<int8_t> volume;
std::atomic<bool> volumeFiner;
std::atomic<bool> handshakeEastOut;
std::atomic<bool> handshakeWestOut;
int8_t volumeHistory = 0;
QueueHandle_t msgInQ;
std::atomic<bool> bufferAactive;
int32_t bufferA[220];
int32_t bufferB[220];
// Objects
U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0); // Display Driver Object
Knob K0(1, 7, 4);								   // Octave Knob Object
Knob K1(0, 3, 1);								   // Waveform Knob Object
Knob K2(0, 1);									   // Send / Receive Knob Object
Knob K3(0, 16);									   // Volume Knob Object
// Program Specific Structures
const int32_t stepSizes[85] = {0, 3185014, 3374405, 3575058, 3787642, 4012867, 4251484, 4504291, 4772130, 5055895, 5356535, 5675051, 6012507, 6370029, 6748811, 7150116, 7575284, 8025734, 8502969, 9008582, 9544260, 10111791, 10713070, 11350102, 12025014, 12740059, 13497622, 14300233, 15150569, 16051469, 17005939, 18017164, 19088521, 20223583, 21426140, 22700205, 24050029, 25480118, 26995245, 28600466, 30301138, 32102938, 34011878, 36034329, 38177042, 40447167, 42852281, 45400410, 48100059, 50960237, 53990491, 57200933, 60602277, 64205876, 68023756, 72068659, 76354085, 80894335, 85704562, 90800821, 96200119, 101920475, 107980982, 114401866, 121204555, 128411753, 136047513, 144137319, 152708170, 161788670, 171409125, 181601642, 192400238, 203840951, 215961965, 228803732, 242409110, 256823506, 272095026, 288274638, 305416340, 323577341, 342818251, 363203285, 384800476};
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
	{0x10, 0x00, 0x18, 0x00, 0x5c, 0x04, 0x9f, 0x02, 0x1f,
	 0x01, 0x9f, 0x02, 0x5c, 0x04, 0x18, 0x00, 0x10, 0x00}, // mute
	{0x10, 0x00, 0x18, 0x00, 0x1c, 0x00, 0x1f, 0x00, 0x5f,
	 0x00, 0x1f, 0x00, 0x1c, 0x00, 0x18, 0x00, 0x10, 0x00}, // volume lowest
	{0x10, 0x00, 0x18, 0x00, 0x1c, 0x00, 0x5f, 0x00, 0x5f,
	 0x00, 0x5f, 0x00, 0x1c, 0x00, 0x18, 0x00, 0x10, 0x00}, // volume low
	{0x10, 0x00, 0x18, 0x00, 0x1c, 0x01, 0x5f, 0x01, 0x5f,
	 0x01, 0x5f, 0x01, 0x1c, 0x01, 0x18, 0x00, 0x10, 0x00}, // volume mid lower
	{0x10, 0x00, 0x98, 0x00, 0x1c, 0x01, 0x5f, 0x01, 0x5f,
	 0x01, 0x5f, 0x01, 0x1c, 0x01, 0x98, 0x00, 0x10, 0x00}, // volume mid higher
	{0x10, 0x02, 0x98, 0x04, 0x1c, 0x05, 0x5f, 0x09, 0x5f,
	 0x09, 0x5f, 0x09, 0x1c, 0x05, 0x98, 0x04, 0x10, 0x02}, // volume max
};
const unsigned char icon_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0xcc, 0x00, 0xcc, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x01, 0x02, 0x01, 0xfe, 0x01, 0x00, 0x00};
const int8_t sinLUT[256] = {
	0, 3, 6, 9, 12, 15, 18, 21, 24, 28, 31, 34, 37, 40, 43, 46,
	48, 51, 54, 57, 60, 63, 65, 68, 71, 73, 76, 78, 81, 83, 85, 88,
	90, 92, 94, 96, 98, 100, 102, 104, 106, 108, 109, 111, 112, 114, 115, 117,
	118, 119, 120, 121, 122, 123, 124, 124, 125, 126, 126, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 126, 126, 125, 124, 124, 123, 122, 121, 120, 119,
	118, 117, 115, 114, 112, 111, 109, 108, 106, 104, 102, 100, 98, 96, 94, 92,
	90, 88, 85, 83, 81, 78, 76, 73, 71, 68, 65, 63, 60, 57, 54, 51,
	48, 46, 43, 40, 37, 34, 31, 28, 24, 21, 18, 15, 12, 9, 6, 3,
	0, -4, -7, -10, -13, -16, -19, -22, -25, -29, -32, -35, -38, -41, -44, -47,
	-49, -52, -55, -58, -61, -64, -66, -69, -72, -74, -77, -79, -82, -84, -86, -89,
	-91, -93, -95, -97, -99, -101, -103, -105, -107, -109, -110, -112, -113, -115, -116, -118,
	-119, -120, -121, -122, -123, -124, -125, -125, -126, -127, -127, -128, -128, -128, -128, -128,
	-128, -128, -128, -128, -128, -128, -127, -127, -126, -125, -125, -124, -123, -122, -121, -120,
	-119, -118, -116, -115, -113, -112, -110, -109, -107, -105, -103, -101, -99, -97, -95, -93,
	-91, -89, -86, -84, -82, -79, -77, -74, -72, -69, -66, -64, -61, -58, -55, -52,
	-49, -47, -44, -41, -38, -35, -32, -29, -25, -22, -19, -16, -13, -10, -7, -4};
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

// Set multiplexer bits to select row, and set output from multiplexer
void setRow(const uint8_t rowIdx, const bool value) {
	digitalWrite(REN_PIN, LOW);
	digitalWrite(RA0_PIN, rowIdx & 0x01);
	digitalWrite(RA1_PIN, rowIdx & 0x02);
	digitalWrite(RA2_PIN, rowIdx & 0x04);
	digitalWrite(OUT_PIN, value);
	digitalWrite(REN_PIN, HIGH);
}

// Scales output signal according to global volume value, to range 0-255
uint32_t scaleVolume(uint32_t Vout) {
	uint32_t newVout = 0;
	if (volumeFiner) {
		newVout = (Vout * 12 * volume) >> 16;
	} else {								  // 25 = floor( (1/10) << 8 )
		newVout = (Vout * 25 * volume) >> 16; // scale by 2*8 cuz 16-bit*8-bit=24-bit -> scale by 16 to get to 8
	}
	return newVout;
}

// Interrupt driven routine to send waveform to DAC
void sampleISR() {
	static int32_t phaseAcc = 0;
	phaseAcc += currentStepSize;
	int32_t Vout = 0;
	if (selectedWaveform == SAWTOOTH) {
		Vout = phaseAcc >> 16;
	} else if (selectedWaveform == SQUARE) {
		if (phaseAcc < 0) {
			Vout = 0x00007FFF;
		} else {
			Vout = 0xFFFF8000;
		}
	} else if (selectedWaveform == TRIANGLE) {
		Vout = (abs(phaseAcc) - 1073741824) >> 15;
	} else if (selectedWaveform == SINE) {
		Vout = (sinLUT[(uint32_t)phaseAcc >> 24]) << 8;
	}
	Vout = scaleVolume(Vout);
	analogWrite(OUTR_PIN, Vout + 128);
}

// Interrupt service routine that copies received CAN messages to (larger) internal buffer when available
void CAN_RX_ISR() {
	uint8_t ISR_RX_Message[8];
	uint32_t ISR_rxID;
	CAN_RX(ISR_rxID, ISR_RX_Message);
	if (isMainSynth)
		xQueueSendFromISR(msgInQ, ISR_RX_Message, nullptr);
}

// Task to update activeNotes[] and currentStepSize based on received CAN message
void decodeTask(void *pvParameters) {
	static uint8_t RX_Message[8] = {0};
	while (1) {
		xQueueReceive(msgInQ, RX_Message, portMAX_DELAY);
		if (RX_Message[0] == 0x50) { // Pressed
			activeNotes[(RX_Message[1] - 1) * 12 + RX_Message[2]] = true;
			latestKey = (RX_Message[1] - 1) * 12 + RX_Message[2];
			currentStepSize = stepSizes[latestKey];
		} else if (RX_Message[0] == 0x52) { // Released
			activeNotes[(RX_Message[1] - 1) * 12 + RX_Message[2]] = false;
			if (latestKey == (RX_Message[1] - 1) * 12 + RX_Message[2]) {
				latestKey = 0;
				currentStepSize = stepSizes[latestKey]; // Atomic Store
			}
		} else if (RX_Message[0] == 0x4D) { // Main Synth Announce
			isMainSynth = false;
			K2.setRotation(1);
		}
	}
}

// Function to send a CAN message containing a changed key and it's new state
void keyChangedSendTXMessage(uint8_t octave, uint8_t key, bool pressed) {
	uint8_t TX_Message[8] = {0};
	if (pressed) {
		TX_Message[0] = 0x50; // "P"
	} else {
		TX_Message[0] = 0x52; // "R"
	}
	TX_Message[1] = octave;
	TX_Message[2] = key;
	if (isMainSynth) {
		xQueueSend(msgInQ, TX_Message, 0);
	} else {
		CAN_TX(canID, TX_Message);
	}
}

// Function to send CAN message instructing other synths to change to sending mode
void announceMainSynth() {
	uint8_t TX_Message[8] = {0};
	TX_Message[0] = 0x4D; // "M"
	CAN_TX(canID, TX_Message);
}

// Task to update keyArray values at a higher priority
void scanKeysTask(void *pvParameters) {
	const TickType_t xFrequency = 20 / portTICK_PERIOD_MS;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while (1) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		for (uint8_t i = 0; i < 7; i++) {
			switch (i) {
				case 3: // Display Power
				case 4: // Display Reset, active low
					setRow(i, HIGH);
					break;
				case 5: // Handshake Output West
					setRow(i, handshakeWestOut);
					break;
				case 6: // Handshake Output East
					setRow(i, handshakeEastOut);
					break;
				default: // Unimplemented
					setRow(i, LOW);
					break;
			}
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
		if (keyArray[5] & 0x1 && isMainSynth) {
			announceMainSynth();
		}
		if (volumeFiner) {
			K3.changeLimitsVolume(0, 20);
		} else {
			K3.changeLimitsVolume(0, 5);
		}
		K0.updateRotation(keyArray[4] & 0x4, keyArray[4] & 0x8);
		K1.updateRotation(keyArray[4] & 0x1, keyArray[4] & 0x2);
		K2.updateRotation(keyArray[3] & 0x4, keyArray[3] & 0x8);
		K3.updateRotation(keyArray[3] & 0x1, keyArray[3] & 0x2);
		octave = K0.getRotation();
		selectedWaveform = K1.getRotation();
		isMainSynth = !K2.getRotation();
		volume = K3.getRotation();
		volumeHistory = (volumeHistory << 1) + ((keyArray[5] & 0x2) >> 1);
		volumeFiner = ((!(volumeHistory == 1)) & volumeFiner) | ((volumeHistory == 1) & !volumeFiner);
	}
}

// Task containing display graphics and update code
void displayUpdateTask(void *pvParameters) {
	const TickType_t xFrequency = 100 / portTICK_PERIOD_MS;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while (1) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		u8g2.clearBuffer();					   // clear the internal memory
		u8g2.setFont(u8g2_font_profont12_mf);  // choose a suitable font
		u8g2.drawStr(2, 10, notes[latestKey]); // Print the currently pressed keys

		u8g2.setCursor(2, 20);
		for (uint8_t i = 0; i < 7; i++) {
			u8g2.print(keyArray[i], HEX);
		};
		// u8g2.setCursor(100, 10); // Debug print of received CAN message
		// u8g2.print((char)RX_Message[0]);
		// u8g2.print(RX_Message[1]);
		// u8g2.print(RX_Message[2], HEX);

		// Print current octave number above knob 0
		u8g2.drawStr(2, 30, "O:");
		u8g2.setCursor(14, 30);
		u8g2.print(octave);

		// Draw currently selected waveform above knob 1
		u8g2.drawXBM(38, 22, 13, 9, waveforms[K1.getRotation()]);

		// Print Send / Receive State above knob 2
		if (K2.getRotation()) {
			u8g2.drawStr(74, 30, "SEND");
		} else {
			u8g2.drawStr(74, 30, "RECV");
		}

		// Print volume indicator above knob 3
		if (!volumeFiner) {
			u8g2.drawXBM(112, 22, 13, 9, volumes[volume]);
		} else {
			u8g2.setCursor(117, 30);
			u8g2.print(volume);
		}

		if (!isMainSynth) {
			u8g2.drawHLine(1, 6, 26);
			u8g2.drawHLine(36, 26, 18);
			u8g2.drawHLine(110, 26, 16);
			u8g2.drawStr(40, 10, "Secondary Mode");
		}

		u8g2.sendBuffer();			// transfer internal memory to the display
		digitalToggle(LED_BUILTIN); // Toggle LED to show display update rate
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
	handshakeWestOut = false;
	handshakeEastOut = true;
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
	CAN_Init(false);
	setCANFilter(canID, 0x7fc); // Mask last 2 bits
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
