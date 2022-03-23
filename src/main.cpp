// https://github.com/adamb314/ThreadHandler

#include <Arduino.h>
#include <U8g2lib.h>
#include <STM32FreeRTOS.h>
#include "knob.h"

volatile int32_t currentStepSize;
volatile uint8_t keyArray[7];
volatile int8_t volume;
volatile bool volumeFiner;
int8_t volumeHistory = 0;
SemaphoreHandle_t keyArrayMutex;
Knob K1(0,6);
Knob K3(0,10);
enum wave{SQR=0,SAW,TRI,SIN};


#pragma region Config Values
const uint32_t interval = 10;		 // Display update interval
const uint8_t octave = 4;			 // Octave to start on
const uint32_t samplingRate = 44100; // Sampling rate
const int32_t stepSizes[] = {
	0, 6370029, 6748811, 7150116, 7575284, 8025734, 8502969, 9008582,
	9544260, 10111791, 10713070, 11350102, 12025014, 12740059, 13497622,
	14300233, 15150569, 16051469, 17005939, 18017164, 19088521, 20223583,
	21426140, 22700205, 24050029, 25480118, 26995245, 28600466, 30301138,
	32102938, 34011878, 36034329, 38177042, 40447167, 42852281, 45400410,
	48100059, 50960237, 53990491, 57200933, 60602277, 64205876, 68023756,
	72068659, 76354085, 80894335, 85704562, 90800821, 96200119, 101920475,
	107980982, 114401866, 121204555, 12841175, 136047513, 144137319, 152708170,
	161788670, 171409125, 181601642, 192400238}; // Step sizes for each note
