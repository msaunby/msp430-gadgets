/* alarm_clock.c (main program, no dependencies other than standard libc.a)
 * Michael Saunby  https://github.com/msaunby/msp430-gadgets
 * July 2012.
 * A simple four digit LED alarm clock for a 14 pin MSP430 chip.
 * The shortage of I/O pins requires a "creative"
 * re-deployment of pins depending on mode.
 *
 * Works with MSP430F2001
 */
#include <msp430.h>

inline void show_seg(int dnum, int snum);
inline void show_digits( unsigned char *digits );
void incr_clock( unsigned char *digits, int incr );
inline void beep(void);

/*
   The time HH:MM as digits 32:10
*/
unsigned char digits[] = {0,0,0,0};
unsigned char display[] = {0,0,0,0};
unsigned char alarm[] = {0,0,7,0};


/*
      AAA
     F   B
     F   B
      GGG
     E   C
     E   C
      DDD  DP
*/

#define SEG_A 1
#define SEG_B (1<<1)
#define SEG_C (1<<2)
#define SEG_D (1<<3)
#define SEG_E (1<<4)
#define SEG_F (1<<5)
#define SEG_G (1<<6)


unsigned char segs[] = {
SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F,
SEG_B|SEG_C,
SEG_A|SEG_B|SEG_G|SEG_E|SEG_D,
SEG_A|SEG_B|SEG_G|SEG_C|SEG_D,
SEG_F|SEG_G|SEG_B|SEG_C,
SEG_A|SEG_F|SEG_G|SEG_C|SEG_D,
SEG_A|SEG_F|SEG_E|SEG_D|SEG_C|SEG_G,
SEG_A|SEG_B|SEG_C,
SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G,
SEG_G|SEG_F|SEG_A|SEG_B|SEG_C|SEG_D};

#define BUTTON BIT3

#define DIG0 BIT4
#define DIG1 BIT2
#define DIG2 BIT1
#define DIG3 BIT0

#define disp_show_ticks 21
volatile unsigned char disp_show_count = 0;

volatile unsigned char buttons = 0;
volatile unsigned char set_alarm = 0;
volatile unsigned char do_alarm = 0;

#define CANCEL_BUT 5

void main(void)
{
  unsigned char last_but = 0;

  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

  TACCR0 = 16384 - 1; // Xtal freq / required clock rate (2Hz) - 1
  TACTL = TASSEL_1 + MC_1 + TAIE;           // ACLK, contmode, interrupt

  //  Button stuff.  Button input serves two functions.
  // When display is off serves to cancel alarm if sounding and
  // switch display on. These enables the other, strobed, buttons.
  P1DIR &= ~BUTTON; // Direction in.
  P1REN |= BUTTON; // Enable resistor.
  P1OUT &= ~BUTTON; // Select pull-down.
  P1IES |= BUTTON;        // high->low edge (button release)
  P1IFG &= ~BUTTON;       // IFG cleared

  P1DIR |= (DIG0|DIG1|DIG2|DIG3|BIT5|BIT6|BIT7);
  P1OUT &= ~(BIT5|BIT6|BIT7);
  __enable_interrupt();
  incr_clock(digits,1);
  while(1){
	  unsigned char *d = digits;

	  	  if(do_alarm==0x1){
	  		  beep();
	  	  }
     	  if(buttons){
        	  // restart the display on timer if any buttons are pressed
     		  disp_show_count = disp_show_ticks;
     		  if(do_alarm){
     			  // setting this bit silences the alarm.
     			  do_alarm |= 0x2;
     		  }
     	  }
     	  // Try and prevent presses of the alarm-cancel / display-on button
     	  // doing other things.
     	  if(buttons == 0xff)buttons=0;
     	  if(buttons != last_but){
     		  last_but = buttons;
     		  continue;
     	  }
     	  if(disp_show_count){
	    	if(buttons == DIG3){
    	    	set_alarm ^= 1;
	    	}
	    	if(set_alarm) d = alarm;
	    	if(buttons == DIG0){
	    			incr_clock(d,1);  // fast set
	    	}
	    	else if(buttons == DIG1){
	    			incr_clock(d,50);  // slow set
	    	}else{
	    		;
	    	}
			buttons = 0;
			show_digits(d);
	      }else{
	    	// turn all digits off
	    	P1OUT &= ~(DIG0|DIG1|DIG2|DIG3);
	    	buttons = 0;
	    	set_alarm = 0;
	    	// enable button interrupt.
	    	P1IE |= BUTTON;
    	    __low_power_mode_3();
	      }
  }
}


