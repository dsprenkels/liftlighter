#define F_CPU 8000000L
#define BAUD 9600UL
#define BLOCK_BEGIN_M 45
#define BLOCK_END_M 30
#define BLOCK_ANNOUNCE_M 37
#define DEFAULT_FLASH_FREQ 1
#define DEFAULT_MSG_LEN 80
#define LONG_PRESS_DURATION 1000 /* ms */


#include "random.h"
#include "usart.h"
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>


struct Input {
	volatile unsigned char *ddr_reg;
	const unsigned char ddr_shl;
	volatile unsigned char *pin_reg;
	const unsigned char pin_shl;
};


struct Output {
	volatile unsigned char *ddr_reg;
	const unsigned char ddr_shl;
	volatile unsigned char *port_reg;
	const unsigned char port_shl;
};


// ticks (256 ticks correspond to one second)
volatile char HOUR = 0, MINUTE = 0, SECOND = 0, TICK = 0;

// state invalidated (if ==true, then update the state of the lights)
volatile bool STATE_INVALIDATED = 1; // state invalidated on startup

const struct Input CONTROL_BUTTON = {&DDRD, DDD7, &PIND, PIND7};
const struct Input S_SWITCH = {&DDRD, DDD5, &PIND, PIND5};
const struct Output CPUBUSY_LED = {&DDRD, DDD4, &PORTD, PD4};
const struct Output LIGHTS[] = {
	{&DDRB, DDB0, &PORTB, PB0},
	{&DDRB, DDB1, &PORTB, PB1},
	{&DDRB, DDB2, &PORTB, PB2},
	{&DDRB, DDB3, &PORTB, PB3},
	{&DDRB, DDB4, &PORTB, PB4},
	{&DDRB, DDB5, &PORTB, PB5},
	{&DDRC, DDC0, &PORTC, PC0},
	{&DDRC, DDC1, &PORTC, PB1},
	{&DDRC, DDC2, &PORTC, PB2},
	{&DDRC, DDC3, &PORTC, PB3}
};
const int LIGHT_COUNT = sizeof(LIGHTS) / sizeof(LIGHTS[0]);
volatile enum {K_OFF, K_ON, K_FLASHING} K_STATE = K_OFF;
volatile enum {CONTROL_OFF, CONTROL_HOUR, CONTROL_MINUTE, CONTROL_SECOND}
	CONTROL_STATE = CONTROL_OFF;
volatile int CONTROL_BUTTON_PRESSED_MS = 0;


/* UTILITY FUNCTIONS */

bool time_is_between_s(char h0, char m0, char s0, char h1, char m1, char s1)
{
	const char start[] = {h0, m0, s0};
	const char end[] = {h1, m1, s1};
	const char now[] = {HOUR, MINUTE, SECOND};
	if (memcmp(start, end, 3) < 0) {
		// normal
		return memcmp(start, now, 3) < 0 && memcmp(now, end, 3) < 0;
	} else {
		// end < start
		return !(memcmp(start, now, 3) < 0 && memcmp(now, end, 3) < 0);
	}
}


bool time_is_between(char h0, char m0, char h1, char m1)
{
	return time_is_between_s(h0, m0, 0, h1, m1, 0);
}


bool time_even_second()
{
	return SECOND % 2 == 0;
}


bool time_flashing_on(const int freq)
{
	const int dticks = 256 / (2*freq);
	return (TICK / dticks) % 2 == 0;
}


bool control_button_is_down()
{
	return (*CONTROL_BUTTON.pin_reg & (1 << CONTROL_BUTTON.pin_shl)) == 0;
}


/* CPUBUSY_LED CONTROL */

void cpubusy_on()
{
	*CPUBUSY_LED.port_reg |= CPUBUSY_LED.port_shl;
}


void cpubusy_off()
{
	*CPUBUSY_LED.port_reg &= ~CPUBUSY_LED.port_shl;
}


/* LIGHT SWITCHING */

