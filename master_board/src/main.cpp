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
unsigned int traffic_pattern_times[6] = {1000, 15000, 2000, 1000, 25000, 2000};
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

unsigned char fault = 0x00;							// global fault flag
unsigned char pattern_cycle_index = 0;	

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

struct master_light_pattern testing_pattern;

//unsigned short default_pattern_cycle[6] = { 0b0000001100001100,		// Green E/W, Red S/N
											// 0b0000010100010100,		// Yellow E/w, Red S/N
											// 0b0000100100100100,		// Red E/W, Red N/S
											// 0b0000100001100001,		// Red E/W, Green N/S
											// 0b0000100010100010,		// Red E/W, Yellow N/S
											// 0b0000100100100100 };	// Red E/W, Red N/S
//unsigned short default_pattern_delays[6] = {4000, 1000, 500, 4000, 1000, 500}; // how long to wait between transitions (ms)

//
void txLightsFrame(master_light_pattern new_pattern) {
	//Serial.println("transmitting light frame...");
	light_time = sys_time;

	for (int i = 0; i < 4; i++) {
		light_controllers[i].bulb_pattern = testing_pattern.bulbs[i];
	}
		
	//debug

	Serial.print(" TX Controller bulb patterns: ");
	Serial.print(light_controllers[0].bulb_pattern);
	Serial.print(" ");
	Serial.print(light_controllers[1].bulb_pattern);
	Serial.print(" ");
	Serial.print(light_controllers[2].bulb_pattern);
	Serial.print(" ");
	Serial.println(light_controllers[3].bulb_pattern);

	// \debug

	for (unsigned char i = 0; i < 4; i++) {

		/*
		Serial.println(" - - - - - - - Wire dump: ");
		Serial.println(light_controllers[i].slave_id);
		Serial.println(light_controllers[i].bulb_pattern);
		Serial.println(testing_pattern.time_in_pattern, HEX);
		Serial.println(testing_pattern.time_in_pattern >> 8, HEX);
		Serial.println(testing_pattern.time_in_pattern >> 16, HEX);
		Serial.println(testing_pattern.time_in_pattern >> 24, HEX);
		Serial.println("End Wire dump - - - - - - - -");
		*/



		Wire.beginTransmission(slave_id[i]);
		Wire.write(light_controllers[i].slave_id);
		Wire.write(light_controllers[i].bulb_pattern);
		Wire.write(testing_pattern.time_in_pattern);
		Wire.write(testing_pattern.time_in_pattern >> 8);
		Wire.write(testing_pattern.time_in_pattern >> 16);
		Wire.write(testing_pattern.time_in_pattern >> 24);
		Wire.endTransmission();
	}


	
	//Serial.println("TX complete.");
	
}

unsigned char pollPatternUpdate() {
	// check whether we should update the current traffic pattern
	// should be called during WAIT state
	// Check for urgent or casual state change calls
	// update master pattern's status register

	//debug
	//Serial.println(" - - - - - - - -  - POLL");
	//Serial.print(" - - - - - - - - - state_time: ");
	//Serial.println(state_time);
	//Serial.print(" - - -- - - - -  -time in pattern: ");
	//Serial.println(testing_pattern.time_in_pattern);
	if (fault) {
		testing_pattern.status_reg = FAULT;
	}
	else if (state_time >= testing_pattern.time_in_pattern - TICK_PERIOD) {

		testing_pattern.status_reg = 0x00 | STANDARD_UPDATE;

	} else testing_pattern.status_reg = 0x00 | WAITING;


	return testing_pattern.status_reg;


}

