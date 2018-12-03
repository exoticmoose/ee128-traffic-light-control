#include <Arduino.h>
#include <Wire.h>

#define WAITING 0x01
#define CASUAL_UPDATE 0x02
#define URGENT_UPDATE 0x04
#define FAULT 0x08

#define EMERGENCY 0x01
#define NORTH_SOUTH 0x02
#define EAST_WEST 0x04
#define ERROR_MODE 0x80

#define MY_ADDRESS 16


int last_comms_delay = 0;
unsigned char displayed_bulbs = 0x00;
unsigned char status_reg = 0x00;
unsigned int displayed_time_in_pattern = 0;
unsigned int time_in_pattern = 0;
unsigned char pattern_count = 0;

const byte ButtonPin = 2;
const byte ButtonPin2 = 3;
int val = 0;
char tmp_flag_emergency = 0;
char Last_Sent = 0;
char* Message = 0;


void receiveEvent(int var) {
	last_comms_delay = 0;

	unsigned char rx_slave_id = Wire.read();
	unsigned char bulbs = Wire.read();
	unsigned int tmp_time_in_pattern = 0;
	
	for (int i = 0; i < 4; i++) {
		unsigned char c = Wire.read();
		tmp_time_in_pattern |= (c << (8 * i));
	}
	
	Serial.println("- - - - - - - - - -");
	//Serial.println(Wire.available());
	Serial.println(millis());
	Serial.print("\tSlave ID: ");
	Serial.println(rx_slave_id);
	Serial.print("\tBulb Output: ");
	Serial.println(bulbs, HEX);
	Serial.print("\tStatus: ");
	Serial.println(status_reg, HEX);
	Serial.print("\tTIP: ");
	Serial.println(tmp_time_in_pattern);
	Serial.println("- - - - - - - - - -");

	time_in_pattern = tmp_time_in_pattern;
}

void requestEvent() {
 
    Wire.write(status_reg);
    status_reg = status_reg & 0x00;
	last_comms_delay = 0;
}


void emergencyMode() {

	Serial.println("entering emergency mode");
	status_reg = status_reg | ERROR_MODE;
	pattern_count++;
	if (pattern_count < 5) {
		digitalWrite(9, HIGH);
		digitalWrite(8, LOW);
		digitalWrite(7, LOW);
	} else if (pattern_count > 5) {
		digitalWrite(9, LOW);
		digitalWrite(8, LOW);
		digitalWrite(7, LOW);
		if (pattern_count > 10) pattern_count = 0;
	}
}

void Button_Push()
{
	status_reg = status_reg | NORTH_SOUTH;
    Serial.println("0x03");  
}

void Button_Push2()
{
	status_reg = status_reg | EAST_WEST;
    Serial.println("0x02");  
}

void IR_Sensor()
{
  val = analogRead(0);

  if(val < 970 && tmp_flag_emergency == 0)
  {
     status_reg = status_reg | EMERGENCY;
     Serial.println("0x01");
     tmp_flag_emergency = 1;
  }
  else if(tmp_flag_emergency == 1 and val > 970)
  {
    tmp_flag_emergency = 0;
  }
  
}


void setup() {
	Serial.begin(38400);
	Wire.begin(MY_ADDRESS);
	Wire.onReceive(*receiveEvent);
 	Wire.onRequest(requestEvent);
	pinMode(ButtonPin, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(ButtonPin), Button_Push, RISING);
	pinMode(ButtonPin2, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(ButtonPin2), Button_Push2, RISING);
	Serial.println("startup");
	
	DDRB = 0xFF;
  	PORTB = 0;


}

void loop() {
	// put your main code here, to run repeatedly:
	delay(100);
	IR_Sensor();
	last_comms_delay++;

	if (last_comms_delay > 20) {
		emergencyMode();
	}
}