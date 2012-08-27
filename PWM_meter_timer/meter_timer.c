//******************************************************************************
// meter_timer.c (main program, no dependencies other than standard libc.a)
// Michael Saunby  https://github.com/msaunby/msp430-gadgets
// August 2012.
//
// Build with Code Composer Studio V5. Code Size - Text: 700 bytes  Data: 26 bytes
// Tested with MSP430F2001 - as this is the smallest of the family, should be
// fine with larger processors.
//
//  Overview -
//  state 1.  Sleeping - timer off
//  state 2.  Timing - PWM output for time remaining on panel meter.  Buttons change
//             countdown setting.
//  state 3.  Buzzer sounding - PWM output to piezo sounder.
//
// Watch crystal required.
//
//               MSP430F2001
//            -----------------
//        /|\|              XIN|-
//         | |                 | 32kHz
//         --|RST          XOUT|-
//           |                 |
//           |         P1.2/TA1|--> Meter
//           |         P1.6/TA1|--> Sounder
//           -------------------
//
//******************************************************************************

#include <msp430.h>


/*
 * P1.0 (LED0 - for debug only)
 * P1.2 OUTPUT PWM to meter (TA1)
 * P1.3 INPUT Up (Launchpad) button
 * P1.4 INPUT Down button
 * P1.6 OUTPUT PWM to buzzer (TA1)   (LED1)
 */

#define BUTTON_UP BIT3
#define BUTTON_DN BIT4

inline void timer_set_up();
inline void timer_set_down();
inline void display_time();
inline void alarm(void);


// With 2k2 resistor and 1mA FSD meter these values
// work for me.
#define max_ccr1 358
#define METER_CCR0 511
// Timer A vector freq In Hz
#define CLK_FREQ 64

// times are in seconds
volatile unsigned timer = 60;
//volatile unsigned saved_timer = 60;
volatile int clk = 0;
volatile int set_disable = 0;
#define DISABLE_COUNT 20

void main(void)
{
    WDTCTL = WDTPW + WDTHOLD;     // Stop WDT

    /*
     * Really a loop as enable WDT at end to force a restart.
     */
    BCSCTL3 |= XCAP_0;            // xtal caps
    /* For diagnostics set P1.0 (LED0) as output.
     * On launchpad BIT0 and BIT6 have LEDs fitted,
     * setting low rather that as inputs reduced current.
     */
	P1DIR |= (BIT2|BIT6|BIT0);
	P1OUT &= ~(BIT2|BIT6|BIT0);

    TACTL = 0; // Enable PWM mode when required.

	 // buttons set as inputs
	P1DIR &= ~(BUTTON_UP|BUTTON_DN);
	  /* select pull up resistors */
	P1OUT |= (BUTTON_UP|BUTTON_DN);
	  /* enable resistors */
	P1REN |= (BUTTON_UP|BUTTON_DN);
    P1IES &= ~(BUTTON_UP|BUTTON_DN);             // low->hi edge (button release)
    P1IFG &= ~(BUTTON_UP|BUTTON_DN);             // IFG cleared

   	clk=0;
   	P1IE |= (BUTTON_UP | BUTTON_DN); // Button interrupt enabled
   	__enable_interrupt();
   	__low_power_mode_4();  // LPM 3 (timer A off)


   	// Awake.  Prepare PWM output.
   	// Button interrupt disabled as button presses will be handled
   	// in a different way - avoids debouncing.
   	P1IE &= ~(BUTTON_UP | BUTTON_DN);
   	P1OUT |= BIT0; // led0 on (for launchpad)
   	P1SEL |= BIT2;    // TA1 option
   	CCTL1 = OUTMOD_7;        // CCR1 reset/set
   	TACCR0 = METER_CCR0; // PWM period
   	TACTL = TASSEL_1 + MC_1 + TAIE;
   	P1DIR &= ~BIT6; // Ensures sounder is quiet.
   	set_disable = DISABLE_COUNT;  // prevent up/down set for a while
   	// Generate PWM output for meter and also count
   	// timer interrupts. These give a measure of elapsed time.
    __low_power_mode_3();  // LPM 3 (timer A active)
    P1OUT &= ~BIT0;  // led0 off
    P1SEL &= ~BIT2;    // TA1 option off for meter.
    alarm();
    WDTCTL = WDTPW+WDTCNTCL;
/*
 * Processor restarts...
 */
}

inline void alarm(void){
    P1SEL |= BIT6;
	P1DIR |= BIT6;     // P1.6 output
	CCR0 = 32;         // PWM Period
	CCR1 = 8;
	TACTL = TASSEL_1 + MC_1;
	__delay_cycles(4000000);
}


#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
    P1IFG &= ~(BUTTON_UP | BUTTON_DN);                           // P1.4 IFG cleared
    __low_power_mode_off_on_exit();
}

//#define TICKS 0x4000
//#define TICKS 0x4
#define TICKS 64

#pragma vector=TIMERA1_VECTOR
__interrupt void Timer_A(void)
{
 // Diagnostic output. Check freq on pin P1.0 (IC pin 2)

	//P1OUT ^= BIT0;

    switch( TAIV ){
     case 10:
    	 clk++;
    	 if(clk == TICKS){
    	    clk = 0;
    	 	if(timer){
    	 		timer--;
    	 	}
    	 }

    	 //P1OUT ^= BIT0;
    	 display_time();
    	 // check up/down buttons
    	  if(set_disable){
    		  set_disable--;
    	  }
    	  else{
    	    if(!(P1IN & BUTTON_UP)){
    			timer_set_up();
    			set_disable = DISABLE_COUNT;
    		}
    	    else if(!(P1IN & BUTTON_DN)){
    			timer_set_down();
    			set_disable = DISABLE_COUNT;
    		}
    	  }
      break;
    }
    if(timer == 0){
	  TACTL = 0;
	  __low_power_mode_off_on_exit();
	  return;
    }
}

/*
 * Increment timer to next minute (or 5 if over 10 minutes)
 */
inline void timer_set_up()
{
	unsigned int incr = 1; // 1 minute
	clk = 0;

	if(timer > 5400)return;

	if(timer > 600)incr=5;
	timer = ((timer / 60)+incr) * 60;
	//timer = saved_timer;
}

inline void timer_set_down()
{
	unsigned int incr = 1; // 1 minute
	clk = 0;
	if(timer < 60){
		timer = 0;
	}else{
		if(timer > 600)incr=5;
		timer = ((timer/60)-incr) * 60;
		//timer = saved_timer;
	}
}


// Display times in minutes or tenths of a minute
// on a meter reading 0 to 100.
inline void display_time()
{
	if(timer < (95*6)){
		CCR1 = (max_ccr1 * (timer / 6))/100;
	}else{
		CCR1 = (max_ccr1 * (timer / 60))/100;
	}
}

