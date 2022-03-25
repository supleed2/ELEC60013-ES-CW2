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
std::atomic<int32_t> currentStepSize[12];
std::atomic<uint8_t> keyArray[7];
std::atomic<uint8_t> octave;
std::atomic<int8_t> volume;
std::atomic<bool> volumeFiner;
std::atomic<int8_t> wave;
std::atomic<uint16_t> pressedKeys;
int8_t volumeHistory = 0;
QueueHandle_t msgInQ;
uint8_t RX_Message[8] = {0};
// Objects
U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0); // Display driver object
Knob K0(2,14,8); 	//Octave encoder
Knob K1(0,6); 		//Waveform encoder
Knob K3(0,10);		//Volume encoder
// Program Specific Structures
const int32_t notes[8][12] = {
	{0},
	{3185014, 3374405, 3575058, 3787642, 4012867, 4251484, 4504291, 4772130, 5055895, 5356535, 5675051, 6012507},
	{6370029, 6748811, 7150116, 7575284, 8025734, 8502969, 9008582, 9544260, 10111791, 10713070, 11350102, 12025014},
	{12740059, 13497622, 14300233, 15150569, 16051469, 17005939, 18017164, 19088521, 20223583, 21426140, 22700205, 24050029},
	{25480118, 26995245, 28600466, 30301138, 32102938, 34011878, 36034329, 38177042, 40447167, 42852281, 45400410, 48100059},
	{50960237, 53990491, 57200933, 60602277, 64205876, 68023756, 72068659, 76354085, 80894335, 85704562, 90800821, 96200119},
	{101920475, 107980982, 114401866, 121204555, 128411753, 136047513, 144137319, 152708170, 161788670, 171409125, 181601642, 192400238},
	{203840951, 215961965, 228803732, 242409110, 256823506, 272095026, 288274638, 305416340, 323577341, 342818251, 363203285, 384800476}
};
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
				topKey =  i * 4 + j;
			}
		}
	}
	return topKey;
}

// Returns integer where each bit represents a separate key (1 = pressed, 0 = not pressed)
uint16_t getKeys() {
	uint16_t keys = 0;
	for (uint8_t i = 0; i < 3; i++) {
		for (uint8_t j = 0; j < 4; j++) {
			if (keyArray[i] & (0x1 << j)) {
				keys += 0x1 << (j+i*4);
			}
		}
	}
	return keys;
}

int8_t getKeyCount() {
	uint8_t keyCount = 0;
	for (uint8_t i = 0; i < 3; i++) {
		for (uint8_t j = 0; j < 4; j++) {
			if (keyArray[i] & (0x1 << j)) {
				keyCount += 1;
			}
		}
	}
	return keyCount;
}

int32_t mixInputs(int32_t *accs) {
	int8_t keyCount = getKeyCount();
	int32_t output = 0;
	if(keyCount!=0){
		for(int8_t i = 0; i<12; i++){
			if(wave==SAWTOOTH){
				output += (*(accs+i)/keyCount) >> 16;
			}else if(wave==SQUARE){
				if(*(accs+i)<0){
					output += 0x8000/keyCount;
				}else{
					output += 0;
				}
			}else if(wave==TRIANGLE){
				//TODO
			}else if(wave==SINE){
				//TODO
			}
		}
	}
	return output;
}

// Interrupt driven routine to send waveform to DAC
void sampleISR(){
	static int32_t phaseAcc[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
	int8_t keyCount = getKeyCount();
	uint16_t keys = getKeys();
	Serial.print(1);
	for(int8_t i = 0; i<12; i++){
		if(keys & (0x1 << i)){
			phaseAcc[i] += notes[octave][i];
		}else{
			phaseAcc[i] = 0;
		}
	}
	int32_t Vout = mixInputs(phaseAcc);
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
		pressedKeys = getKeys();
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
		//u8g2.drawStr(2, 10, notes[key].note.c_str()); // Print the current key
		digitalToggle(LED_BUILTIN);
		u8g2.setCursor(2, 20);
		for (uint8_t i = 0; i < 7; i++) {
			u8g2.print(keyArray[i], HEX);
		};
		u8g2.setCursor(100, 10);
		u8g2.print((char)RX_Message[0]);
		u8g2.print(RX_Message[1]);
		u8g2.print(RX_Message[2], HEX);

		u8g2.setCursor(70, 30);
		u8g2.print(getKeys());

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