void switch_lights(const bool lights_on[LIGHT_COUNT])
{
	int i;
	char maskb = 0, maskc = 0;
	char portb = 0, portc = 0;
	const struct Output *light;

	// which bits are we allowed to touch?
	for (i = 0; i < LIGHT_COUNT; i++) {
		maskb |= LIGHTS[i].port_reg == &PORTB;
		maskc |= LIGHTS[i].port_reg == &PORTC;
	}

	// build new PORT{B,C} values
	for (i = 0; i < LIGHT_COUNT; i++) {
		light = &LIGHTS[i];
		if (light->port_reg == &PORTB) {
			portb |= lights_on[i] << light->port_shl;
		}
		if (light->port_reg == &PORTC) {
			portc |= lights_on[i] << light->port_shl;
		}
	}

	// output values
	PORTB = (PORTB & ~maskb) | portb;
	PORTC = (PORTC & ~maskc) | portc;
}


/* LIGHT LOGIC */

bool get_light_down_value()
{
	return time_is_between(21, 30, 8, 0);
}


bool get_light_one_value()
{
	// on during the first block
	if (time_is_between(8, BLOCK_BEGIN_M, 10, BLOCK_END_M)) {
		return true;
	} else if (time_is_between(8, BLOCK_ANNOUNCE_M, 8, BLOCK_BEGIN_M)) {
		return time_flashing_on(DEFAULT_FLASH_FREQ);
	}
	return false;
}


bool get_light_two_value()
{
	// on during the second block
	if (time_is_between(10, BLOCK_BEGIN_M, 12, BLOCK_END_M)) {
		return 1;
	} else if (time_is_between(10, BLOCK_ANNOUNCE_M, 10, BLOCK_BEGIN_M)) {
		return time_flashing_on(DEFAULT_FLASH_FREQ);
	} else {
		return 0;
	}
}


bool get_light_three_value() {
	// on during the third block
	if (time_is_between(13, BLOCK_BEGIN_M, 15, BLOCK_END_M)) {
		return 1;
	} else if (time_is_between(13, BLOCK_ANNOUNCE_M, 13, BLOCK_BEGIN_M)) {
		return time_flashing_on(DEFAULT_FLASH_FREQ);
	} else {
		return 0;
	}
}


bool get_light_four_value() {
	// on during the fourth block
	if (time_is_between(15, BLOCK_BEGIN_M, 17, BLOCK_END_M)) {
		return 1;
	} else if (time_is_between(15, BLOCK_ANNOUNCE_M, 15, BLOCK_BEGIN_M)) {
		return time_flashing_on(DEFAULT_FLASH_FREQ);
	} else {
		return 0;
	}
}


bool get_light_five_value() {
	// opening times of the Refter
	return time_is_between(17, 0, 19, 0);
}


bool get_light_b_value() {
	// is it time for beer?
	return time_is_between(16, 00, 21, 00);
}


bool get_light_k_value() {
	return K_STATE == K_ON || (K_STATE == K_FLASHING && time_flashing_on(2));
}


bool get_light_s_value() {
	// on if we are in the southern canteen (controlled by switch)
	return (*S_SWITCH.pin_reg & (1 << S_SWITCH.pin_shl)) != 0;
}


bool get_light_up_value() {
	return time_is_between( 8, BLOCK_ANNOUNCE_M,  8, BLOCK_BEGIN_M) ||
		   time_is_between(10, BLOCK_ANNOUNCE_M, 10, BLOCK_BEGIN_M) ||
		   time_is_between(13, BLOCK_ANNOUNCE_M, 13, BLOCK_BEGIN_M) ||
		   time_is_between(15, BLOCK_ANNOUNCE_M, 15, BLOCK_BEGIN_M);
}

/* CHANGING THE CURRENT TIME */

void control_button_shortpress()
{
	char msg[DEFAULT_MSG_LEN];

	// update time figure
	switch (CONTROL_STATE) {
		case CONTROL_OFF:
			// do nothing
			break;
		case CONTROL_HOUR:
			HOUR++;
			break;
		case CONTROL_MINUTE:
			MINUTE++;
			break;
		case CONTROL_SECOND:
			SECOND++;
			break;
		default:
			snprintf(msg, sizeof(msg), "[ERROR] (%s:%i) invalid CONTROL_STATE: %i\r\n",
					 __FILE__, __LINE__, CONTROL_STATE);
			usart_transmit_str(msg);
			CONTROL_STATE = CONTROL_OFF;
			break;
	}
}

