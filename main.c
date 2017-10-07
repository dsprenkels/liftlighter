#define F_CPU 1000000L
#define BLOCK_BEGIN_M 45
#define BLOCK_END_M 30
#define BLOCK_ANNOUNCE_M 37
#define DEFAULT_FLASH_FREQ 1.0
#define LONG_PRESS_DURATION 1000 /* ms */
// Location: Nijmegen, The Netherlands
#define LOCATION_LONGITUDE 51.8126
#define LOCATION_LATITUDE 5.8372

// Update the state twice per second
#define UPDATES_PER_SECOND 2.0

#include "random.h"
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdbool.h>
#include <stdio.h>
#include <util/delay.h>
#include <util/eu_dst.h>
#include <time.h>

#ifdef ENABLE_WATCHDOG
#include <avr/wdt.h>
#endif /* ENABLE_WATCHDOG */


struct tm_hms {
	int8_t tm_hour;
	int8_t tm_min;
	int8_t tm_sec;
};


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


volatile bool PRESSING_CONTROL_BUTTON = false;

// inputs and outputs
const struct Input CONTROL_BUTTON = {&DDRD, DDD2, &PIND, PIND2};
const struct Input S_SWITCH = {&DDRD, DDD5, &PIND, PIND5};
const struct Output CPUBUSY_LED = {&DDRD, DDD4, &PORTD, PD4};
const struct Output LIGHTS[] = {
	{&DDRB, PB0, &PORTB, PB0},
	{&DDRB, PB1, &PORTB, PB1},
	{&DDRB, PB2, &PORTB, PB2},
	{&DDRB, PB3, &PORTB, PB3},
	{&DDRB, PB4, &PORTB, PB4},
	{&DDRB, PB5, &PORTB, PB5},
	{&DDRC, PC0, &PORTC, PC0},
	{&DDRC, PC1, &PORTC, PC1},
	{&DDRC, PC2, &PORTC, PC2},
	{&DDRC, PC3, &PORTC, PC3}
};
const size_t LIGHT_COUNT = sizeof(LIGHTS) / sizeof(LIGHTS[0]);
volatile enum {K_OFF, K_ON, K_FLASHING} K_STATE = K_OFF;
volatile enum {
	CONTROL_OFF,
	CONTROL_HOUR, CONTROL_MINUTE, CONTROL_SECOND,
	CONTROL_DAY, CONTROL_MONTH, CONTROL_YEAR
} CONTROL_STATE = CONTROL_OFF;
volatile uint16_t CONTROL_BUTTON_PRESSED_MS = 0;


/* UTILITY FUNCTIONS */

static struct tm_hms tm_hms_new_hms(const uint8_t h, const uint8_t m, const uint8_t s)
{
	struct tm_hms ret;
	ret.tm_hour = h;
	ret.tm_min = m;
	ret.tm_sec = s;
	return ret;
}


static struct tm_hms tm_hms_new_hm(const uint8_t h, const uint8_t m)
{
	return tm_hms_new_hms(h, m, 0);
}


static bool tm_hms_lt(const struct tm_hms x, const struct tm_hms y)
{
	if (x.tm_hour < y.tm_hour) return true;
	if (x.tm_hour > y.tm_hour) return false;
	if (x.tm_min < y.tm_min) return true;
	if (x.tm_min > y.tm_min) return false;
	if (x.tm_sec < y.tm_sec) return true;
	return false;
}


static bool tm_hms_le(const struct tm_hms x, const struct tm_hms y)
{
	if (x.tm_hour < y.tm_hour) return true;
	if (x.tm_hour > y.tm_hour) return false;
	if (x.tm_min < y.tm_min) return true;
	if (x.tm_min > y.tm_min) return false;
	if (x.tm_sec <= y.tm_sec) return true;
	return false;
}


static struct tm_hms tm_hms_from_tm(const struct tm *in)
{
	struct tm_hms ret;
	ret.tm_hour = in->tm_hour;
	ret.tm_min = in->tm_min;
	ret.tm_sec = in->tm_sec;
	return ret;
}