void processOutputState() {

	//
	// if (fault)
	// else if (emergency)
	// else if

	unsigned long light_on_time = millis() - light_time;

	Serial.println(" - - - - - - - - - - - - Processing Outputs");
	Serial.print(" - - - - - - - - - - - - current_traffic_pattern: ");
	Serial.println(current_traffic_pattern);
	Serial.print(" - - - - - - - - - - - - requests_crosswalk: ");
	Serial.println(requests_crosswalk, BIN);
	Serial.print(" - - - - - - - - - - - - light_on_time: ");
	Serial.println(light_on_time);



	TrafficPatterns next_traffic_pattern = NS_RED;
	int next_light_time = 0;
	
	unsigned short new_pattern = 0x00;
	bool continue_cycle = true;
	//bool no_change = false;

	/*
	if more than 1:
		everything go to yellow NOW
		(next time around, everything go to RED)

	if NOT more than 1:

		if (currently: any yellow, or any red):
			next state = red for 15 seconds
			done!


		
		else find which slave via bitshift
			if (slave ODD) { //
				// going against it, turn it yellow
				// going with current green light, do nothing
			}
			else if (slave EVEN) {
				// going against it, turn it yellow
				// going with current green light, do nothing
			}



	*/
	if (requests_emergency) {
		Serial.println("Found emergency requests");
		Serial.println(requests_emergency,BIN);
		Serial.println(current_traffic_pattern, BIN);
		unsigned char re = requests_emergency;

		if (!((re & (re - 1)) == 0)) { // if not a power of 2 / more than 1 bit set
			Serial.println("Multiple emergency requests");
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
			Serial.println("Single emergency requests");
			unsigned char idx = 0;
			for (int i = 0; i < 4; i++) {
				if ((re >> i) & 0x01) idx = i; // bit shift to find which slave index
			}
			Serial.println(idx);

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
		
		Serial.println(" - - - - - - - - - - - - Checking crosswalks....");
		if ((requests_crosswalk & 0x0F) && (current_traffic_pattern == EW_GREEN) && (light_on_time > 4 * 1000))  {
			// NS Crosswalk priority over E/W
			// if there's currently a long wait to cross,
			// leave it green for a little bit before transitioning
			next_traffic_pattern = EW_GREEN;
			next_light_time = 1 * 1000;
			continue_cycle = false;
			Serial.println("EW_GREEN for 1 more seconds requested. n.t.p. = 4");
		}
		else if ((requests_crosswalk & 0xF0) && (current_traffic_pattern == NS_GREEN) && (light_on_time > 4 * 1000))  {
			// NS Crosswalk priority over E/W
			// if there's currently a long wait to cross,
			// leave it green for a little bit before transitioning
			next_traffic_pattern = NS_GREEN;
			next_light_time = 1 * 1000;
			continue_cycle = false;
			Serial.println("NS_GREEN for 1 more seconds requested. n.t.p. = 1");
		}

	}



	if (continue_cycle) {
		next_traffic_pattern = current_traffic_pattern + (TrafficPatterns)1;
		if (next_traffic_pattern > 5) next_traffic_pattern = NS_RED;
		next_light_time = traffic_pattern_times[next_traffic_pattern];
	}

	Serial.print("next_traffic_pattern: ");
	Serial.println(next_traffic_pattern);
	new_pattern = traffic_pattern_lights[next_traffic_pattern];

	for (int i = 0; i < 4; i++) {
		testing_pattern.bulbs[i] = (new_pattern >> (3 * i)) & 0x07; 
	}
	testing_pattern.time_in_pattern = next_light_time;

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
					Serial.print("Got response: ");
					Serial.print(c, HEX);
					Serial.print("\t from ID: ");
					Serial.println(slave_id[i]);
					
					if (c & EMERGENCY) {
						requests_emergency |= 1 << i;
					}
					if (c & NORTH_SOUTH) {
						requests_crosswalk |= 1 << (i + 0);
					}
					if (c & EAST_WEST) {
						requests_crosswalk |= 1 << (i + 4);

					}
					if (c & 0x80) {
						// fault flag from slave
						testing_pattern.status_reg |= FAULT;
						fault = 1;
					}
				}// else Serial.println("Zero response from slave");
			}

	}


	unsigned long light_on_time = millis() - light_time;

	if ((requests_crosswalk & 0x0F) && (current_traffic_pattern == EW_GREEN) && (light_on_time > 4 * 1000))  {
		testing_pattern.status_reg |= STANDARD_UPDATE;
		Serial.println("Updated status register from inside pollSlaves()");
	}
	else if ((requests_crosswalk & 0xF0) && (current_traffic_pattern == NS_GREEN) && (light_on_time > 4 * 1000))  {
		testing_pattern.status_reg |= STANDARD_UPDATE;
		Serial.println("Updated status register from inside pollSlaves()");
	}

	unsigned char re  = requests_emergency;
	if (re) {
		if (!((re & (re - 1)) == 0)) { // if not a power of 2 / more than 1 bit set
			Serial.println("Multiple emergency requests");
			if (current_traffic_pattern == NS_RED) {
				testing_pattern.status_reg |= URGENT_UPDATE;
			}	
			else if (current_traffic_pattern == EW_RED) {
				testing_pattern.status_reg |= URGENT_UPDATE;
			}
			else if (current_traffic_pattern == NS_GREEN) {
				testing_pattern.status_reg |= URGENT_UPDATE;
			}
			else if (current_traffic_pattern == EW_GREEN) {
				testing_pattern.status_reg |= URGENT_UPDATE;
			}
		}
		else { // only 1 light
			Serial.println("Single emergency requests");
			unsigned char idx = 0;
			for (int i = 0; i < 4; i++) {
				if ((re >> i) & 0x01) idx = i; // bit shift to find which slave index
			}
			Serial.println(idx);

			if (idx == 1 || idx == 3) {
				// If slave is NS
				if (current_traffic_pattern == EW_GREEN) {
					testing_pattern.status_reg |= URGENT_UPDATE;
				}
				else if (current_traffic_pattern == NS_RED) {
					testing_pattern.status_reg |= URGENT_UPDATE;
				}


			} 
			else if (idx == 0 || idx == 2) {
				// If slave is EW
				if (current_traffic_pattern == NS_GREEN) {
					testing_pattern.status_reg |= URGENT_UPDATE;
				}
				else if (current_traffic_pattern == EW_RED) {
					testing_pattern.status_reg |= URGENT_UPDATE;
				}
			}
		}
	}
}