void control_button_longpress()
{
	char msg[DEFAULT_MSG_LEN];

	// go to the next control state
	switch (CONTROL_STATE) {
		case CONTROL_OFF:
			CONTROL_STATE = CONTROL_HOUR;
			break;
		case CONTROL_HOUR:
			CONTROL_STATE = CONTROL_MINUTE;
			break;
		case CONTROL_MINUTE:
			CONTROL_STATE = CONTROL_SECOND;
			break;
		case CONTROL_SECOND:
			CONTROL_STATE = CONTROL_OFF;
			break;
		default:
			snprintf(msg, sizeof(msg), "[ERROR] (%s:%i) invalid CONTROL_STATE: %i\r\n",
					 __FILE__, __LINE__, CONTROL_STATE);
			usart_transmit_str(msg);
			CONTROL_STATE = CONTROL_OFF;
			break;
	}
}


/* INTERRUPT HANDLERS */

ISR(INT0_vect)
{
	// the CONTROL button is down, reset the timer
	CONTROL_BUTTON_PRESSED_MS = 0;
}

ISR(TIMER2_OVF_vect)
{
	if (++TICK == 0) {
		STATE_INVALIDATED = true;
		goto inc_second;
	}
	return;

	inc_second:
	if (++SECOND == 60) {
		SECOND = 0;
		goto inc_minute;
	}
	return;

	inc_minute:
	if (++MINUTE == 60) {
		MINUTE = 0;
		goto inc_hour;
	}
	return;

	inc_hour:
	if (++HOUR == 24) {
		HOUR = 0;
	}
	return;
}


/* MAINLOOP FUNCTIONS */

void init()
{
	int i;
	const struct Output *light;
	unsigned char ddrb = 0, ddrc = 0, ddrd = 0;
	char msg[DEFAULT_MSG_LEN];

	// set cpubusy led pin to output
	*CPUBUSY_LED.ddr_reg |= 1 << CPUBUSY_LED.ddr_shl;

	// set all light pins to output
	for (i = 0; i < LIGHT_COUNT; i++) {
		light = &LIGHTS[i];
		if (light->ddr_reg == &DDRB) {
			ddrb |= 1 << light->ddr_shl;
		} else if (light->ddr_reg == &DDRC) {
			ddrc |= 1 << light->ddr_shl;
		} else if (light->ddr_reg == &DDRD) {
			ddrd |= 1 << light->ddr_shl;
		} else {
			snprintf(msg, sizeof(msg), "[ERROR] (%s:%i) invalid DDRx register: 0x%p\r\n",
			         __FILE__, __LINE__, light->ddr_reg);
			usart_transmit_str(msg);
		}
	}
	DDRB |= ddrb;
	DDRC |= ddrc;
	DDRD |= ddrd;
	_NOP(); // for synchronization

	// turn on all lights to indicate startup
	const bool lights_on[] = {true, true, true, true, true,
		                      true, true, true, true, true};
	switch_lights(lights_on);

	// set CONTROL_BUTTON to input
	*CONTROL_BUTTON.ddr_reg &= ~(1 << CONTROL_BUTTON.ddr_shl);
	_NOP();

	// set S_SWITCH to input
	*S_SWITCH.ddr_reg &= ~(1 << S_SWITCH.ddr_shl);
	_NOP();

	// initialize Timer/Counter2 to measure seconds
	_delay_ms(1000); // wait for crystal to stabilize
	ASSR |= 1 << AS2; // set async clocking
	// prescaler 128 s.t. 1 second exactly overflows a 8 bit value
	TCCR2 = (1 << CS22) | (1 << CS20); // set the rest to 0
	while ((ASSR & (1 << TCR2UB)) != 0) {} // wait TCCR2 to update
	TIMSK |= 1 << TOIE2; // enable overflow interrupt

	// setup the INT0 interrupt source
	MCUCR |= 1 << ISC01; // on falling edge
	GICR |= 1 << INT0; // enable interrupt on INT0

#ifdef ENABLE_WATCHDOG
	// enable watchdog Timer (watchdog of about 2 secs)
	wdt_reset();
	WDTCR |= (1 << WDE) | (1 << WDP2) | (1 << WDP1) | (1 << WDP0);
#endif /* ENABLE_WATCHDOG */

	// enable global interrupt
	sei();
}


