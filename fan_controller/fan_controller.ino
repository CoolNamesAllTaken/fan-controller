#include <Wire.h> // I2C communication library
#include <LiquidCrystal_I2C.h> // LCD w/ I2C backpack library
#include <ClickEncoder.h> // rotary encoder with the clicky bit library
#include <TimerOne.h> // library for timer interrupts
#include <DHT.h> // Adafruit DHT sensor library

#define DHT_PIN 4 // AM2302 Sensor on pin D3
#define DHT_TYPE DHT22 // AM2302

#define ENCODER_PIN_A A1
#define ENCODER_PIN_B A0
#define ENCODER_PIN_BTN A2
#define ENCODER_STEPS_PER_NOTCH 1

LiquidCrystal_I2C lcd(0x27,16,2); // set the LCD address to 0x27 for a 16 chars and 2 line display
DHT dht(DHT_PIN, DHT_TYPE); // set DHT sensor
ClickEncoder* encoder;

int setTemp = 30; // set temperature [deg. C]
char* mode = " ON";

void timerIsr() {
	encoder->service();
}

void setup()
{
	lcd.init(); // initialize the lcd
	lcd.backlight();
	lcd.setCursor(0,0);

	encoder = new ClickEncoder(ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_PIN_BTN, ENCODER_STEPS_PER_NOTCH);

	Timer1.initialize(500);
	Timer1.attachInterrupt(timerIsr);

	dht.begin(); // initialize DHT sensor
	lcd.clear();
}

void loop()
{
	float humidity = dht.readHumidity();
	float temp = dht.readTemperature();
	// TODO: handle isnan(h) and isnan(t) errors

	readEncoder();
	drawLCD(humidity, temp);
}

void readEncoder() {
	setTemp += (int)encoder->getValue(); // update set temp from encoder scroll
	if (setTemp < 0) setTemp = 0;
	else if (setTemp > 99) setTemp = 99;

	// update controller mode from encoder click
	ClickEncoder::Button b = encoder->getButton();
	if (b == ClickEncoder::Clicked) {
		if (mode == " ON") mode = "OFF";
		else mode = " ON";
	}
}

void drawLCD(float humidity, float temp) {
	lcd.setCursor(0,0);
	char topLine[16];

	sprintf(topLine, "%.2d%cC   Set: %.2d%cC", (int)temp, (char)0xDF, setTemp, (char)0xDF); // adds a degrees char
	lcd.print(topLine);

	lcd.setCursor(0,1); // col 0, row 1
	char bottomLine[16];

	sprintf(bottomLine, "%.2d%c    Mode: %.3s", (int)humidity, (char)37, mode); // adds a % char
	lcd.print(bottomLine);
}