#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
    P1IFG &= ~BUTTON;                           // P1.4 IFG cleared
    buttons = 0xff;
    P1IE &= ~BUTTON;
}

// Timer_A3 Interrupt Vector (TAIV) handler
volatile unsigned char ticks=0;
volatile char disp_on = 0;

#pragma vector=TIMERA1_VECTOR
__interrupt void Timer_A(void)
{
  switch( TAIV )
  {
    //case  2:     // CCR1 not used
    //case  4:     // CCR2 not used
    //	break;
    case 10:
    	if(set_alarm){
    	  disp_on ^= 1;
    	}else{
    		disp_on = 1;
    	}
    	ticks++;
    	if(ticks==120){incr_clock(digits,1);ticks=0;}
    	//
    	if((digits[0]==alarm[0])&&(digits[1]==alarm[1])&&(digits[2]==alarm[2])&&(digits[3]==alarm[3])){
    	  do_alarm |= 0x1;
    	}else{
    		do_alarm = 0;
    	}
    	// For testing use -
    	// incr_clock(1);
    	if(disp_show_count)disp_show_count--;
    	break;
    default:
    	break;
  }
  __low_power_mode_off_on_exit();
}

volatile unsigned char counter = 0;

void incr_clock(unsigned char *digits, int count ){
  if(count > 1){
	  counter++;
	  if(counter < count) return;
  }
  counter = 0;
  digits[0]++;
  if(digits[0]>9){digits[0]=0;digits[1]++;}
  if(digits[1]>5){digits[1]=0;digits[2]++;}
  if(digits[2]>9){digits[2]=0;digits[3]++;}
  if((digits[3]==2)&&(digits[2]==4)){digits[3]=digits[2]=digits[1]=digits[0]=0;}

}

#define shift 5  // output on P1.5, P1.6, P1.7

inline void show_digits( unsigned char *digits ){
	 // convert the BCD digits to 7 segment(A-G) codes.
	int dnum, snum;
	for(dnum=0;dnum<4;dnum++){
		display[dnum] = segs[digits[dnum]];
		for(snum=0;snum<7;snum++){
			if(display[dnum] & (1<<snum)) show_seg(dnum,snum);
		}
	}
}


inline void show_seg(int dnum, int snum){
	// Sets output lines to control 7 segment LED display.
	// Also detects button pushes as these are strobed
	// by the digit select lines.
	P1OUT &= ~(DIG0|DIG1|DIG2|DIG3|(0x7 << shift));
	if(disp_on){
	  // Set the three segment select bits.
	  P1OUT |= ((snum & 0x7) << shift);
	}else{
		P1OUT |= 0x6 << shift;  // show central dash - seg G (#6).
	}
	switch(dnum){
		case 0:
			P1OUT |= DIG0;
			if(P1IN & BUTTON) buttons = DIG0;
			break;
		case 1:
			P1OUT |= DIG1;
			if(P1IN & BUTTON) buttons = DIG1;
			break;
		case 2:
			P1OUT |= DIG2;
			if(P1IN & BUTTON) buttons = DIG2;
			break;
		case 3:
			P1OUT |= DIG3;
			if(P1IN & BUTTON) buttons = DIG3;
			break;
	}
	// Tweak the delay to get a flicker free display refresh.
	__delay_cycles(1000); // 200 = 900Hz digit refresh
	// turn off all digit select lines and test for a button
	// press. If there is one then it's the display on/alarm cancel button.
	// This button connects BUTTON (P1.3) to VCC.
	P1OUT &= ~(DIG0|DIG1|DIG2|DIG3);
	if(P1IN & BUTTON) buttons = 0xff;
}

inline void beep(){
	int x;
	P1OUT |= (0x7 << shift);
	for (x=0;x < 100;x++){
		P1OUT ^= BIT1;
     	 __delay_cycles(1500);
    }
	P1OUT &= ~(DIG0|DIG1|DIG2|DIG3);
	P1OUT &= ~(0x7 << shift);
}
