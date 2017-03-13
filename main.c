/******************************************************************************
 * Timer Output Compare Demo
 *
 * Description:
 *
 * This demo configures the timer to a rate of 1 MHz, and the Output Compare
 * Channel 1 to toggle PORT T, Bit 1 at rate of 10 Hz. 
 *
 * The toggling of the PORT T, Bit 1 output is done via the Compare Result Output
 * Action bits.	
 * 
 * The Output Compare Channel 1 Interrupt is used to refresh the Timer Compare
 * value at each interrupt
 * 
 * Author:
 *	Jon Szymaniak (08/14/2009)
 *	Tom Bullinger (09/07/2011)	Added terminal framework
 *
 *****************************************************************************/


// system includes
#include <hidef.h>			/* common defines and macros */
#include <stdio.h>			/* Standard I/O Library */

// project includes
#include "types.h"
#include "globals.h" // Pull in capture_values
#include "derivative.h" /* derivative-specific definitions */

 
// Count how many times a specific delta value has been "hit"
// Centered at 0x03E8 @ 1KHz - 1000uS
// Index = measured delta - LOW_PERIOD

#define LOW_PERIOD 0x03B6
#define NUM_BUCKETS 100

UINT16 num_occurances[NUM_BUCKETS];
UINT16 capture_values[NUM_CAPTURES];
UINT16 capture_idx;
UINT8 finished_capturing;

// Prototypes

void process_values(void);
void print_values(void);
void pre_capture(void);
void post_capture(void);
void init_buckets(void);

// Definitions

// Change this value to change the frequency of the output compare signal.
// The value is in Hz.
#define OC_FREQ_HZ		((UINT16)10)

// Macro definitions for determining the TC1 value for the desired frequency
// in Hz (OC_FREQ_HZ). The formula is:
//
// TC1_VAL = ((Bus Clock Frequency / Prescaler value) / 2) / Desired Freq in Hz
//
// Where:
//				Bus Clock Frequency		 = 2 MHz
//				Prescaler Value				 = 2 (Effectively giving us a 1 MHz timer)
//				2 --> Since we want to toggle the output at half of the period
//				Desired Frequency in Hz = The value you put in OC_FREQ_HZ
//
#define BUS_CLK_FREQ	((UINT32) 2000000)	 
#define PRESCALE			((UINT16)	2)				 
#define TC1_VAL			 ((UINT16)	(((BUS_CLK_FREQ / PRESCALE) / 2) / OC_FREQ_HZ))


// Initializes SCI0 for 8N1, 9600 baud, polled I/O
// The value for the baud selection registers is determined
// using the formula:
//
// SCI0 Baud Rate = ( 2 MHz Bus Clock ) / ( 16 * SCI0BD[12:0] )
//--------------------------------------------------------------
void InitializeSerialPort(void)
{
	// Set baud rate to ~9600 (See above formula)
	SCI0BD = 13;					
	
	// 8N1 is default, so we don't have to touch SCI0CR1.
	// Enable the transmitter and receiver.
	SCI0CR2_TE = 1;
	SCI0CR2_RE = 1;
}


// Initializes I/O and timer settings for the demo.
//--------------------------------------------------------------			 
void InitializeTimer(void)
{
	// Set the timer prescaler to %2, since the bus clock is at 2 MHz,
	// and we want the timer running at 1 MHz
	TSCR2_PR0 = 1;
	TSCR2_PR1 = 0;
	TSCR2_PR2 = 0;
		
	// Enable input capture on Channel 1
	TIOS_IOS1 = TIOS_INPUT_CAPTURE;
	
	// Set up output compare action to toggle Port T, bit 1
//	TCTL2_OM1 = 0;
//	TCTL2_OL1 = 1;
	TCTL4_EDG1A = 1;
	TCTL4_EDG1B = 0;
	
	// Set up timer compare value
	TC1 = TC1_VAL;
	
	// Clear the Output Compare Interrupt Flag (Channel 1) 
	TFLG1 = TFLG1_C1F_MASK;
	
	// Enable the edge trigger interrupt on Channel 1;
	TIE_C1I = 0;	
	
	// Enable the timer
	TSCR1_TEN = 1;
	 
	//
	// Enable interrupts via macro provided by hidef.h
	//
	EnableInterrupts;
}


