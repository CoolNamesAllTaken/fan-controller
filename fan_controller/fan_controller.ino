#include <Wire.h> // I2C communication library
#include <LiquidCrystal_I2C.h> // LCD w/ I2C backpack library
#include <ClickEncoder.h> // rotary encoder with the clicky bit library
#include <TimerOne.h> // library for timer interrupts
#include <DHT.h> // Adafruit DHT sensor library
#include <EEPROM.h> // Arduino built-in EEPROM library for saving PID gains

#define DHT_PIN 4 // AM2302 Sensor on pin D3
#define DHT_TYPE DHT22 // AM2302

#define ENCODER_PIN_A A1
#define ENCODER_PIN_B A0
#define ENCODER_PIN_BTN A2
#define ENCODER_STEPS_PER_NOTCH 1

#define MOSFET_PIN 3 // output pin connected to gate of N-channel MOSFET

// P, I, D gains are each floats (4 bytes)
#define TEMP_CONTROLLER_P_ADDR 0
#define TEMP_CONTROLLER_I_ADDR 4
#define TEMP_CONTROLLER_D_ADDR 8
#define TEMP_CONTROLLER_TIME_INTERVAL_ADDR 12
#define TEMP_CONTROLLER_RESPONSE_SCALER_ADDR 14

#define TEMP_CONTROLLER_PWM_REGISTER OCR2B

#define NUM_PARAMS 5 // P, I, D, Scaler, Time Interval
#define TEMP_CONTROLLER_PID_INCREMENT 0.1 // add / subtract
#define TEMP_CONTROLLER_TIME_INTERVAL_INCREMENT 1 // add / subtract seconds

#define TEMP_CONTROLLER_NEUTRAL_PWM 120 // neutral duty cycle of fan pwm register
#define TEMP_ERR_HISTORY_LENGTH 10 // number of time captures to use for I gain etc.

LiquidCrystal_I2C lcd(0x27,16,2); // set the LCD address to 0x27 for a 16 chars and 2 line display
DHT dht(DHT_PIN, DHT_TYPE); // set DHT sensor
ClickEncoder* encoder;

int setTemp = 30; // set temperature [deg. C]
char* mode = " ON";

// Error variables used for PID control
int tempErrHistory [TEMP_ERR_HISTORY_LENGTH];
int tempErrHistoryInd = 0;

// Last time (in milliseconds) that integral and derivative error were recorded
unsigned long lastErrMillis = 0;

// PID gains for temperature controller
float tempControllerP = 0.0f;
float tempControllerI = 0.0f;
float tempControllerD = 0.0f;

int tempControllerTimeInterval; // in seconds
float tempControllerResponseScaler; // scales the response to avoid bottoming out

// Params menu variables
int selectedParam = 0; // 0 = none, 1 = P, 2 = I, 3 = D, 4 = response scaler, 5 = time interval
const int paramCursorXPos[NUM_PARAMS] = {0, 5, 10, 0, 9};
const int paramCursorYPos[NUM_PARAMS] = {0, 0, 0, 1, 1};
bool modifiedParam = false; // flag for re-drawing PID param screen

void timerIsr() {
	encoder->service();
}

