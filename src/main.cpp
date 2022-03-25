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
std::atomic<int32_t> currentStepSize;
std::atomic<uint8_t> keyArray[7];
std::atomic<uint8_t> octave;
std::atomic<int8_t> volume;
std::atomic<bool> volumeFiner;
std::atomic<int8_t> wave;
int8_t volumeHistory = 0;
QueueHandle_t msgInQ;
uint8_t RX_Message[8] = {0};
// Objects
U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0); // Display driver object
Knob K0(2,14,8); 	//Octave encoder
Knob K1(0,6); 		//Waveform encoder
Knob K3(0,10);		//Volume encoder
// Program Specific Structures
typedef struct{
	int32_t stepSize;
	std::string note;
} Note;
const Note notes[] = {
	{0, "None"}, {3185014, "C1"}, {3374405, "C1#"}, {3575058, "D1"}, {3787642, "D1#"}, {4012867, "E1"}, {4251484, "F1"}, {4504291, "F1#"}, {4772130, "G1"}, {5055895, "G1#"}, {5356535, "A1"}, {5675051, "A1#"}, {6012507, "B1"}, {6370029, "C2"}, {6748811, "C2#"}, {7150116, "D2"}, {7575284, "D2#"}, {8025734, "E2"}, {8502969, "F2"}, {9008582, "F2#"}, {9544260, "G2"}, {10111791, "G2#"}, {10713070, "A2"}, {11350102, "A2#"}, {12025014, "B2"}, {12740059, "C3"}, {13497622, "C3#"}, {14300233, "D3"}, {15150569, "D3#"}, {16051469, "E3"}, {17005939, "F3"}, {18017164, "F3#"}, {19088521, "G3"}, {20223583, "G3#"}, {21426140, "A3"}, {22700205, "A3#"}, {24050029, "B3"}, {25480118, "C4"}, {26995245, "C4#"}, {28600466, "D4"}, {30301138, "D4#"}, {32102938, "E4"}, {34011878, "F4"}, {36034329, "F4#"}, {38177042, "G4"}, {40447167, "G4#"}, {42852281, "A4"}, {45400410, "A4#"}, {48100059, "B4"}, {50960237, "C5"}, {53990491, "C5#"}, {57200933, "D5"}, {60602277, "D5#"}, {64205876, "E5"}, {68023756, "F5"}, {72068659, "F5#"}, {76354085, "G5"}, {80894335, "G5#"}, {85704562, "A5"}, {90800821, "A5#"}, {96200119, "B5"}, {101920475, "C6"}, {107980982, "C6#"}, {114401866, "D6"}, {121204555, "D6#"}, {128411753, "E6"}, {136047513, "F6"}, {144137319, "F6#"}, {152708170, "G6"}, {161788670, "G6#"}, {171409125, "A6"}, {181601642, "A6#"}, {192400238, "B6"}, {203840951, "C7"}, {215961965, "C7#"}, {228803732, "D7"}, {242409110, "D7#"}, {256823506, "E7"}, {272095026, "F7"}, {288274638, "F7#"}, {305416340, "G7"}, {323577341, "G7#"}, {342818251, "A7"}, {363203285, "A7#"}, {384800476, "B7"}};
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
const int16_t sineLookUpTable[] = {
  128,131,134,137,140,143,146,149,152,156,159,162,165,168,171,174,
  176,179,182,185,188,191,193,196,199,201,204,206,209,211,213,216,
  218,220,222,224,226,228,230,232,234,236,237,239,240,242,243,245,
  246,247,248,249,250,251,252,252,253,254,254,255,255,255,255,255,
  255,255,255,255,255,255,254,254,253,252,252,251,250,249,248,247,
  246,245,243,242,240,239,237,236,234,232,230,228,226,224,222,220,
  218,216,213,211,209,206,204,201,199,196,193,191,188,185,182,179,
  176,174,171,168,165,162,159,156,152,149,146,143,140,137,134,131,
  128,124,121,118,115,112,109,106,103,99, 96, 93, 90, 87, 84, 81,
  79, 76, 73, 70, 67, 64, 62, 59, 56, 54, 51, 49, 46, 44, 42, 39,
  37, 35, 33, 31, 29, 27, 25, 23, 21, 19, 18, 16, 15, 13, 12, 10,
  9,  8,  7,  6,  5,  4,  3,  3,  2,  1,  1,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  1,  1,  2,  3,  3,  4,  5,  6,  7,  8,
  9,  10, 12, 13, 15, 16, 18, 19, 21, 23, 25, 27, 29, 31, 33, 35,
  37, 39, 42, 44, 46, 49, 51, 54, 56, 59, 62, 64, 67, 70, 73, 76,
  79, 81, 84, 87, 90, 93, 96, 99, 103,106,109,112,115,118,121,124
};
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