// Output Compare Channel 1 Interrupt Service Routine
// Refreshes TC1 and clears the interrupt flag.
//					
// The first CODE_SEG pragma is needed to ensure that the ISR
// is placed in non-banked memory. The following CODE_SEG
// pragma returns to the default scheme. This is neccessary
// when non-ISR code follows. 
//
// The TRAP_PROC tells the compiler to implement an
// interrupt funcion. Alternitively, one could use
// the __interrupt keyword instead.
// 
// The following line must be added to the Project.prm
// file in order for this ISR to be placed in the correct
// location:
//		VECTOR ADDRESS 0xFFEC OC1_isr 
#pragma push
#pragma CODE_SEG __SHORT_SEG NON_BANKED
//--------------------------------------------------------------			 
void interrupt 9 OC1_isr( void )
{		
	// Capture data
	if (capture_idx < NUM_CAPTURES)
	{
		capture_values[capture_idx] = (UINT16)TC1;
		++capture_idx;
	}	
	else if (capture_idx == NUM_CAPTURES)
	{
		TIE_C1I = 0;
		finished_capturing = 1;	
	}	 	 
	// Clear int flag
	TFLG1 = TFLG1_C1F_MASK;
	
}
#pragma pop


// This function is called by printf in order to
// output data. Our implementation will use polled
// serial I/O on SCI0 to output the character.
//
// Remember to call InitializeSerialPort() before using printf!
//
// Parameters: character to output
//--------------------------------------------------------------			 
void TERMIO_PutChar(INT8 ch)
{
	// Poll for the last transmit to be complete
	do
	{
		// Nothing	
	} while (SCI0SR1_TC == 0);
	
	// write the data to the output shift register
	SCI0DRL = ch;
}


// Polls for a character on the serial port.
//
// Returns: Received character
//--------------------------------------------------------------			 
UINT8 GetChar(void)
{ 
	// Poll for data
	do
	{
		// Nothing
	} while(SCI0SR1_RDRF == 0);
	 
	// Fetch and return data from SCI0
	return SCI0DRL;
}	

void init_buckets(void)
{
	memset(num_occurances,0,NUM_BUCKETS * (sizeof(UINT16)));
}	
	
void process_values(void)
{	  
	UINT16 i;
	for (i = 1; i < NUM_CAPTURES; ++i)
	{
		// Get delta between two captures
		UINT16 compare_idx = capture_values[i] - capture_values[i-1];
		
		// Normalize to bucket index (0-99)
		compare_idx -= LOW_PERIOD;
		
		if (compare_idx < 100)
		{
		
			// Place value in bucket
			++num_occurances[compare_idx];
		}
	}		
}	
						  
void print_values(void)
{
	UINT16 i;
	(void)printf("Finished capturing.\r\n");
	(void)printf("100 Buckets used; omitting empty buckets.\r\n");
	for (i = 0; i < NUM_BUCKETS; ++i)
	{
		// If > 0 hits, print the index and how many
		if (num_occurances[i] != 0)
		{
			(void)printf("Bucket %3d: %d\r\n",i+LOW_PERIOD,num_occurances[i]);	
		}
			
	}		
}

// Wait for a user to strike a key bef
void pre_capture(void)
{			 
	(void)printf("Strike enter to begin capture.\r\n"); 
	init_buckets();
				  
	finished_capturing = 0;
	capture_idx = 0;
	// Block on getting a char		
	GetChar();
			
	// Enable the interrupt after the user says to start
	TIE_C1I = 1;
}

// After all data has been captured, wait for user to strike a
// key then print the histogram
void post_capture(void)
{
	process_values();
	print_values();		  
}


// Entry point of our application code
//--------------------------------------------------------------			 
void main(void)
{

	UINT8 userInput;
	capture_idx = 0;
	
	InitializeSerialPort();
	InitializeTimer();
	
	 
	for (;;)
	{
		// Show prompt
		pre_capture();
		while (!finished_capturing)
		{
		 	// nothing
		}
		post_capture();
	}
}
