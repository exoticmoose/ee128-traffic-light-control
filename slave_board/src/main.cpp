#include <Arduino.h>
#include <Wire.h>

#define URGENT_UPDATE 0x01
#define CASUAL_UPDATE 0x02
#define WAITING 0x04
#define FAULT 0x08

#define MY_ADDRESS 8

int last_instruction_delay = 0;


unsigned char displayed_bulbs = 0x00;
unsigned char status_reg = 0x00;
unsigned int displayed_time_in_pattern = 0;


void receiveEvent(int var) {
	last_instruction_delay = 0;

	unsigned char rx_slave_id = Wire.read();
	unsigned char bulbs = Wire.read();
	unsigned char tmp_status_reg = Wire.read();
	unsigned int tmp_time_in_pattern = 0;
	for (int i = 0; i < 4; i++) {
		unsigned char c = Wire.read();
		tmp_time_in_pattern |= (c << (8 * i));
	}
	

	Serial.print("wire available: ");
	Serial.println(Wire.available());
	Serial.println(rx_slave_id);
	Serial.println(bulbs);
	Serial.println(tmp_status_reg);

	Serial.println(tmp_time_in_pattern);
	Serial.println("- - - - - - - - - -");

}

void emergencyMode() {

	Serial.println("entering emergency mode");
}

void setup() {
	Serial.begin(38400);
	Wire.begin(MY_ADDRESS);
	Wire.onReceive(*receiveEvent);
}

void loop() {
	// put your main code here, to run repeatedly:
	delay(100);
	last_instruction_delay++;

	 // TODO
 	 // if more than 500ms since last instruction, enter blinking red light loop
	if (last_instruction_delay > 15) {
		emergencyMode();
	}

}