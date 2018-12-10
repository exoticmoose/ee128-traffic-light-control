/*
Traffic Light Control System - Master Board
Paul Schneider, pschn002@ucr.edu
9 December 2018


Developed for EE128 @ UCR - Fall 2018 - Dr. Hyoseung Kim

Master element of a traffic control system, to be used in conjunction with
four light controller (slave) boards over I2C/Wire. System capable of 
response to 8 (2 at each corner) pedestrian crosswalks and IR sensor input
to control traffic light state.

*/


#include <Arduino.h>
#include <Wire.h>

#define WAITING 0x01
#define STANDARD_UPDATE 0x02
#define URGENT_UPDATE 0x04
#define FAULT 0x08

#define EMERGENCY 0x01
#define NORTH_SOUTH 0x02
#define EAST_WEST 0x04

#define TICK_PERIOD 50



uint8_t slave_id[4] = {8, 16, 32, 64};

enum States {BEGIN, INIT, START, TRANSMIT, ERROR_CHECK, PROCESS_OUTPUTS, WAIT, FAULT_RECOVER};
enum TrafficPatterns {NS_RED, NS_GREEN, NS_YELLOW, EW_RED, EW_GREEN, EW_YELLOW};
unsigned int traffic_pattern_times[6] = {500, 15000, 2000, 500, 25000, 2000};
unsigned short traffic_pattern_lights[6] = {0b0000100100100100,		// red ns
											0b0000100001100001,		// green ns
											0b0000100010100010, 	// yellow ns
											0b0000100100100100,		// red ew
											0b0000001100001100,		// green ew
											0b0000010100010100 };	// yellow ew

unsigned long sys_time = 0; 		// absolute system time updated in loop()
unsigned long sys_tick = 0; 		// number of ticks of SM total 
unsigned long sys_state_time = 0; 	// system time at start of current state, updated at the end of sm_tick()
unsigned long state_time = 0; 		// time elapsed in current state
unsigned long light_time = 0;

States sys_state = BEGIN;
TrafficPatterns current_traffic_pattern = NS_RED;

unsigned char fault = 0x00;					// global fault flag
unsigned int fault_count = 0;
unsigned char requests_crosswalk = 0x00;
unsigned char requests_emergency = 0x00;

struct light_interface {
	unsigned char slave_id = 0x00;
	unsigned char bulb_pattern = 0x00;
	unsigned char status_reg = 0x00; // status register received FROM SLAVES
};

struct light_interface light_controllers[4];

struct master_light_pattern {
	unsigned char bulbs[4] = {0};
	unsigned long time_in_pattern = 10000; // could be 0 default

	unsigned char status_reg = 0x00 | WAITING; // status register for MASTER INTERNAL USE
  
} default_master_light_pattern;

struct master_light_pattern system_pattern;

void txLightsFrame(master_light_pattern new_pattern) {

	light_time = sys_time;

	for (int i = 0; i < 4; i++) {
		light_controllers[i].bulb_pattern = system_pattern.bulbs[i];
	}
		
	for (unsigned char i = 0; i < 4; i++) {
		Wire.write(light_controllers[i].bulb_pattern);
		Wire.write(system_pattern.time_in_pattern);
		Wire.write(system_pattern.time_in_pattern >> 8);
		Wire.write(system_pattern.time_in_pattern >> 16);
		Wire.write(system_pattern.time_in_pattern >> 24);
		Wire.endTransmission();
	}
}

unsigned char pollPatternUpdate() {

	if (fault) {
		system_pattern.status_reg = FAULT;
	}
	else if (state_time >= system_pattern.time_in_pattern - TICK_PERIOD) {
		system_pattern.status_reg = 0x00 | STANDARD_UPDATE;
	} 
	else system_pattern.status_reg = 0x00 | WAITING;

	return system_pattern.status_reg;
}