void setup()
{
	Serial.begin(9600); // for debugging

	// Set PWM fan control on timer 2, pin 3 (OC2B) to not mess with timer 1
	pinMode(MOSFET_PIN, OUTPUT);
	digitalWrite(MOSFET_PIN, LOW); // stop fan from turning on
	TCCR2A = 0; // clear first settings register for timer 2
	TCCR2B = 0; // clear second settings register for timer 2
	TCCR2A = _BV(WGM21) | _BV(WGM20) | _BV(COM2B1); // | _BV(COM2A1)
	// WGM2 = 0b011 (timer 2 in fast pwm mode)
	// COM2B = 0b10 (timer 2 channel b in non-inverting mode)
	// COM2A = 0b10 (timer 2 channel a in non-inverting mode), not used
	TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20);
	// CS2 = 0b111 (prescale 1024)
	// OCR2A = 50; // duty cycle register for channel A, not used
	TEMP_CONTROLLER_PWM_REGISTER = TEMP_CONTROLLER_NEUTRAL_PWM; // duty cycle register for channel B, can be between 0 and 255 (inclusive)

	// Load gains for temperature controller
	EEPROM.get(TEMP_CONTROLLER_P_ADDR, tempControllerP);
	EEPROM.get(TEMP_CONTROLLER_I_ADDR, tempControllerI);
	EEPROM.get(TEMP_CONTROLLER_D_ADDR, tempControllerD);
	EEPROM.get(TEMP_CONTROLLER_TIME_INTERVAL_ADDR, tempControllerTimeInterval);
	EEPROM.get(TEMP_CONTROLLER_RESPONSE_SCALER_ADDR, tempControllerResponseScaler);

	lcd.init(); // initialize the lcd
	lcd.backlight();
	lcd.setCursor(0,0);

	encoder = new ClickEncoder(ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_PIN_BTN, ENCODER_STEPS_PER_NOTCH);

	Timer1.initialize(500);
	Timer1.attachInterrupt(timerIsr);

	dht.begin(); // initialize DHT sensor
	lcd.clear();

	zeroTempErrHistory();
}

void loop()
{
	float humidity = dht.readHumidity();
	float temp = dht.readTemperature();
	// TODO: handle isnan(h) and isnan(t) errors

	controlFan(humidity, temp); // control fan regardless of current mode

	if (mode == "SET") {
		// PID parameter setting mode
		updateParams();
	} else {
		// Normal thermostat mode
		updateThermostat(humidity, temp);
	}
}

void zeroTempErrHistory() {
	for (int i = 0; i < TEMP_ERR_HISTORY_LENGTH; i++) {
		tempErrHistory[i] = 0;
	}
}

int sumTempErrHistory() {
	int totalErr = 0;
	for (int i = 0; i < TEMP_ERR_HISTORY_LENGTH; i++) {
		totalErr += tempErrHistory[i];
	}
	return totalErr;
}

void setTempErr(tempErr) {
	tempErrHistoryInd++;
	if (tempErrHistoryInd >= TEMP_ERR_HISTORY_LENGTH)
		tempErrHistoryInd = 0; // loop around
	tempErrHistory[tempErrHistoryInd] = tempErr;
}

int getTempErr(tempErr) {
	return tempErrHistory[tempErrHistoryInd];
}

int getDTempErr(tempErr) {
	oldTempErrHistoryInd = tempErrHistoryInd - 1;
	if (oldTempErrHistoryInd < 0)
		oldTempErrHistoryInd = TEMP_ERR_HISTORY_LENGTH - 1;
	return tempErrHistory[tempErrHistoryInd] - tempErrHistory[oldTempErrHistoryInd];
}

bool isActive() {
	for (int i = 0; i < TEMP_ERR_HISTORY_LENGTH; i++) {
		if (tempErrHistory[i] > 0) return true; // box has been hot, probably active
	}
	return false; // box has not been hot in history, probably inactive
}

void controlFan(float humidity, float temp) {
	if (mode == "OFF" || !isActive()) {
		digitalWrite(MOSFET_PIN, LOW); // turn off fan
		zeroTempErrHistory();
	} else {
		// mode is ON or SET and fan is needed
		if (millis() - lastErrMillis >= tempControllerTimeInterval * 1000) {
			// update integral and derivative error every time interval
			setTempErr(temp - setTemp);
			lastErrMillis = millis();
			float tempErrVal = getTempErr() * tempControllerP + sumTempErrHistory() * tempControllerI + getDTempErr() * tempControllerD;
			float pwmVal = TEMP_CONTROLLER_NEUTRAL_PWM + (tempErrVal * tempControllerResponseScaler);
			if (pwmVal > 255) pwmVal = 255;
			if (pwmVal < 0) pwmVal = 0;
			analogWrite(MOSFET_PIN, (byte)pwmVal);
			Serial.println(pwmVal);
		}
	}
		
}