static unsigned char waveforms[4][18] = {
	{0x7f, 0x10, 0x41, 0x10, 0x41, 0x10, 0x41, 0x10, 0x41, 0x10, 0x41, 0x10,
	0x41, 0x10, 0x41, 0x10, 0xc1, 0x1f}, //square wave
	{0x70, 0x10, 0x58, 0x18, 0x48, 0x08, 0x4c, 0x0c, 0x44, 0x04, 0x46, 0x06,
   	0x42, 0x02, 0x43, 0x03, 0xc1, 0x01}, //sawtooth wave
	{0x08, 0x00, 0x1c, 0x00, 0x36, 0x00, 0x63, 0x00, 0xc1, 0x00, 0x80, 0x11,
	0x00, 0x1b, 0x00, 0x0e, 0x00, 0x04}, //triange wave
	{0x1c, 0x00, 0x36, 0x00, 0x22, 0x00, 0x63, 0x00, 0x41, 0x10, 0xc0, 0x18,
   	0x80, 0x08, 0x80, 0x0d, 0x00, 0x07} //sine wave
};
static unsigned char volumes[6][18] = {
	{0x10, 0x02, 0x98, 0x04, 0x1c, 0x05, 0x5f, 0x09, 0x5f, 0x09, 0x5f, 0x09,
   	0x1c, 0x05, 0x98, 0x04, 0x10, 0x02 }, //volume max
	{0x10, 0x00, 0x98, 0x00, 0x1c, 0x01, 0x5f, 0x01, 0x5f, 0x01, 0x5f, 0x01,
   	0x1c, 0x01, 0x98, 0x00, 0x10, 0x00 }, //volume mid higher
	{0x10, 0x00, 0x18, 0x00, 0x1c, 0x01, 0x5f, 0x01, 0x5f, 0x01, 0x5f, 0x01,
   	0x1c, 0x01, 0x18, 0x00, 0x10, 0x00 }, //volume mid lower
	{0x10, 0x00, 0x18, 0x00, 0x1c, 0x00, 0x5f, 0x00, 0x5f, 0x00, 0x5f, 0x00,
   	0x1c, 0x00, 0x18, 0x00, 0x10, 0x00 }, //volume low
	{0x10, 0x00, 0x18, 0x00, 0x1c, 0x00, 0x1f, 0x00, 0x5f, 0x00, 0x1f, 0x00,
   	0x1c, 0x00, 0x18, 0x00, 0x10, 0x00 }, //volume lowest
	{0x10, 0x00, 0x18, 0x00, 0x5c, 0x04, 0x9f, 0x02, 0x1f, 0x01, 0x9f, 0x02,
   	0x5c, 0x04, 0x18, 0x00, 0x10, 0x00} //mute
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

uint16_t getTopKey(volatile uint8_t array[]) {
	uint16_t topKey = 0;
	for (uint8_t i = 0; i < 3; i++) {
		for (uint8_t j = 0; j < 4; j++) {
			if (array[i] & (0x1 << j)) {
				topKey = (octave - 2) * 12 + i * 4 + j + 1;
			}
		}
	}
	return topKey;
}

void sampleISR(){
	static int32_t phaseAcc = 0;
	phaseAcc += currentStepSize;
	int32_t Vout = phaseAcc >> 16; 
	if(volumeFiner){
		Vout = (Vout*12*volume) >> 16;
	}else{ // 25 = floor( (1/10) << 8 )
		Vout = (Vout*25*volume) >> 16; 	//scale by 2*8 cuz 16-bit*8-bit=24-bit -> scale by 16 to get to 8
	}
	analogWrite(OUTR_PIN, Vout + 128);
}

void scanKeysTask(void * pvParameters){
	uint8_t keyArrayCopy[7];
	bool volumeFinerNext;
	const TickType_t xFrequency = 50/portTICK_PERIOD_MS;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while(1){
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		for (uint8_t i = 0; i < 7; i++) {
			setRow(i);
			delayMicroseconds(3);
			keyArrayCopy[i] = readCols();
		}
		xSemaphoreTake(keyArrayMutex, portMAX_DELAY);
		memcpy((void*)keyArray, keyArrayCopy, 7);
		xSemaphoreGive(keyArrayMutex);
		digitalToggle(LED_BUILTIN);
		__atomic_store_n(&currentStepSize, stepSizes[getTopKey(keyArrayCopy)], __ATOMIC_RELAXED);
		K1.updateRotation(keyArrayCopy[4] & 0x1, keyArrayCopy[4] & 0x2);
		if(volumeFiner){
			K3.changeLimitsVolume(0,20);
		}else{
			K3.changeLimitsVolume(0,10);
		}
		K3.updateRotation(keyArrayCopy[3] & 0x1, keyArrayCopy[3] & 0x2);
		__atomic_store_n(&volume, K3.getRotation(), __ATOMIC_RELAXED);
		volumeHistory = (volumeHistory << 1) + ((keyArrayCopy[5]&0x2)>>1);
		volumeFinerNext = ((!(volumeHistory==1))&volumeFiner) | ((volumeHistory==1)&!volumeFiner);
		__atomic_store_n(&volumeFiner, volumeFinerNext, __ATOMIC_RELAXED);
	}
}

void displayUpdateTask(void * pvParameters){
	uint8_t keyArrayCopy[7];
	const TickType_t xFrequency = 100/portTICK_PERIOD_MS;
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while(1){
		vTaskDelayUntil(&xLastWakeTime, xFrequency);

		xSemaphoreTake(keyArrayMutex, portMAX_DELAY);
		memcpy(keyArrayCopy, (void*)keyArray, 7);
		xSemaphoreGive(keyArrayMutex);

		u8g2.clearBuffer();							// clear the internal memory
		u8g2.setFont(u8g2_font_profont12_mf);		// choose a suitable font
		u8g2.setCursor(2, 10);						// set the cursor position
		u8g2.print(currentStepSize); // Print the current frequency
		//digitalToggle(LED_BUILTIN);
		u8g2.setCursor(2, 20);
		for (uint8_t i = 0; i < 7; i++) {
			u8g2.print(keyArrayCopy[6-i], HEX);
		}
		// Print waveform icon
		int K1_rot = K1.getRotation();
		if(K1_rot<2){
			u8g2.drawXBM(38,22,13,9,waveforms[SQR]);
		}else if(K1_rot<4){
			u8g2.drawXBM(38,22,13,9,waveforms[SAW]);
		}else if(K1_rot<6){
			u8g2.drawXBM(38,22,13,9,waveforms[TRI]);
		}else if(K1_rot==6){
			u8g2.drawXBM(38,22,13,9,waveforms[SIN]);
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
		scanKeysTask, /* Function that implements the task */
		"scanKeys", /* Text name for the task */
		64, /* Stack size in words, not bytes */
		NULL, /* Parameter passed into the task */
		2, /* Task priority */
		&scanKeysHandle /* Pointer to store the task handle */
	);
	xTaskCreate(
		displayUpdateTask, /* Function that implements the task */
		"displayUpdate", /* Text name for the task */
		256, /* Stack size in words, not bytes */
		NULL, /* Parameter passed into the task */
		1, /* Task priority */
		&displayUpdateHandle /* Pointer to store the task handle */
	);

	keyArrayMutex = xSemaphoreCreateMutex();

	vTaskStartScheduler();
}

void loop() {}