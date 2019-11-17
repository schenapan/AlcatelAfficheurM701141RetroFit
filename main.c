#include <atmel_start.h>

#include "display.h"

extern uint32_t getUsCounter(void);

int main(void)
{
	// debug
	static uint32_t l_time;

	/* Initializes MCU, drivers and middleware */
	atmel_start_init();

	/* Replace with your application code */
	
	// initialize display
	dispInit();
	
	// debug
	l_time = getUsCounter();
	
	// main loop
	while (1) {

		// debug
		if( (uint32_t)(getUsCounter()-l_time) > 250000 )
		{
			l_time += 250000;
			
			// toggle next pixel
			static uint8_t line_loop = 0;
			
			for( uint8_t l_digit_loop=0; l_digit_loop<DISP_NB_DIGITS; l_digit_loop++ )
			{
				for( uint8_t l_column_loop=0; l_column_loop<DISP_NB_COLUMNS; l_column_loop++ )
				{
					dispToggleLed(l_digit_loop,line_loop%DISP_NB_LINES,l_column_loop);
				}
			}
			dispApply();
			line_loop++;
		}

		// display
		dispTick();
	}
}