void sm_tick(States state) {
	// generic state machine tick
	sys_tick++;
	States old = state;
	//Serial.println("- - - sm_tick");


	//Serial.print("- - - - old state: ");
	//Serial.println(state);
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
			//debug
			//Serial.print("- - - - - - WAIT state_time: ");
			//Serial.println(state_time);
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
			
			// TODO psuedo
			// if (unresolved slave requests) 
			// if (non-responsive slave)
			// if .......


			else state = PROCESS_OUTPUTS;
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

	//Serial.print("- - - - new state: ");
	//Serial.println(state);
	switch (state) {
		// state actions

		
		case INIT:
			// initialize all global variables and "clear" things up
			for (int i = 0; i < 4; i++) {
				testing_pattern.bulbs[i] = (traffic_pattern_lights[0] >> (3 * i)) & 0x07; 
			}
			testing_pattern.time_in_pattern = 4 * 1000; // 10s
			testing_pattern.status_reg = 0x00 | WAITING;

		case START:
			// go through first transition
			break;
		case TRANSMIT:
			// no processing, purely send data
			txLightsFrame(testing_pattern);
			break;
		case WAIT:
			// check if there's any updating to do
			testing_pattern.status_reg =  pollPatternUpdate();
			pollSlaves();
			break;
		case PROCESS_OUTPUTS:
			// calculate next state outputs
			processOutputState();
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

	if (state != old) sys_state_time = sys_time; // new state entered, update this time
	sys_state = state;
}


void setup() {
	// put your setup code here, to run once:
	Wire.begin(); // no ID for master
	Serial.begin(38400); // for debugging TODO

	testing_pattern.status_reg = 0x00 | WAITING ;
	testing_pattern.time_in_pattern = 12345;

	light_controllers[0].slave_id = 8;
	light_controllers[1].slave_id = 16;
	light_controllers[2].slave_id = 32;
	light_controllers[3].slave_id = 64;

}

void loop() {
	// put your main code here, to run repeatedly:
	
	Serial.println("- LOOP - ");
	Serial.print("Status: ");
	Serial.print(testing_pattern.status_reg, BIN);
	Serial.print("\tcurrent_traffic_pattern = ");
	Serial.println(current_traffic_pattern);
	
	sys_time = millis();
	state_time = sys_time - sys_state_time;	
	sm_tick(sys_state);

	delay(TICK_PERIOD);
	
	Serial.println(" ");

}