/*
 * cafficheur.cpp
 *
 * Created: 11/11/2019 12:19:12
 *  Author: sebw
 */ 
#include <atmel_start.h>
#include <string.h>

#include "display.h"

// define when JTAG is connected
#define JTAG_DEBUG


//////////////////////////////////////////////////////////////////////////
//
// COLUMN : LOW LEVEL TO ENABLE
//	1	|	2	|	3	|	4	|	5*	|
// PC0	|	PC1	|	PC7	|	PC6	|	PC5	|
//
// CLK DIGIT : LOW TO HIGH FRONT
//	1	|	2	|	3	|	4	|	5	|	6	|	7	|	8	|	9*	|	10*	|	11*	|	12	|
// PB7	|	PB6	|	PB5	|	PB4	|	PB3	|	PB2	|	PB1	|	PB0	|	PC4	|	PC3	|	PC2	|	PA1	|
//
// LINE : LOW LEVEL FOR LED ON
//	1	|	2	|	3	|	4	|	5	|	6	|	7	|
//	PA0	|	PA7	|	PA6	|	PA5	|	PA4	|	PA3	|	PA2	|
//
// JTAG : PC2-5
//
//////////////////////////////////////////////////////////////////////////

// leds column mask
#define DISP_LED_COLUMN_1_MASK (1<<0)
#define DISP_LED_COLUMN_2_MASK (1<<1)
#define DISP_LED_COLUMN_3_MASK (1<<7)
#define DISP_LED_COLUMN_4_MASK (1<<6)
#define DISP_LED_COLUMN_5_MASK (1<<5)
// all
#define DISP_LED_COLUMNS_MASK (DISP_LED_COLUMN_1_MASK|DISP_LED_COLUMN_2_MASK|DISP_LED_COLUMN_3_MASK|DISP_LED_COLUMN_4_MASK|DISP_LED_COLUMN_5_MASK)

// led line mask
#define DISP_LED_LINE_1_MASK (1<<0)
#define DISP_LED_LINE_2_MASK (1<<7)
#define DISP_LED_LINE_3_MASK (1<<6)
#define DISP_LED_LINE_4_MASK (1<<5)
#define DISP_LED_LINE_5_MASK (1<<4)
#define DISP_LED_LINE_6_MASK (1<<3)
#define DISP_LED_LINE_7_MASK (1<<2)
// all
#define DISP_LINE_LEDS_MASK	(~(1<<1))

// individual mask for digit clock
// -B
#define DISP_DIGIT_1_CLK_MASK	(1<<7)
#define DISP_DIGIT_2_CLK_MASK	(1<<6)
#define DISP_DIGIT_3_CLK_MASK	(1<<5)
#define DISP_DIGIT_4_CLK_MASK	(1<<4)
#define DISP_DIGIT_5_CLK_MASK	(1<<3)
#define DISP_DIGIT_6_CLK_MASK	(1<<2)
#define DISP_DIGIT_7_CLK_MASK	(1<<1)
#define DISP_DIGIT_8_CLK_MASK	(1<<0)
// -C
#define DISP_DIGIT_9_CLK_MASK	(1<<4)
#define DISP_DIGIT_10_CLK_MASK	(1<<3)
#define DISP_DIGIT_11_CLK_MASK	(1<<2)
// -A
#define DISP_DIGIT_12_CLK_MASK	(1<<1)

// useful mask for clock
#define DISP_DIGIT_PORT_A_CLOCK_MASK (DISP_DIGIT_12_CLK_MASK)
#define DISP_DIGIT_PORT_B_CLOCK_MASK (0xFF)
#define DISP_DIGIT_PORT_C_CLOCK_MASK (DISP_DIGIT_9_CLK_MASK|DISP_DIGIT_10_CLK_MASK|DISP_DIGIT_11_CLK_MASK)


static inline void dispSetColumnsOff(void){ PORTC_set_port_level(DISP_LED_COLUMNS_MASK, true); }

static inline void dispSetLineLedsOff(void)
{
	// load register with off value for line leds 1-7
	PORTA_set_port_level(DISP_LINE_LEDS_MASK, true);
	// clock digit 1-8
	PORTB_set_port_level(DISP_DIGIT_PORT_A_CLOCK_MASK, false);
	PORTB_set_port_level(DISP_DIGIT_PORT_A_CLOCK_MASK, true);
	PORTB_set_port_level(DISP_DIGIT_PORT_A_CLOCK_MASK, false);
	// clock digit 9-11
	#ifndef JTAG_DEBUG
	PORTC_set_port_level(DISP_DIGIT_PORT_C_CLOCK_MASK, false);
	PORTC_set_port_level(DISP_DIGIT_PORT_C_CLOCK_MASK, true);
	PORTC_set_port_level(DISP_DIGIT_PORT_C_CLOCK_MASK, false);
	#endif
	// clock digit 12
	PORTA_set_port_level(DISP_DIGIT_PORT_A_CLOCK_MASK, false);
	PORTA_set_port_level(DISP_DIGIT_PORT_A_CLOCK_MASK, true);
	PORTA_set_port_level(DISP_DIGIT_PORT_A_CLOCK_MASK, false);
}