uint32_t scaleVolume(uint32_t Vout){
	uint32_t newVout = 0;
	if(volumeFiner){
		newVout = (Vout*12*volume) >> 16;
	}else{ // 25 = floor( (1/10) << 8 )
		newVout = (Vout*25*volume) >> 16; 	//scale by 2*8 cuz 16-bit*8-bit=24-bit -> scale by 16 to get to 8
	}
	return newVout;
}

// Returns key value (as notes[] index) of highest currently pressed key
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
void sampleISR(){
	static int32_t phaseAcc = 0;
	phaseAcc += currentStepSize;
	int32_t Vout = 0;
	if(wave==SAWTOOTH){
		Vout = phaseAcc >> 16; 
	}else if(wave==SQUARE){
		if(phaseAcc<0){
			Vout = 0x00007FFF;
		}else{
			Vout = 0xFFFF8000;
		}
	}else if(wave==TRIANGLE){
		Vout = (abs(phaseAcc)-1073741824) >> 15;
	}else if(wave==SINE){
		Vout = (sineLookUpTable[(uint32_t)phaseAcc>>24]-128)<<8;
	}
	Vout = scaleVolume(Vout);
	analogWrite(OUTR_PIN, Vout + 128);
}

// void CAN_RX_ISR() {
// 	uint8_t ISR_RX_Message[8];
// 	uint32_t ISR_rxID;
// 	CAN_RX(ISR_rxID, ISR_RX_Message);
// 	xQueueSendFromISR(msgInQ, ISR_RX_Message, nullptr);
// }

// void decodeTask(void *pvParameters) {
// 	while (1) {
// 		xQueueReceive(msgInQ, RX_Message, portMAX_DELAY);
// 		if (RX_Message[0] == 0x50) { // Pressed
// 			currentStepSize = notes[(RX_Message[1] - 1) * 12 + RX_Message[2]].stepSize;
// 		} else { // Released
// 			currentStepSize = 0;
// 		}
// 	}
// }