void updateThermostat(float humidity, float temp) {
	// Draws LCD
	lcd.setCursor(0,0);
	char topLine[16];

	sprintf(topLine, "%.2d%cC   Set: %.2d%cC", (int)temp, (char)0xDF, setTemp, (char)0xDF); // adds a degrees char
	lcd.print(topLine);

	lcd.setCursor(0,1); // col 0, row 1
	char bottomLine[16];

	sprintf(bottomLine, "%.2d%c    Mode: %.3s", (int)humidity, (char)37, mode); // adds a % char
	lcd.print(bottomLine);

	// Reads encoder during normal operation (ie. not when parameters are being set)
	setTemp += (int)encoder->getValue(); // update set temp from encoder scroll
	if (setTemp < 0) setTemp = 0;
	else if (setTemp > 99) setTemp = 99;

	
	ClickEncoder::Button b = encoder->getButton();
	if (b == ClickEncoder::Clicked) {
		// update controller mode from encoder click
		if (mode == " ON") mode = "OFF";
		else mode = " ON";
	} else if (b == ClickEncoder::DoubleClicked) {
		// enter PID parameter setting mode
		mode = "SET";
		selectedParam = 0;
		modifiedParam = true;
		lcd.clear();
	}
}

void updateParams() {
	if (modifiedParam) {
		// Print PID values
		lcd.setCursor(0,0);
		char topLine[16];
		char pStr[4];
		dtostrf(tempControllerP, 3, 1, pStr); // 3 is min width, 1 is precision, float val copied into pStr
		char iStr[4];
		dtostrf(tempControllerI, 3, 1, iStr);
		char dStr[4];
		dtostrf(tempControllerD, 3, 1, dStr);
		sprintf(topLine, "P%s I%s D%s        ", pStr, iStr, dStr);
		lcd.print(topLine);

		// Print response scaler and time interval
		lcd.setCursor(0,1);
		char bottomLine[16];
		char scalerStr[6];
		dtostrf(tempControllerResponseScaler, 5, 3, scalerStr);
		sprintf(bottomLine, "SCL%s INT%d        ", scalerStr, tempControllerTimeInterval);
		lcd.print(bottomLine);

		modifiedParam = false; // reset draw params flag
	}

	// Modify parameter interface
	if (selectedParam > 0) {
		lcd.setCursor(paramCursorXPos[selectedParam-1], paramCursorYPos[selectedParam-1]);
		lcd.blink();

		int scroll = (int)encoder->getValue();
		if (scroll != 0) modifiedParam = true;
		
		if (selectedParam == 1) {
			tempControllerP += scroll * TEMP_CONTROLLER_PID_INCREMENT;
			if (tempControllerP < 0) tempControllerP = 0;
			EEPROM.put(TEMP_CONTROLLER_P_ADDR, tempControllerP);
		} else if (selectedParam == 2) {
			tempControllerI += scroll * TEMP_CONTROLLER_PID_INCREMENT;
			if (tempControllerI < 0) tempControllerI = 0;
			EEPROM.put(TEMP_CONTROLLER_I_ADDR, tempControllerI);
		} else if (selectedParam == 3) {
			tempControllerD += scroll * TEMP_CONTROLLER_PID_INCREMENT;
			if (tempControllerD < 0) tempControllerD = 0;
			EEPROM.put(TEMP_CONTROLLER_D_ADDR, tempControllerD);
		} else if (selectedParam == 4) {
			tempControllerResponseScaler *= pow(2, scroll);
			EEPROM.put(TEMP_CONTROLLER_RESPONSE_SCALER_ADDR, tempControllerResponseScaler);
		} else if (selectedParam == 5) {
			tempControllerTimeInterval += scroll * TEMP_CONTROLLER_TIME_INTERVAL_INCREMENT;
			if (tempControllerTimeInterval <= 0) tempControllerTimeInterval = 0;
			EEPROM.put(TEMP_CONTROLLER_TIME_INTERVAL_ADDR, tempControllerTimeInterval);
		} else modifiedParam = false;
	}

	ClickEncoder::Button b = encoder->getButton();
	if (b == ClickEncoder::Clicked) {
		// Change selected parameter
		selectedParam += 1;
		if (selectedParam > NUM_PARAMS) {
			selectedParam = 0;
			lcd.noBlink();
		}
	} else if (b == ClickEncoder::DoubleClicked) {
		mode = " ON";
		lcd.clear();
	}
}