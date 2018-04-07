#include <EEPROM.h> // Arduino built-in library for saving to EEPROM

// P, I, D gains are each floats (4 bytes)
#define TEMP_CONTROLLER_P_ADDR 0
#define TEMP_CONTROLLER_I_ADDR 4
#define TEMP_CONTROLLER_D_ADDR 8
#define TEMP_CONTROLLER_TIME_INTERVAL_ADDR 12
#define TEMP_CONTROLLER_RESPONSE_SCALER_ADDR 14

void setup() {
	// Clear EEPROM
	for (int addr=0; addr < 255; addr++) {
		byte defaultVal = 255;
		EEPROM.write(addr, defaultVal);
	}

	float tempControllerP = 1.0f;
	float tempControllerI = 3.0f;
	float tempControllerD = 0.2f;
	int tempControllerTimeInterval = 8000;
	float tempControllerResponseScaler = 0.1;

	EEPROM.put(TEMP_CONTROLLER_P_ADDR, tempControllerP);
	EEPROM.put(TEMP_CONTROLLER_I_ADDR, tempControllerI);
	EEPROM.put(TEMP_CONTROLLER_D_ADDR, tempControllerD);
	EEPROM.put(TEMP_CONTROLLER_TIME_INTERVAL_ADDR, tempControllerTimeInterval);
	EEPROM.put(TEMP_CONTROLLER_RESPONSE_SCALER_ADDR, tempControllerResponseScaler);

	Serial.begin(9600);
	float check_p = 0.00f;
	float check_i = 0.00f;
	float check_d = 0.00f;
	int check_tint = 0;
	float check_rscaler = 0;
	EEPROM.get(TEMP_CONTROLLER_P_ADDR, check_p);
	EEPROM.get(TEMP_CONTROLLER_I_ADDR, check_i);
	EEPROM.get(TEMP_CONTROLLER_D_ADDR, check_d);
	EEPROM.get(TEMP_CONTROLLER_TIME_INTERVAL_ADDR, check_tint);
	EEPROM.get(TEMP_CONTROLLER_RESPONSE_SCALER_ADDR, check_rscaler);
	Serial.println(check_p);
	Serial.println(check_i);
	Serial.println(check_d);
	Serial.println(check_tint);
	Serial.println(check_rscaler);

	// Serial.println("Printing more of EEPROM");
	// for (int addr=0; addr < 50; addr++) {
	// 	byte blob = EEPROM.read(addr);
	// 	Serial.println(blob);
	// }
}

void loop() {

}