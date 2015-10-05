/* Pin mapping:
*  P1.0= LED1 -for pedal calibration(get ready) + ultrasonic threshold distance indication
*  P1.1 & .2 = for UART
*  P1.3= Switch3
*  P1.4= ADC1
*  P1.6= LED2 -for pedal calibrating + MIDI tx indication
*  ultrasonic sensor (HC-SRO4):
*  P1.5= trigger
*  P1.7= echo
*  Steps for calibration of foot pedal:
*  1. place Pedal in max position; 2. Press and release Button S2 (P1.3); 3. when green blinks after a series of red blinks, switch the pedal to minimum or lowest position. after the green, you're good to go!
*/

#include "msp430.h"
#include "math.h"
#include "string.h"
char j;
unsigned int a, b[2],  miliseconds;
unsigned int lowerlim2=0;	//lower and upper limits of distance('1') and pot. voltage lev./foot controller('2')
unsigned int upperlim2=1023;
unsigned int lowerlim1=5;	//5cm: change this to set lower threshold distance (minimum possible=0cm)
unsigned int upperlim1=30;	//30cm: change this to set upper threshold distance (max 400cm or 4m!)
unsigned int sensor,dist, distance[2];
unsigned char foot, hand, footold, handold;

inline void serial_log(const char *str);

unsigned int get_adc_0() //ADC at PIN P1.4
{
	unsigned int value_1;
	ADC10CTL1 = INCH_4 + ADC10DIV_3;
	ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE + ADC10SR;
	for( value_1 = 240; value_1 > 0; value_1-- );
	ADC10CTL0 |= ENC + ADC10SC;
	__bis_SR_register(CPUOFF + GIE);
	value_1 = ADC10MEM;
	ADC10CTL0 &= ~ENC;
	return value_1;
}


void serial_log3(unsigned char x)
{
	while (!(IFG2 & UCA0TXIFG));
	UCA0TXBUF = x;
}


void SEND()
{
	{
		footold=foot;
		handold=hand;
		P1OUT &= ~BIT6;
		//conversion: linear relation used here. Change this formula according to your needs.
		foot=(unsigned char)((float)((127)*(((float)(a-lowerlim2))/((float)(upperlim2 -lowerlim2))))); 
		hand=(unsigned char)((float)((127)*(((float)(dist))/((float)(upperlim1)))));
		//refer MIDI specs sheet to understand the concept of status and data byte and their possible values.
		if(footold!=foot){	
	 serial_log3((unsigned char)(0xB1));	//controller on channel 1: 1011-0001
	 serial_log3((unsigned char)(0x04));	//foot controller: 0-0000100
	 serial_log3((unsigned char)(foot));}
		if(handold!=hand){
	 serial_log3((unsigned char)(0xB2));	//controller on channel 2: 1011-0001
	 serial_log3((unsigned char)(0x0C));	//hand controller/effect1: 0-(0xC)
	 serial_log3((unsigned char)(hand));
		}
		P1OUT |= BIT6;
	}
}


inline void serial_log(const char *str)
{
    unsigned int i;
    unsigned int len = strlen(str);
    for (i = 0; i < len; i++) {
        while (!(IFG2 & UCA0TXIFG));
        UCA0TXBUF = str[i];
    }
}