void update_state()
{
	char msg[DEFAULT_MSG_LEN];

	switch (K_STATE) {
		case K_ON:
		case K_OFF:
			// once every 5 hours (random), choose for `K` a new random state
			if (randint(1, 5*60*60) <= 1) {
				// go flashing one every 24 times (rare; once per 5 days)
				if (randint(1, 24) <= 1) {
					K_STATE = K_FLASHING;
				} else {
					K_STATE = !K_STATE;
				}
			}
			break;
		case K_FLASHING:
			// stop flashing after (on average) 100 seconds
			if (randint(1, 100) <= 1) {
				K_STATE = K_OFF;
			}
			break;
		default:
			// unreachable state, reset
			snprintf(msg, sizeof(msg), "[ERROR] (%s:%i) invalid K_STATE: %i\r\n",
			         __FILE__, __LINE__, K_STATE);
			usart_transmit_str(msg);
			K_STATE = K_OFF;
	}
}


void update_lights()
{
	bool lights_on[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int figure;
	char msg[DEFAULT_MSG_LEN];
	int i;

	if (CONTROL_STATE == CONTROL_OFF) {
		lights_on[0] = get_light_down_value();
		lights_on[1] = get_light_one_value();
		lights_on[2] = get_light_two_value();
		lights_on[3] = get_light_three_value();
		lights_on[4] = get_light_four_value();
		lights_on[5] = get_light_five_value();
		lights_on[6] = get_light_b_value();
		lights_on[7] = get_light_k_value();
		lights_on[8] = get_light_s_value();
		lights_on[9] = get_light_up_value();
	} else {
		switch (CONTROL_STATE) {
			case CONTROL_OFF:
				snprintf(msg, sizeof(msg), "[ERROR] (%s:%i) unreachable\r\n",
				         __FILE__, __LINE__);
				usart_transmit_str(msg);
				return;
			case CONTROL_HOUR:
				figure = HOUR;
				break;
			case CONTROL_MINUTE:
				figure = MINUTE;
				break;
			case CONTROL_SECOND:
				figure = SECOND;
				break;
			default:
				snprintf(msg, sizeof(msg), "[ERROR] (%s:%i) invalid CONTROL_STATE: %i\r\n",
				         __FILE__, __LINE__, CONTROL_STATE);
				usart_transmit_str(msg);
				CONTROL_STATE = CONTROL_OFF;
				return;
		}
		for (i = 0; i < LIGHT_COUNT; i++) {
			lights_on[LIGHT_COUNT-i] = (figure & (1 << i)) != 0 && time_flashing_on(2);
		}
	}
	switch_lights(lights_on);
}


int main(void)
{
	// initialize USART for debugging
	usart_init();

	usart_transmit_str("[INFO] starting liftlighter\r\n");
	init();

	usart_transmit_str("[INFO] going into sleep mode\r\n");
	MCUCR |= 1 << SE; // enable sleep

	do_sleep:
	sleep_cpu();
	while (STATE_INVALIDATED) {
		cpubusy_on();
		#ifdef ENABLE_WATCHDOG
				wdt_reset(); // reset watchdog
		#endif /* ENABLE_WATCHDOG */

		if (control_button_is_down()) {
			if (CONTROL_BUTTON_PRESSED_MS == LONG_PRESS_DURATION) {
				control_button_longpress();
			} else {
				_delay_ms(1);
				CONTROL_BUTTON_PRESSED_MS++;
			}
		} else if (CONTROL_BUTTON_PRESSED_MS != 0) {
			// control button has *just* been lifted
			control_button_shortpress();
		} else {
			STATE_INVALIDATED = false;
			update_state();
		}

		update_lights();

		cpubusy_off();
	}
	goto do_sleep;

	return 0;
}