static bool tm_hms_is_between(const struct tm_hms cur,
                              const struct tm_hms start,
                              const struct tm_hms end)
{
	if (tm_hms_lt(start, end)) {
		// normal
		return tm_hms_le(start, cur) && tm_hms_le(cur, end);
	} else {
		// end < start
		return tm_hms_le(start, cur) || tm_hms_le(cur, end);
	}
}


static bool time_flashing_on(const float freq)
{
	return time(NULL) % 2 == 0;
}


static bool control_button_is_down()
{
	return (*CONTROL_BUTTON.pin_reg & (1 << CONTROL_BUTTON.pin_shl)) != 0;
}


/* CPUBUSY_LED CONTROL */

static void cpubusy_on()
{
	*CPUBUSY_LED.port_reg |= (1 << CPUBUSY_LED.port_shl);
}


static void cpubusy_off()
{
	*CPUBUSY_LED.port_reg &= ~(1 << CPUBUSY_LED.port_shl);
}


/* LIGHT SWITCHING */

static void switch_lights(const bool lights_on[LIGHT_COUNT])
{
	size_t i;
	uint8_t maskb = 0, maskc = 0;
	uint8_t portb = 0, portc = 0;
	const struct Output *light;

	// which bits are we allowed to touch?
	for (i = 0; i < LIGHT_COUNT; i++) {
		maskb |= (uint8_t) ((LIGHTS[i].port_reg == &PORTB) << LIGHTS[i].port_shl);
		maskc |= (uint8_t) ((LIGHTS[i].port_reg == &PORTC) << LIGHTS[i].port_shl);
	}

	// build new PORT{B,C} values
	for (i = 0; i < LIGHT_COUNT; i++) {
		light = &LIGHTS[i];
		if (light->port_reg == &PORTB) {
			portb |= (uint8_t) (lights_on[i] << light->port_shl);
		}
		if (light->port_reg == &PORTC) {
			portc |= (uint8_t) (lights_on[i] << light->port_shl);
		}
	}

	// output values
	PORTB = (uint8_t) (PORTB & ~maskb) | portb;
	PORTC = (uint8_t) (PORTC & ~maskc) | portc;
}


/* LIGHT LOGIC */

static bool get_light_down_value(const struct tm *cur_tm)
{
	const struct tm_hms cur = tm_hms_from_tm(cur_tm);
	const struct tm_hms start = tm_hms_new_hm(21, 30);
	const struct tm_hms end = tm_hms_new_hm(8, 00);
	return tm_hms_is_between(cur, start, end);
}


static bool get_light_one_value(const struct tm *cur_tm)
{
	// on during the first block
	const struct tm_hms cur = tm_hms_from_tm(cur_tm);
	const struct tm_hms start = tm_hms_new_hm(8, BLOCK_END_M);
	const struct tm_hms end = tm_hms_new_hm(10, BLOCK_END_M);
	const struct tm_hms a_start = tm_hms_new_hm(8, BLOCK_ANNOUNCE_M);
	const struct tm_hms a_end = tm_hms_new_hm(8, BLOCK_BEGIN_M);

	if (tm_hms_is_between(cur, start, end)) {
		return true;
	} else if (tm_hms_is_between(cur, a_start, a_end)) {
		return time_flashing_on(DEFAULT_FLASH_FREQ);
	}
	return false;
}


static bool get_light_two_value(const struct tm *cur_tm)
{
	// on during the second block
	const struct tm_hms cur = tm_hms_from_tm(cur_tm);
	const struct tm_hms start = tm_hms_new_hm(10, BLOCK_END_M);
	const struct tm_hms end = tm_hms_new_hm(12, BLOCK_END_M);
	const struct tm_hms a_start = tm_hms_new_hm(10, BLOCK_ANNOUNCE_M);
	const struct tm_hms a_end = tm_hms_new_hm(10, BLOCK_BEGIN_M);

	if (tm_hms_is_between(cur, start, end)) {
		return true;
	} else if (tm_hms_is_between(cur, a_start, a_end)) {
		return time_flashing_on(DEFAULT_FLASH_FREQ);
	}
	return false;
}