void processOutputState() {

	unsigned long light_on_time = millis() - light_time;

	TrafficPatterns next_traffic_pattern = NS_RED;
	int next_light_time = 0;
	
	unsigned short new_pattern = 0x00;
	bool continue_cycle = true;

	if (requests_emergency) {

		unsigned char re = requests_emergency;

		if (!((re & (re - 1)) == 0)) { // if not a power of 2 / more than 1 bit set
			if (current_traffic_pattern == NS_RED) {
				next_traffic_pattern = NS_RED;
				next_light_time = 15 * 1000;
				continue_cycle = false;
			}
			else if (current_traffic_pattern == EW_RED) {
				next_traffic_pattern = EW_RED;
				next_light_time = 15 * 1000;
				continue_cycle = false;
			}
			else if (current_traffic_pattern == NS_GREEN) {
				next_traffic_pattern = NS_YELLOW;
				next_light_time = 1 * 1000;
				continue_cycle = false;
			}
			else if (current_traffic_pattern == EW_GREEN) {
				next_traffic_pattern = EW_YELLOW;
				next_light_time = 1 * 1000;
				continue_cycle = false;
			}
		}
		else { // only 1 light
			unsigned char idx = 0;
			for (int i = 0; i < 4; i++) {
				if ((re >> i) & 0x01) idx = i; // bit shift to find which slave index
			}
			if (idx == 1 || idx == 3) {
				// If slave is NS
				if (current_traffic_pattern == EW_GREEN) {
					next_traffic_pattern = EW_YELLOW;
					next_light_time = 2 * 1000;
					continue_cycle = false;
				}
				else if (current_traffic_pattern == NS_RED) {
					next_traffic_pattern = NS_RED;
					next_light_time = 15 * 1000;
					continue_cycle = false;
				}
			} 
			else if (idx == 0 || idx == 2) {
				// If slave is EW
				if (current_traffic_pattern == NS_GREEN) {
					next_traffic_pattern = NS_YELLOW;
					next_light_time = 2 * 1000;
					continue_cycle = false;
				}
				else if (current_traffic_pattern == EW_RED) {
					next_traffic_pattern = EW_RED;
					next_light_time = 15 * 1000;
					continue_cycle = false;
				}


			}
		}
	}                                                          
	else if (requests_crosswalk) {
		
		if ((requests_crosswalk & 0x0F) && (current_traffic_pattern == EW_GREEN) && (light_on_time > 4 * 1000))  {
			// NS Crosswalk priority over E/W
			// if there's currently a long wait to cross,
			// leave it green for a little bit before transitioning
			next_traffic_pattern = EW_GREEN;
			next_light_time = 1 * 1000;
			continue_cycle = false;
		}
		else if ((requests_crosswalk & 0xF0) && (current_traffic_pattern == NS_GREEN) && (light_on_time > 4 * 1000))  {
			// NS Crosswalk priority over E/W
			// if there's currently a long wait to cross,
			// leave it green for a little bit before transitioning
			next_traffic_pattern = NS_GREEN;
			next_light_time = 1 * 1000;
			continue_cycle = false;
		}

	}

	if (continue_cycle) {
		next_traffic_pattern = current_traffic_pattern + (TrafficPatterns)1;
		if (next_traffic_pattern > 5) next_traffic_pattern = NS_RED;
		next_light_time = traffic_pattern_times[next_traffic_pattern];
	}

	new_pattern = traffic_pattern_lights[next_traffic_pattern];

	for (int i = 0; i < 4; i++) {
		system_pattern.bulbs[i] = (new_pattern >> (3 * i)) & 0x07; 
	}
	system_pattern.time_in_pattern = next_light_time;

	if (next_traffic_pattern == NS_GREEN) requests_crosswalk = requests_crosswalk & 0xF0;
	if (next_traffic_pattern == EW_GREEN) requests_crosswalk = requests_crosswalk & 0x0F;
	if ((requests_emergency) && ((current_traffic_pattern == NS_RED) || (current_traffic_pattern == EW_RED))) {
		requests_emergency = 0x00;
	}

	current_traffic_pattern = next_traffic_pattern;
}