// time management
#define DISP_COLUMN_REFRESH_RATE_US	4000 // 4ms
extern uint32_t getUsCounter(void);
uint32_t disp_ref_time = 0;

// pixel management
// main array to display by digit/column, each byte is leds value of a column
uint8_t disp_led_array[DISP_NB_DIGITS][DISP_NB_COLUMNS];
uint8_t disp_led_array_shadow[DISP_NB_DIGITS][DISP_NB_COLUMNS];
struct sDispDigitCtrl{
	bool apply; // want to apply digit from shadow to real
}dips_digit_ctrl[DISP_NB_DIGITS];

// real array display a column
void dispColumn( uint8_t i_column_number );

// load shadow array to real display
static inline void dispAsyncApply( void )
{
	for( uint8_t loop=0; loop<DISP_NB_DIGITS; loop++ )
	{
		if( dips_digit_ctrl[loop].apply )
		{
			memcpy( disp_led_array[loop], disp_led_array_shadow[loop], DISP_NB_COLUMNS );
			dips_digit_ctrl[loop].apply = false;
		}
	}
}


void dispInit(void)
{
	// set IO to output
	PORTA_set_port_dir(0xFF, PORT_DIR_OUT);
	PORTB_set_port_dir(0xFF, PORT_DIR_OUT);
	PORTC_set_port_dir(0xFF, PORT_DIR_OUT);
	
	// default level
	// column off
	dispSetColumnsOff();

	// LINEs led off
	dispSetLineLedsOff();
	
	// update array to off
	dispSetAllLedsOff();
	dispApply();
	
	// get reference time
	disp_ref_time = getUsCounter();
}

void dispTick(void)
{
	static uint8_t l_current_column = 0;
	
	// do waiting operation if first digit
	dispAsyncApply();
	
	
	// refresh one column display every 4ms
	if( (uint32_t)(getUsCounter()-disp_ref_time) > DISP_COLUMN_REFRESH_RATE_US )
	{
		// update time
		disp_ref_time += DISP_COLUMN_REFRESH_RATE_US;
		
		// refresh column
		dispColumn( l_current_column );
			
		// next column
		l_current_column++;
		if( l_current_column >= DISP_NB_COLUMNS ){ l_current_column = 0; }
	}
}

// set digit shadows array all led to on or off
void dispSetDigitLedsOff( uint8_t i_digit, bool i_Off )
{
	memset( disp_led_array_shadow[i_digit], i_Off?DISP_LINE_LEDS_MASK:0, DISP_NB_COLUMNS );
}

// set shadow array all digits to off
void dispSetAllLedsOff( void )
{
	for( uint8_t loop=0; loop<DISP_NB_DIGITS; loop++ ) { dispSetDigitLedsOff( loop, true ); }
}

// load shadow digit array to real display
void dispApplyDigit( uint8_t i_digit )
{
	dips_digit_ctrl[i_digit].apply = true;
}

// load shadow array to real display
void dispApply( void )
{
	for( uint8_t loop=0; loop<DISP_NB_DIGITS; loop++ ) { dispApplyDigit(loop); }
}

void dispToggleLed( uint8_t i_digit, uint8_t i_line, uint8_t i_column )
{
	uint8_t l_led_mask;
	uint8_t l_led_value;
	
	if( 0 == i_line ){ l_led_mask = DISP_LED_LINE_1_MASK; }
	else if( 1 == i_line ){ l_led_mask = DISP_LED_LINE_2_MASK; }
	else if( 2 == i_line ){ l_led_mask = DISP_LED_LINE_3_MASK; }
	else if( 3 == i_line ){ l_led_mask = DISP_LED_LINE_4_MASK; }
	else if( 4 == i_line ){ l_led_mask = DISP_LED_LINE_5_MASK; }
	else if( 5 == i_line ){ l_led_mask = DISP_LED_LINE_6_MASK; }
	else if( 6 == i_line ){ l_led_mask = DISP_LED_LINE_7_MASK; }
		
	// reverse pixel value
	l_led_value = disp_led_array_shadow[i_digit][i_column] & l_led_mask;
	l_led_value = ~l_led_value;
	l_led_value &= l_led_mask;
		
	// set to 0
	disp_led_array_shadow[i_digit][i_column] &= ~l_led_mask;
	// set to wanted value
	disp_led_array_shadow[i_digit][i_column] |= l_led_value;
}


