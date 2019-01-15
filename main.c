#include <avr/io.h>
#include "Timer.h"

void Set_A2D_Pin(unsigned char pinNum);
void ADC_init();

typedef struct task {
	int state; // Current state of the task
	unsigned long period; // Rate at which the task should tick
	unsigned long elapsedTime; // Time since task's previous tick
	int (*Tick)(int); // Function to call for task's tick
} task;

const unsigned char numTasks = 2;
const unsigned long tasksPeriodGCD = 10;
const unsigned long periodX = 10;
const unsigned long periodY = 10;

task tasks[2];

enum xStates {xStart, xSetADC, xWait, xProcessADC};
int TickXAxis(int xState) {
	static unsigned char i = 0;
	static signed short x;
	switch (xState) // transitions
	{
		case xStart:
			xState = xSetADC;
			break;
		case xSetADC:
			xState = xWait;
			break;
		case xWait:
			if (i < 5) {
				xState = xWait;
			}
			else {
				xState = xProcessADC;
			}
			break;
		case xProcessADC:
			xState = xSetADC;
			break;
		default:
			xState = xWait;
			break;
	}
	switch (xState) { // actions
		case xSetADC:
			Set_A2D_Pin(0);
			break;
		case xWait:
			++i;
			break;
		case xProcessADC:
			i = 0;
			
			//ADC ranges from +/- 512
			x = ADC;
			
			// calibration results
			// - 800:  0   degrees
			// - 1630: 90  degrees
			// - 2500: 180 degrees
			
			// +/- joystick position to 90 degrees (i.e. 1630)
			OCR1A = ICR1 - 1630 + x;
			break;
		default:
			break;
	}
	return xState;
}
enum yStates {yStart, ySetADC, yWait, yProcessADC};
int TickYAxis(int yState) {
	static unsigned char j = 5;
	static unsigned char filter = 50;
	static unsigned short y;
	switch (yState) // transitions
	{
		case yStart:
			yState = ySetADC;
			break;
		case ySetADC:
			yState = yWait;
			break;
		case yWait:
			if (j < 5) {
				yState = yWait;
			}
			else {
				yState = yProcessADC;
			}
			break;
		case yProcessADC:
			yState = ySetADC;
			break;
		default:
			yState = yWait;
			break;
	}
	switch (yState) { // actions
		case ySetADC:
			Set_A2D_Pin(1);
			break;
		case yWait:
			++j;
			break;
		case yProcessADC:
			j = 0; // reset counter
			
			y = ADC;
			
			// LED bar output, joystick centered: 0b10 0000 0000			
			if(y > 0x200 + filter) {
				PORTC = 0x01; // car goes forward
			}
			else if(y < 0x200 - filter) {
				PORTC = 0x02; // car goes backward
			}
			else {
				PORTC = 0x00; // idle
			}
			break;
		default:
			break;
	}
	return yState;
}

void TimerISR() {
	unsigned char i;
	for (i = 0; i < numTasks; ++i) {
		if ( tasks[i].elapsedTime >= tasks[i].period ) { // Ready
			tasks[i].state = tasks[i].Tick(tasks[i].state);
			tasks[i].elapsedTime = 0;
		}
		tasks[i].elapsedTime += tasksPeriodGCD;
	}
}

int main()
{
	TCCR1A |= 1<<WGM11 | 1<<COM1A1 | 1<<COM1A0;
	TCCR1B |= 1<<WGM13 | 1<<WGM12 | 1<<CS10;
	ICR1 = 19999;

	DDRC = 0xFF; PORTC = 0x00; // output for motor driver
	DDRD = 0xFF; PORTD = 0x00; // output for servo
	
	unsigned char i = 0;
	tasks[i].state = 0;
	tasks[i].period = periodX;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].Tick = &TickXAxis;
	++i;
	tasks[i].state = 0;
	tasks[i].period = periodY;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].Tick = &TickYAxis;
	
	TimerSet(tasksPeriodGCD);
	TimerOn();
	
	ADC_init(); 
	
	while (1) {	}
	return 0;
}

void Set_A2D_Pin(unsigned char pinNum) {
	// NOTE: this function is taken from the CS122A labs
	// Pins on PORTA are used as input for A2D conversion
	// The default channel is 0 (PA0)
	// The value of pinNum determines the pin on PORTA
	// used for A2D conversion
	// Valid values range between 0 and 7, where the value
	// represents the desired pin for A2D conversion
	ADMUX = (pinNum <= 0x07) ? pinNum : ADMUX;
	// Allow channel to stabilize
	static unsigned char i = 0;
	for ( i=0; i<15; i++ ) { asm("nop"); }
}
void ADC_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	// ADEN: setting this bit enables analog-to-digital conversion.
	// ADSC: setting this bit starts the first conversion.
	// ADATE: setting this bit enables auto-triggering. Since we are
	//        in Free Running Mode, a new conversion will trigger whenever
	//        the previous conversion completes.
}