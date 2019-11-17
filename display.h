/*
 * cafficheur.h
 *
 * Created: 11/11/2019 12:19:35
 *  Author: sebw
 */ 


#ifndef CAFFICHEUR_H_
#define CAFFICHEUR_H_

#define DISP_NB_DIGITS	12
#define DISP_NB_COLUMNS	5
#define DISP_NB_LINES	7

// initialize Display to all off
void dispInit(void);
	
// tick to call in main loop
void dispTick(void);

// load shadow digit array to real display
void dispApplyDigit( uint8_t i_digit );

// load shadow array to real display
void dispApply( void );

// set digit shadows array all led to on or off
void dispSetDigitLedsOff( uint8_t i_digit, bool i_Off );

// set shadow array all digits to off
void dispSetAllLedsOff( void );


// basic function to toggle a specific led in shadow array
void dispToggleLed( uint8_t i_digit, uint8_t i_line, uint8_t i_column );

	

#endif /* CAFFICHEUR_H_ */