void pollSlaves() {
	// send requests to each slave to obtain new traffic pattern update requests
	for (int i = 0; i < 4; i++) {

		Wire.requestFrom((int)slave_id[i], 1); // get status register of slave
		while (Wire.available()) {
				char c = Wire.read();
				if (c) {
					if (c & EMERGENCY) {
						requests_emergency |= 1 << i;
					}
					if (c & NORTH_SOUTH) {
						requests_crosswalk |= 1 << (i + 0);
					}
					if (c & EAST_WEST) {
						requests_crosswalk |= 1 << (i + 4);
					}
				}// else Serial.println("Zero response from slave");
			}

	}

	unsigned long light_on_time = millis() - light_time;

	if ((requests_crosswalk & 0x0F) && (current_traffic_pattern == EW_GREEN) && (light_on_time > 4 * 1000))  {
		system_pattern.status_reg |= STANDARD_UPDATE;
	}
	else if ((requests_crosswalk & 0xF0) && (current_traffic_pattern == NS_GREEN) && (light_on_time > 4 * 1000))  {
		system_pattern.status_reg |= STANDARD_UPDATE;
	}

	unsigned char re  = requests_emergency;
	if (re) {
		if (!((re & (re - 1)) == 0)) { // if not a power of 2 / more than 1 bit set
			if (current_traffic_pattern == NS_RED) {
				system_pattern.status_reg |= URGENT_UPDATE;
			}	
			else if (current_traffic_pattern == EW_RED) {
				system_pattern.status_reg |= URGENT_UPDATE;
			}
			else if (current_traffic_pattern == NS_GREEN) {
				system_pattern.status_reg |= URGENT_UPDATE;
			}
			else if (current_traffic_pattern == EW_GREEN) {
				system_pattern.status_reg |= URGENT_UPDATE;
			}
		}
		else { // only 1 light
			unsigned char idx = 0;
			for (int i = 0; i < 4; i++) {
				if ((re >> i) & 0x01) idx = i; // bit shift to find which slave index
			}
			Serial.println(idx);

			if (idx == 1 || idx == 3) {
				// If slave is NS
				if (current_traffic_pattern == EW_GREEN) {
					system_pattern.status_reg |= URGENT_UPDATE;
				}
				else if (current_traffic_pattern == NS_RED) {
					system_pattern.status_reg |= URGENT_UPDATE;
				}
			} 
			else if (idx == 0 || idx == 2) {
				// If slave is EW
				if (current_traffic_pattern == NS_GREEN) {
					system_pattern.status_reg |= URGENT_UPDATE;
				}
				else if (current_traffic_pattern == EW_RED) {
					system_pattern.status_reg |= URGENT_UPDATE;
				}
			}
		}
	}
}

void sm_tick(States state) {

	sys_tick++;
	States old = state;

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
			else if (system_pattern.status_reg >> 1) { // if casual or urgent updates
				state = ERROR_CHECK;
			}
			else if (state_time > system_pattern.time_in_pattern + 2048) state = FAULT_RECOVER; // timeout precaution, 2048ms error window
			else state = WAIT; // loop until fault, timeout, or update request
			break;

		case PROCESS_OUTPUTS:
			if (fault) state = FAULT_RECOVER; // this state basically only used to call the function
			else state = TRANSMIT;
			break;

		case ERROR_CHECK:

			if (fault) state = FAULT_RECOVER;
			else state = PROCESS_OUTPUTS;
			break;

		case FAULT_RECOVER:
			if (fault_count > 10000) state = INIT;
			else if (fault) state = FAULT_RECOVER;
			else state = START;
			break;

		case BEGIN:
			state = INIT;
		// ----------------------------
		default:
			//dont get here please
			state = INIT;
			break;

	}
	switch (state) {
		// state actions
		case INIT:
			// initialize all global variables and "clear" things up
			for (int i = 0; i < 4; i++) {
				system_pattern.bulbs[i] = (traffic_pattern_lights[0] >> (3 * i)) & 0x07; 
			}
			system_pattern.time_in_pattern = 4 * 1000; // 4s
			system_pattern.status_reg = 0x00 | WAITING;
			break;

		case START:
			// go through first transition
			break;
		case TRANSMIT:
			// no processing, purely send data
			txLightsFrame(system_pattern);
			break;
		case WAIT:
			// check if there's any updating to do
			system_pattern.status_reg =  pollPatternUpdate();
			pollSlaves();
			break;
		case PROCESS_OUTPUTS:
			// calculate next state outputs
			processOutputState();
			break;
		case ERROR_CHECK:
			if (system_pattern.status_reg & 0x80) {fault = 0x01;}
			break;
		case FAULT_RECOVER:
			fault_count++;
			break;
		case BEGIN:
			// not entered
			break;

		default:
			// again, please no
			break;
	}

	if (state != old) sys_state_time = sys_time; // new state entered, update this time
	sys_state = state;
}


void setup() {
	// put your setup code here, to run once:
	Wire.begin(); // no ID for master
	Serial.begin(38400); // for debugging

	system_pattern.status_reg = 0x00 | WAITING ;
	system_pattern.time_in_pattern = 12345;

	light_controllers[0].slave_id = 8;
	light_controllers[1].slave_id = 16;
	light_controllers[2].slave_id = 32;
	light_controllers[3].slave_id = 64;

}

void loop() {

	sys_time = millis();
	state_time = sys_time - sys_state_time;	
	sm_tick(sys_state);

	delay(TICK_PERIOD);
}