static bool get_light_three_value(const struct tm *cur_tm)
{
	// on during the third block
	const struct tm_hms cur = tm_hms_from_tm(cur_tm);
	const struct tm_hms start = tm_hms_new_hm(13, BLOCK_END_M);
	const struct tm_hms end = tm_hms_new_hm(15, BLOCK_END_M);
	const struct tm_hms a_start = tm_hms_new_hm(13, BLOCK_ANNOUNCE_M);
	const struct tm_hms a_end = tm_hms_new_hm(13, BLOCK_BEGIN_M);

	if (tm_hms_is_between(cur, start, end)) {
		return true;
	} else if (tm_hms_is_between(cur, a_start, a_end)) {
		return time_flashing_on(DEFAULT_FLASH_FREQ);
	}
	return false;
}


static bool get_light_four_value(const struct tm *cur_tm)
{
	// on during the fourth block
	const struct tm_hms cur = tm_hms_from_tm(cur_tm);
	const struct tm_hms start = tm_hms_new_hm(15, BLOCK_END_M);
	const struct tm_hms end = tm_hms_new_hm(17, BLOCK_END_M);
	const struct tm_hms a_start = tm_hms_new_hm(15, BLOCK_ANNOUNCE_M);
	const struct tm_hms a_end = tm_hms_new_hm(15, BLOCK_BEGIN_M);

	if (tm_hms_is_between(cur, start, end)) {
		return true;
	} else if (tm_hms_is_between(cur, a_start, a_end)) {
		return time_flashing_on(DEFAULT_FLASH_FREQ);
	}
	return false;
}


static bool get_light_five_value(const struct tm *cur_tm)
{
	// opening times of the Refter
	const struct tm_hms cur = tm_hms_from_tm(cur_tm);
	const struct tm_hms start = tm_hms_new_hm(15, 0);
	const struct tm_hms end = tm_hms_new_hm(17, 0);
	return tm_hms_is_between(cur, start, end);
}


static bool get_light_b_value(const struct tm *cur_tm)
{
	// is it time for beer?
	const struct tm_hms cur = tm_hms_from_tm(cur_tm);
	const struct tm_hms start = tm_hms_new_hm(16, 0);
	const struct tm_hms end = tm_hms_new_hm(21, 0);
	return tm_hms_is_between(cur, start, end);
}


static bool get_light_k_value(const struct tm *cur_tm)
{
	return K_STATE == K_ON || (K_STATE == K_FLASHING && time_flashing_on(2.0));
}


static bool get_light_s_value(const struct tm *_)
{
	// on if we are in the southern canteen (controlled by switch)
	return (*S_SWITCH.pin_reg & (1 << S_SWITCH.pin_shl)) != 0;
}


static bool get_light_up_value(const struct tm *cur_tm)
{
	const struct tm_hms cur = tm_hms_from_tm(cur_tm);
	const struct tm_hms a_start1 = tm_hms_new_hm(8, BLOCK_ANNOUNCE_M);
	const struct tm_hms a_end1 = tm_hms_new_hm(8, BLOCK_BEGIN_M);
	const struct tm_hms a_start2 = tm_hms_new_hm(10, BLOCK_ANNOUNCE_M);
	const struct tm_hms a_end2 = tm_hms_new_hm(10, BLOCK_BEGIN_M);
	const struct tm_hms a_start3 = tm_hms_new_hm(13, BLOCK_ANNOUNCE_M);
	const struct tm_hms a_end3 = tm_hms_new_hm(13, BLOCK_BEGIN_M);
	const struct tm_hms a_start4 = tm_hms_new_hm(15, BLOCK_ANNOUNCE_M);
	const struct tm_hms a_end4 = tm_hms_new_hm(15, BLOCK_BEGIN_M);

	return tm_hms_is_between(cur, a_start1, a_end1) ||
	       tm_hms_is_between(cur, a_start2, a_end2) ||
	       tm_hms_is_between(cur, a_start3, a_end3) ||
	       tm_hms_is_between(cur, a_start4, a_end4);
}


/* CHANGING THE CURRENT TIME */