// void keyChangedSendTXMessage(uint8_t octave, uint8_t key, bool pressed) {
// 	uint8_t TX_Message[8] = {0};
// 	if (pressed) {
// 		TX_Message[0] = 0x50; // "P"
// 	} else {
// 		TX_Message[0] = 0x52; // "R"
// 	}
// 	TX_Message[1] = octave;
// 	TX_Message[2] = key;
// 	CAN_TX(canID, TX_Message);
// }

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
				for (uint8_t j = 0; j < 4; j++) {
					if ((oldRow & (0x1 << j)) ^ (newRow & (0x1 << j))) {
						//keyChangedSendTXMessage(octave, i * 4 + j + 1, newRow & (0x1 << j));
					}
				}
			}
		};
		if(volumeFiner){
			K3.changeLimitsVolume(0,20);
		}else{
			K3.changeLimitsVolume(0,10);
		};
		currentStepSize = notes[getTopKey()].stepSize; // Atomic Store
		K0.updateRotation(keyArray[4] & 0x4, keyArray[4] & 0x8);
		octave = K0.getRotation()/2;
		K1.updateRotation(keyArray[4] & 0x1, keyArray[4] & 0x2);
		wave = K1.getRotation()/2;
		K3.updateRotation(keyArray[3] & 0x1, keyArray[3] & 0x2);
		volume = K3.getRotation();
		volumeHistory = (volumeHistory << 1) + ((keyArray[5]&0x2)>>1);
		volumeFiner = ((!(volumeHistory==1))&volumeFiner) | ((volumeHistory==1)&!volumeFiner);
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
		uint16_t key = getTopKey();
		u8g2.drawStr(2, 10, notes[key].note.c_str()); // Print the current key
		digitalToggle(LED_BUILTIN);
		u8g2.setCursor(2, 20);
		for (uint8_t i = 0; i < 7; i++) {
			u8g2.print(keyArray[i], HEX);
		};
		u8g2.setCursor(100, 10);
		u8g2.print((char)RX_Message[0]);
		u8g2.print(RX_Message[1]);
		u8g2.print(RX_Message[2], HEX);

		// Print waveform icon
		int K1_rot = K1.getRotation();
		if(K1_rot<2){
			u8g2.drawXBM(38,22,13,9,waveforms[SQUARE]);
		}else if(K1_rot<4){
			u8g2.drawXBM(38,22,13,9,waveforms[SAWTOOTH]);
		}else if(K1_rot<6){
			u8g2.drawXBM(38,22,13,9,waveforms[TRIANGLE]);
		}else if(K1_rot==6){
			u8g2.drawXBM(38,22,13,9,waveforms[SINE]);
		};

		// Print octave number
		u8g2.setCursor(2, 30);
		u8g2.print("O:");
		u8g2.setCursor(14, 30);
		u8g2.print(octave);

		// Print volume indicator
		if(!volumeFiner){
			if(volume==0){
				u8g2.drawXBM(116,22,13,9,volumes[5]);
			}else if(volume<3){
				u8g2.drawXBM(116,22,13,9,volumes[4]);
			}else if(volume<5){
				u8g2.drawXBM(116,22,13,9,volumes[3]);
			}else if(volume<7){
				u8g2.drawXBM(116,22,13,9,volumes[2]);
			}else if(volume<9){
				u8g2.drawXBM(116,22,13,9,volumes[1]);
			}else if(volume<11){
				u8g2.drawXBM(116,22,13,9,volumes[0]);
			};
		}else{
			u8g2.setCursor(117, 30);
			u8g2.print(volume/2);
		};
		u8g2.sendBuffer(); // transfer internal memory to the display
	}
}

void setup() {
	octave = 4;
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
#pragma region UART Setup
	Serial.begin(115200);
	Serial.println("Hello World");
#pragma endregion
// #pragma region CAN Setup
// 	msgInQ = xQueueCreate(36, 8);
// 	CAN_Init(true);
// 	setCANFilter(0x123, 0x7ff);
// 	CAN_RegisterRX_ISR(CAN_RX_ISR);
// 	CAN_Start();
// #pragma endregion
#pragma region Task Scheduler Setup
	TIM_TypeDef *Instance = TIM1;
	HardwareTimer *sampleTimer = new HardwareTimer(Instance);
	sampleTimer->setOverflow(samplingRate, HERTZ_FORMAT);
	sampleTimer->attachInterrupt(sampleISR);
	sampleTimer->resume();
	TaskHandle_t scanKeysHandle = nullptr;
	TaskHandle_t displayUpdateHandle = nullptr;
	xTaskCreate(
		scanKeysTask,	// Function that implements the task
		"scanKeys",		// Text name for the task
		64,				// Stack size in words, not bytes
		nullptr,		// Parameter passed into the task
		2,				// Task priority
		&scanKeysHandle // Pointer to store the task handle
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