void dispColumn( uint8_t i_column_number )
{
	// disable previous column
	if( 0 == i_column_number ){ PORTC_set_port_level(DISP_LED_COLUMN_5_MASK, true); }
	else if( 1 == i_column_number ){ PORTC_set_port_level(DISP_LED_COLUMN_1_MASK, true); }
	else if( 2 == i_column_number ){ PORTC_set_port_level(DISP_LED_COLUMN_2_MASK, true); }
	else if( 3 == i_column_number ){ PORTC_set_port_level(DISP_LED_COLUMN_3_MASK, true); }
	else if( 4 == i_column_number ){ PORTC_set_port_level(DISP_LED_COLUMN_4_MASK, true); }
		
	// load register digit by digit
	// digit 1
	PORTA_write_port( disp_led_array[0][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTB_set_port_level(DISP_DIGIT_1_CLK_MASK, true); // clock up
	// digit 2
	PORTA_write_port( disp_led_array[1][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTB_set_port_level(DISP_DIGIT_2_CLK_MASK, true); // clock up
	// digit 3
	PORTA_write_port( disp_led_array[2][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTB_set_port_level(DISP_DIGIT_3_CLK_MASK, true); // clock up
	// digit 4
	PORTA_write_port( disp_led_array[3][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTB_set_port_level(DISP_DIGIT_4_CLK_MASK, true); // clock up
	// digit 5
	PORTA_write_port( disp_led_array[4][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTB_set_port_level(DISP_DIGIT_5_CLK_MASK, true); // clock up
	// digit 6
	PORTA_write_port( disp_led_array[5][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTB_set_port_level(DISP_DIGIT_6_CLK_MASK, true); // clock up
	// digit 7
	PORTA_write_port( disp_led_array[6][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTB_set_port_level(DISP_DIGIT_7_CLK_MASK, true); // clock up
	// digit 8
	PORTA_write_port( disp_led_array[7][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTB_set_port_level(DISP_DIGIT_8_CLK_MASK, true); // clock up
#ifndef JTAG_DEBUG
	// digit 9
	PORTA_write_port( disp_led_array[8][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTC_set_port_level(DISP_DIGIT_9_CLK_MASK, true); // clock up
	// digit 10
	PORTA_write_port( disp_led_array[9][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTC_set_port_level(DISP_DIGIT_10_CLK_MASK, true); // clock up
	// digit 11
	PORTA_write_port( disp_led_array[10][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTC_set_port_level(DISP_DIGIT_11_CLK_MASK, true); // clock up
#endif		
	// digit 12
	PORTA_write_port( disp_led_array[11][i_column_number]&DISP_LINE_LEDS_MASK ); // load
	PORTA_set_port_level(DISP_DIGIT_12_CLK_MASK, true); // clock up
	
	// enable current column
	if( 0 == i_column_number ){ PORTC_set_port_level(DISP_LED_COLUMN_1_MASK, false); }
	else if( 1 == i_column_number ){ PORTC_set_port_level(DISP_LED_COLUMN_2_MASK, false); }
	else if( 2 == i_column_number ){ PORTC_set_port_level(DISP_LED_COLUMN_3_MASK, false); }
	else if( 3 == i_column_number ){ PORTC_set_port_level(DISP_LED_COLUMN_4_MASK, false); }
	else if( 4 == i_column_number ){ PORTC_set_port_level(DISP_LED_COLUMN_5_MASK, false); }
		
	// digit clock down
	PORTB_set_port_level(DISP_DIGIT_1_CLK_MASK, false); // clock up
	PORTB_set_port_level(DISP_DIGIT_2_CLK_MASK, false); // clock up
	PORTB_set_port_level(DISP_DIGIT_3_CLK_MASK, false); // clock up
	PORTB_set_port_level(DISP_DIGIT_4_CLK_MASK, false); // clock up
	PORTB_set_port_level(DISP_DIGIT_5_CLK_MASK, false); // clock up
	PORTB_set_port_level(DISP_DIGIT_6_CLK_MASK, false); // clock up
	PORTB_set_port_level(DISP_DIGIT_7_CLK_MASK, false); // clock up
	PORTB_set_port_level(DISP_DIGIT_8_CLK_MASK, false); // clock up
#ifndef JTAG_DEBUG
	PORTC_set_port_level(DISP_DIGIT_9_CLK_MASK, false); // clock up
	PORTC_set_port_level(DISP_DIGIT_10_CLK_MASK, false); // clock up
	PORTC_set_port_level(DISP_DIGIT_11_CLK_MASK, false); // clock up
#endif
	PORTA_set_port_level(DISP_DIGIT_12_CLK_MASK, false); // clock up
}