void main()
{
    // Clock init
    WDTCTL = WDTPW + WDTHOLD;               // Stop WDT
    BCSCTL1 = CALBC1_1MHZ;                  // Run at 1 MHz
    DCOCTL = CALDCO_1MHZ;                   // Run at 1 MHz

    // USCI init
    P1SEL = BIT1 + BIT2;                    // Set pin modes to USCI_A0
    P1SEL2 = BIT1 + BIT2;                   // Set pin modes to USCI_A0
    P1DIR |= BIT2;                          // Set 1.2 to output
    UCA0CTL1 |= UCSSEL_2;                   // SMCLK
    UCA0BR0 = 104;                          // 1 MHz -> 9600 baud; 32 for 31250 baud or MIDI
    UCA0BR1 = 0;                            // 1 MHz -> 9600 baud; 0 for 31250 baud or MIDI
    UCA0MCTL = UCBRS1;                      // Modulation UCBRSx = 1
    UCA0CTL1 &= ~UCSWRST;                   // Initialize USCI state machine**

    //timer module:
    CCTL0 = CCIE;                             // CCR0 interrupt enabled
    CCR0 = 1000;				              // 1ms at 1mhz
    TACTL = TASSEL_2 + MC_1;                  // SMCLK, upmode

    P1DIR |= (BIT6+BIT0+BIT5); 	//LED1,2 and trigger
    P1DIR &= ~(BIT3+BIT7);		//calib button and echo pin
    P1OUT &= ~(BIT6+BIT5+BIT0);	//turn off LEDs and trigger o/p
    P1REN |= BIT3;
    P1IE |= BIT3+BIT7;
    P1IE &= ~BIT0;      // disable interupt
    P1IFG &= ~(BIT3+BIT7);   //clear interrupt flags: button and echo

    while (1)	//forever loop
    {

    	P1OUT |= BIT5;			// generate pulse for ultrasonic module
    	__delay_cycles(10);             // for 10us
    	P1OUT &= ~BIT5;                 // stop pulse
    	//P1IFG = 0x00;                   // clear flag just in case anything happened before
    	P1IES &= ~BIT7;			// rising edge on ECHO pin
     	__delay_cycles(30000);          // delay for 30ms (after this time echo times out if there is no object detected)

     	{	//taking a sample and adding to buffer for averaging
     		distance[0]=distance[1];
     		distance[1] =(sensor/58);           // converting ECHO lenght into cm
     		b[0]=b[1];
			b[1]=get_adc_0();
     		for (j=0; j<2; j++) { dist += distance[j];  a +=b[j];}

     		dist /=3;
     		if (dist>upperlim1) dist=upperlim1;
     		else if(dist<lowerlim1) dist=lowerlim1;

     		a /=3;
     		if (a>upperlim2) a=upperlim2;
     		else if(a<lowerlim2) a=lowerlim2;

     		SEND();	//send midi value
     	}

     	if((dist == lowerlim1 )||(dist == upperlim1 )) P1OUT |= BIT0;  //turning LED on if distance is dis exceeds thresholds.
     	else P1OUT &= ~BIT0;

    }
}


#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void) // ADC Interrupt Service Routine
{__bic_SR_register_on_exit(CPUOFF);
}


#pragma vector=PORT1_VECTOR  //ISR
__interrupt void Port_1(void)
{
		if (P1IFG & BIT3)		//if button is pressed: calibrate
		{	
			P1IFG &= ~BIT3;		//clear flags
			P1IE &= ~(BIT3+BIT7); //disable interrupts
			P1OUT &= ~(BIT0+BIT6);	//switch off leds
			for (j=0; j<5; j++)	{		//indication that calibration is beginning: upper limit for pedal; user is required put pedal in max. position
							P1OUT |= BIT0; __delay_cycles(300000);
							P1OUT &= ~BIT0; __delay_cycles(300000);
						}
				P1OUT |= BIT6;
				__delay_cycles(100000);
				upperlim2=get_adc_0();
				P1OUT &= ~BIT6;

				for (j=0; j<5; j++)	{		//indication that calibration is beginning: lower limit for pedal; user is required put pedal in min. position
								P1OUT |= BIT0; __delay_cycles(300000);
								P1OUT &= ~BIT0; __delay_cycles(300000);
							}
				P1OUT |= BIT6;
				__delay_cycles(100000);
				lowerlim2=get_adc_0();
				P1OUT &= ~BIT6;

				P1IE |= BIT3+BIT7;	//enable interrupts again
		}


		if(P1IFG & BIT7)  //if echo received
		{
			unsigned int timer=TAR;
		          if(!(P1IES & BIT7)) // is this the rising edge?
		          {
		            TACTL|=TACLR;   // clears timer A
		            miliseconds = 0;
		            P1IES |= BIT7;  //falling edge
		          }
		          else  {
		            sensor = (long)miliseconds*1000 + timer;	//calculating ECHO lenght= 'miliseconds'(in usec)+'TAR'(counter value in usec)
		          }
		    P1IFG &= ~BIT7;				//clear flag
		}
}


#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
  miliseconds++;
}