static void control_button_shortpress()
{
	time_t current_time = time(NULL);
	struct tm current_tm;

	localtime_r(&current_time, &current_tm);

	// update time figure
	switch (CONTROL_STATE) {
		case CONTROL_OFF:
			// do nothing
			return;
		case CONTROL_HOUR:
			current_tm.tm_hour = (current_tm.tm_hour + 1) % 24;
			break;
		case CONTROL_MINUTE:
			current_tm.tm_min = (current_tm.tm_min + 1) % 60;
			break;
		case CONTROL_SECOND:
			// do not add 1, but reset to minute offset
			current_tm.tm_sec = 0;
			break;
		case CONTROL_DAY:
			current_tm.tm_mday = (current_tm.tm_mday + 1) %
				month_length(1900 + (uint16_t) current_tm.tm_year,
				             current_tm.tm_mon + 1);
			break;

		case CONTROL_MONTH:
			current_tm.tm_mon = (current_tm.tm_mon + 1) % 12;
			break;
		case CONTROL_YEAR:
			if (++current_tm.tm_year >= 200) {
				current_tm.tm_year -= 100;
			}
			break;
		default:
			CONTROL_STATE = CONTROL_OFF;
			return;
	}

	// write the new system time
	set_system_time(mktime(&current_tm));
}


static void control_button_longpress()
{
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
			CONTROL_STATE = CONTROL_DAY;
			break;
		case CONTROL_DAY:
			CONTROL_STATE = CONTROL_MONTH;
			break;
		case CONTROL_MONTH:
			CONTROL_STATE = CONTROL_YEAR;
			break;
		default:
			CONTROL_STATE = CONTROL_OFF;
			break;
	}
}


/* INTERRUPT HANDLERS */

ISR(INT0_vect)
{
	// the CONTROL button is down, reset the timer
	PRESSING_CONTROL_BUTTON = true;
}

ISR(TIMER2_OVF_vect)
{
	system_tick();
}


/* MAINLOOP FUNCTIONS */

