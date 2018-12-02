#include <Arduino.h>
#include <Wire.h>

#define WAITING 0x01
#define CASUAL_UPDATE 0x02
#define URGENT_UPDATE 0x04
#define FAULT 0x08



uint8_t slave_id[4] = {8, 16, 32, 64};

enum States {BEGIN, INIT, START, TRANSMIT, ERROR_CHECK, PROCESS_OUTPUTS, WAIT, FAULT_RECOVER};

unsigned long sys_time = 0; // absolute system time updated in loop()
unsigned long sys_tick = 0; // number of ticks of SM total 
unsigned long sys_state_time = 0; // system time at start of current state, updated at the end of sm_tick()
unsigned long state_time = 0; // time elapsed in current state
States sys_state = INIT;

unsigned char fault = 0x00;

struct light_interface {
	unsigned char slave_id = 0x00;
	unsigned char bulb_pattern = 0x00;
	unsigned char status_reg = 0x00 | WAITING;
};

struct light_interface light_controllers[4];

struct master_light_pattern {
	unsigned char bulbs[4] = {0};
	unsigned char status_reg = 0x00 | WAITING;
	unsigned long time_in_pattern = 10000; // could be 0 default
  
} default_master_light_pattern;

struct master_light_pattern testing_pattern;

unsigned short sample_pattern[6] = {	0b0000001100001100, // Green E/W, Red S/N
									0b0000010100010100, // Yellow E/w, Red S/N
									0b0000100100100100, // Red E/W, Red N/S
									0b0000100001100001, // Red E/W, Green N/S
									0b0000100010100010, // Red E/W, Yellow N/S
									0b0000100100100100}; // Red E/W, Red N/S


void txLightsFrame(master_light_pattern new_pattern) {
	Serial.println("transmitting light frame...");
	
	for (int i = 0; i < 4; i++) {
		light_controllers[i].bulb_pattern = testing_pattern.bulbs[i];
		light_controllers[i].status_reg = testing_pattern.status_reg;


	}

	// TODO remove comments once all 4 boards enabled
	//for (unsigned char i = 0; i < 4; i++) {
		Wire.beginTransmission(slave_id[0]);
		Wire.write(slave_id[0]);
		Wire.write(new_pattern.bulbs[0]);
		Wire.write(new_pattern.status_reg);
		Wire.write(new_pattern.time_in_pattern);
		Wire.write(new_pattern.time_in_pattern >> 8);
		Wire.write(new_pattern.time_in_pattern >> 16);
		Wire.write(new_pattern.time_in_pattern >> 24);
		Wire.endTransmission();
	//}

	Serial.println("TX complete.");
	
}

void sm_tick(States state) {
	// generic state machine tick
	sys_tick++;
	States old = state;
	Serial.println("- - - sm_tick");


	Serial.print("- - - - state: ");
	Serial.println(state);
	switch (state) {
		// state changes
		case INIT:
			state = START;
			break;

		case START:
			if (!fault) state = TRANSMIT;
			else state = ERROR_CHECK;
			break;

		case TRANSMIT:
			if (fault) state = FAULT_RECOVER;
			else state = WAIT;
			break;

		case WAIT:
			if (fault) state = FAULT_RECOVER;
			else if (testing_pattern.status_reg >> 1) { // if casual or urgent updates, 
				state = ERROR_CHECK;
			}
			else if (state_time > testing_pattern.time_in_pattern + 2048) state = FAULT_RECOVER; // timeout precaution, 2048ms error window
			else state = WAIT; // loop until fault, timeout, or update request
			break;

		case PROCESS_OUTPUTS:
			if (fault) state = FAULT_RECOVER; // this state basically only used to call the function
			else state = TRANSMIT;
			break;

		case ERROR_CHECK:
			if (fault) state = FAULT_RECOVER;
			else state = START;
			break;

		case FAULT_RECOVER:
			// ??? Big ol' TODO here
			state = START;
			break;

		case BEGIN:
			state = INIT;
		// ----------------------------
		default:
			//dont get here please
			state = INIT;
			break;

	}

	Serial.print("- - - - state: ");
	Serial.println(state);
	switch (state) {
		// state actions

		
		case INIT:
			// initialize all global variables and "clear" things up
			for (int i = 0; i < 4; i++) {
				testing_pattern.bulbs[i] = (sample_pattern[0] >> (3 * i)) & 0x07; 
			}
			testing_pattern.time_in_pattern = 10 * 1000; // 10s
			testing_pattern.status_reg = 0x00 | WAITING;

		case START:
			// go through first transition
			break;
		case TRANSMIT:
			// no processing, purely send data
			break;
		case WAIT:
			// do nothing
			break;
		case PROCESS_OUTPUTS:
			// calculate next state outputs
			
			break;
		case ERROR_CHECK:

			break;
		case FAULT_RECOVER:

			break;
		case BEGIN:

			break;

		default:
			// again, please no
			testing_pattern.bulbs[0] = 0xCC;
			break;
	}

	if (state != old) sys_state_time = 0; // new state entered, update this time
	sys_state = state;
}


void setup() {
	// put your setup code here, to run once:
	Wire.begin(); // no ID for master
	Serial.begin(38400); // for debugging TODO

	testing_pattern.bulbs[0] = 0xAB;
	testing_pattern.status_reg = 0x00 | WAITING | URGENT_UPDATE;
	testing_pattern.time_in_pattern = 12345;

	light_controllers[0].slave_id = 8;
	light_controllers[1].slave_id = 16;
	light_controllers[2].slave_id = 32;
	light_controllers[3]].slave_id = 64;





}

void loop() {
	// put your main code here, to run repeatedly:
	txLightsFrame(testing_pattern);
	testing_pattern.bulbs[0]++;
	delay(1000);


	sys_time = millis();
	state_time = sys_time - sys_state_time;
	sm_tick(sys_state);
}