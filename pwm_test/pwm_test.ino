#include <ClickEncoder.h> // rotary encoder with the clicky bit library
#include <TimerOne.h> // library for timer interrupts

#define ENCODER_PIN_A A1
#define ENCODER_PIN_B A0
#define ENCODER_PIN_BTN A2
#define ENCODER_STEPS_PER_NOTCH 1

#define MOSFET_PIN 3 // output pin connected to gate of N-channel MOSFET

ClickEncoder* encoder;

void timerIsr() {
	encoder->service();
}

void setup() {
	// Set PWM fan control on timer 2, pin 3 (OC2B) to not mess with timer 1
	pinMode(MOSFET_PIN, OUTPUT);
	TCCR2A = 0; // clear first settings register for timer 2
	TCCR2B = 0; // clear second settings register for timer 2
	TCCR2A = _BV(WGM21) | _BV(WGM20) | _BV(COM2B1); // | _BV(COM2A1)
	// WGM2 = 0b111 (timer 2 in fast pwm mode with OCR2A as TOP)
	// COM2B = 0b10 (timer 2 channel b in non-inverting mode)
	// COM2A = 0b10 (timer 2 channel a in non-inverting mode), not used
	TCCR2B = _BV(WGM22) | _BV(CS21);
	// CS2 = 0b010 (prescale 1024)
	OCR2A = 255; // duty cycle register for channel A, not used
	OCR2B = 0; // duty cycle register for channel B, can be between 0 and 255 (inclusive)

	Serial.begin(9600);

	encoder = new ClickEncoder(ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_PIN_BTN, ENCODER_STEPS_PER_NOTCH);

	Timer1.initialize(500);
	Timer1.attachInterrupt(timerIsr);
}

void loop() {
	Serial.println(OCR2B);
	int scroll = (int)encoder->getValue();

	int pwmVal = (int)OCR2B + scroll;
	if (pwmVal < 0) pwmVal = 0;
	else if (pwmVal > 255) pwmVal = 255;
	analogWrite(MOSFET_PIN, pwmVal);
	Serial.println(pwmVal);

}