static void init()
{
	size_t i;
	const struct Output *light;
	uint8_t ddrb = 0, ddrc = 0, ddrd = 0;

	// set cpubusy led pin to output
	*CPUBUSY_LED.ddr_reg |= (uint8_t) (1 << CPUBUSY_LED.ddr_shl);
	_NOP();

	// set all light pins to output
	for (i = 0; i < LIGHT_COUNT; i++) {
		light = &LIGHTS[i];
		if (light->ddr_reg == &DDRB) {
			ddrb |= (uint8_t) (1 << light->ddr_shl);
		} else if (light->ddr_reg == &DDRC) {
			ddrc |= (uint8_t) (1 << light->ddr_shl);
		} else if (light->ddr_reg == &DDRD) {
			ddrd |= (uint8_t) (1 << light->ddr_shl);
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
	*CONTROL_BUTTON.ddr_reg &= (uint8_t) ~(1 << CONTROL_BUTTON.ddr_shl);
	_NOP();

	// set S_SWITCH to input
	*S_SWITCH.ddr_reg &= (uint8_t) ~(1 << S_SWITCH.ddr_shl);
	_NOP();

	// initialize the system time
#ifdef DEFAULT_TIME
	set_system_time(DEFAULT_TIME - UNIX_OFFSET);
#else /* DEFAULT_TIME */
	set_system_time(0);
#endif /* DEFAULT_TIME */
	set_zone(+1 * ONE_HOUR);
	set_dst(eu_dst);
	set_position(LOCATION_LONGITUDE, LOCATION_LATITUDE);

	// initialize Timer/Counter2 to measure seconds
	_delay_ms(1000); // wait for crystal to stabilize
	ASSR |= 1 << AS2; // set async clocking
	// prescaler 128 s.t. 1 second exactly overflows a 8 bit value
	TCCR2 = (1 << CS22) | (1 << CS20); // set the rest to 0
	while ((ASSR & (1 << TCR2UB)) != 0) {} // wait TCCR2 to update
	TIMSK |= 1 << TOIE2; // enable overflow interrupt

	// setup the INT0 interrupt source
	MCUCR |= (1 << ISC00) | (1 << ISC01); // on rising edge
	GICR |= 1 << INT0; // enable interrupt on INT0

#ifdef ENABLE_WATCHDOG
	// enable watchdog Timer (watchdog of about 2 secs)
	WDTCR |= (1 << WDE) | (1 << WDP2) | (1 << WDP1) | (1 << WDP0);
	_NOP();
	wdt_reset();
#endif /* ENABLE_WATCHDOG */

	// enable global interrupt
	sei();
}


static void update_state()
{
	switch (K_STATE) {
		case K_ON:
		case K_OFF:
			// once every 5 hours (random), choose for `K` a new random state
			if (randint(1, 5*60*60) <= 1) {
				// go flashing once every 24 times (rare; once per 5 days)
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
			K_STATE = K_OFF;
	}
}


static void update_lights_normal(bool *lights_on, const struct tm *current_tm)
{
	lights_on[0] = get_light_down_value(current_tm);
	lights_on[1] = get_light_one_value(current_tm);
	lights_on[2] = get_light_two_value(current_tm);
	lights_on[3] = get_light_three_value(current_tm);
	lights_on[4] = get_light_four_value(current_tm);
	lights_on[5] = get_light_five_value(current_tm);
	lights_on[6] = get_light_b_value(current_tm);
	lights_on[7] = get_light_k_value(current_tm);
	lights_on[8] = get_light_s_value(current_tm);
	lights_on[9] = get_light_up_value(current_tm);
}


static void update_lights_control(bool *lights_on, const struct tm *current_tm)
{
	uint32_t figure;
	size_t i;

	switch (CONTROL_STATE) {
		case CONTROL_OFF:
			return;
		case CONTROL_HOUR:
			figure = current_tm->tm_hour;
			break;
		case CONTROL_MINUTE:
			figure = current_tm->tm_min;
			break;
		case CONTROL_SECOND:
			figure = current_tm->tm_sec;
			break;
		case CONTROL_DAY:
			figure = current_tm->tm_mday;
			break;
		case CONTROL_MONTH:
			figure = current_tm->tm_mon + 1; // s.t. 1 is January
			break;
		case CONTROL_YEAR:
			figure = current_tm->tm_year - 100; // s.t. 0 is equiv to 2000
			break;
		default:
			CONTROL_STATE = CONTROL_OFF;
			return;
	}

	// show the figure in binary format (flashing with 0.5 Hz)
	for (i = 0; i < LIGHT_COUNT; i++) {
		lights_on[LIGHT_COUNT-(i+1)] = ((figure & ((uint32_t) 1 << i)) != 0);
	}

}


static void update_lights()
{
	bool lights_on[] = {false, false, false, false, false,
	                    false, false, false, false, false};
	const time_t current_time = time(NULL);
	struct tm current_tm;

	localtime_r(&current_time, &current_tm);

	if (CONTROL_STATE != CONTROL_OFF) {
		update_lights_control(lights_on, &current_tm);
	} else {
		update_lights_normal(lights_on, &current_tm);
	}
	switch_lights(lights_on);
}


int main(void)
{
	init();
	MCUCR |= 1 << SE; // enable sleep

	do_sleep:
	sleep_cpu();

	// reset the watchdog
	#ifdef ENABLE_WATCHDOG
	wdt_reset();
	#endif /* ENABLE_WATCHDOG */

	while (true) {
		if (PRESSING_CONTROL_BUTTON && control_button_is_down()) {
			if (CONTROL_BUTTON_PRESSED_MS >= LONG_PRESS_DURATION) {
				// trigger long press
				control_button_longpress();
				CONTROL_BUTTON_PRESSED_MS = 0;
				// disable the button until the next 'down' event
				PRESSING_CONTROL_BUTTON = false;
			} else {
				CONTROL_BUTTON_PRESSED_MS++;
				_delay_ms(1);
			}
		} else if (PRESSING_CONTROL_BUTTON && CONTROL_BUTTON_PRESSED_MS > 0) {
				// button has *just* been lifter, trigger short press
				control_button_shortpress();
				CONTROL_BUTTON_PRESSED_MS = 0;
				PRESSING_CONTROL_BUTTON = false;
		} else {
			// do update logic
			update_state();

			// show new light state
			update_lights();

			if (CONTROL_STATE == CONTROL_OFF) {
				cpubusy_off();
			} else {
				cpubusy_on();
			}
		}
	}
	goto do_sleep;

	return 0;
}
