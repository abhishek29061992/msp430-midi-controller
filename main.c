/* pin mapping:
 * P1.0 - ADC
 * P1.1 - Ultrasonic 'echo'
 * P1.2 - UART Tx Pin
 * P1.3 - foot pedal calibration button (STEPS: align pedal to low level and press button. After ~3 seconds, shift to high level)
 * P1.4,5,6,7 - kbd. output(P1.4-5) and ultrasonic 'trigger'(on P1.4)
 * P2.0-7 - kbd. inputs (See circuit of 'CASIO SA-32')
 */
#include <msp430.h>
#include <string.h>
#include <math.h>
unsigned int ka=0,kb=0,kc=0,kd=0,k2,kaprev,kbprev,kcprev,kdprev;
unsigned char shift=4;	//no. of octaves to shift (max=7)
unsigned char notebegin; //beginning note of 'CASIO SA-32'
unsigned char velocity=127; //the note (ON) velocity
unsigned char foot, footold, hand, handold;
unsigned int sensor,dist, distance[2];
unsigned int a, b[2], miliseconds;
//lower/upper limits of pot. voltage lev. or foot controller('lowerlim2/upperlim2')
unsigned int lowerlim2=0;
unsigned int upperlim2=1023;
//min/max distance for ultrasonic controller level('lowerlim1/upperlim1')
unsigned int lowerlim1=10;	//5cm: change this to set lower threshold distance (minimum possible=0cm)
unsigned int upperlim1=35;	//30cm: change this to set upper threshold distance (max 400cm or 4m; see datasheet)
unsigned int i, j;
unsigned long c;

void send(unsigned char n, unsigned char status);
void scan(unsigned char k);
unsigned int get_adc_0();
void serial_log2(char key);

unsigned int get_adc_0() //ADC at PIN P1.0
{
	unsigned int value_1;
	ADC10CTL1 = INCH_0 + ADC10DIV_3;	//adc10 clk div= 4; PIN P1.0 (A0)
	ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE  + ADC10SR;	//ref selected: Vcc and Vss; ADC10 sample and hold time: 64; ADC10 on; interrupt enable; ADC10 Sampling rate, ref buffer 1: upto 50ksps
	ADC10AE0 |= BIT0;
	for( value_1 = 240; value_1 > 0; value_1-- );
	ADC10CTL0 |= ENC + ADC10SC;	//enable conversion and start conversion
	__bis_SR_register(CPUOFF+ GIE);
	value_1 = ADC10MEM;	//retrieve adc10 value
	ADC10CTL0 &= ~ENC;	// stop encoding
	return value_1;
}

void serial_log2(char key)
{
while (!(IFG2 & UCA0TXIFG));
UCA0TXBUF = key;
}


void main()
{
	// Clock init.
	WDTCTL = WDTPW + WDTHOLD; // Stop WDT
	BCSCTL1 = CALBC1_1MHZ; // Run at 1 MHz
	DCOCTL = CALDCO_1MHZ; // Run at 1 MHz

	//USCI init.
	P1SEL =  BIT2; // Set pin modes to USCI_A0 (output/Tx only)
	P1SEL2 =  BIT2; // Set pin modes to USCI_A0 (output/Tx only)
	UCA0CTL1 |= UCSSEL_2; // SMCLK
	UCA0BR0 = 104; // 1MHz/104 -> 9600
	UCA0BR1 = 0; // 1 MHz -> 9600
	UCA0MCTL = UCBRS1; // Modulation UCBRSx = 1
	UCA0CTL1 &= ~UCSWRST; // Initialize USCI state machine

	//timer init.
	CCTL0 = CCIE;                             // CCR0 interrupt enabled
	CCR0 = 1000;				              // 1ms at 1MHz
	TACTL = TASSEL_2 + MC_1;                  // SMCLK, upmode

	//GPIO init.
	P1DIR |= BIT4+BIT5+BIT6+BIT7;	//output
	P1OUT &= ~(BIT4+BIT5+BIT6+BIT7); // off
	P1DIR &= ~(BIT3+BIT1);		//calib button & echo pin
	P1REN |= BIT3;
	P1IE |= BIT3+BIT1;
	P1IFG &= ~(BIT3+BIT1);   //clear interrupt flags: button
	P2SEL=0;
	P2SEL2=0;
	P2IE=0;
	P2DIR= 0;	//input
	P2IES=0;

	notebegin=5+(12*shift);

	__bis_SR_register(GIE);

while (1)
{
	   {
			P1OUT |= BIT4; 			// generate pulse for ultrasonic module
	    	__delay_cycles(10);             //  10us pulse (See datasheet for working!)
	    	scan(1);
	    	P1OUT &= ~BIT4;                 // stop pulse
	     	__delay_cycles(30000);           // delay for 30ms (after 30s echo times out if no object detected)
	     	P1IES &= ~BIT1;			// rising edge on ECHO pin
	     	{
	     		//taking a sample and adding to buffer for averaging
	     		distance[0]=distance[1];
	     		distance[1] =(sensor/58);           // converting ECHO lenght into cm
	     		if (distance[1]>100) distance[1]=distance[0];  //if object immediately removed, present value=value at which left
	     		b[0]=b[1];
				b[1]=get_adc_0();
	     		for (j=0; j<2; j++) { dist += distance[j];  a +=b[j];}

	     		dist /=3;
	     		if (dist>upperlim1) dist=upperlim1;
	     		else if(dist<lowerlim1) dist=lowerlim1;

	     		a /=3;
	     		if (a>upperlim2) a=upperlim2;
	     		else if(a<lowerlim2) a=lowerlim2;
	     		{
	     				footold=foot;
	     				handold=hand;
	     				//conversion: linear relation used here. Change this formula according to your needs.
	     				foot=(unsigned char)((float)((127)*(((float)(a-lowerlim2))/((float)(upperlim2 -lowerlim2)))));
	     				hand=(unsigned char)((float)((127)*(((float)(dist-lowerlim1))/((float)(upperlim1-lowerlim1)))));
	     				//refer MIDI specs sheet to understand the concept of status and data byte and their possible values.
	     			if(footold!=foot){
	     			serial_log2((unsigned char)(0xB1));	//controller on channel 1: 1011-0001
	     			serial_log2((unsigned char)(0x04));	//foot controller: 0-0000100
	     			serial_log2((unsigned char)(foot));
	     							 }
	     			if(handold!=hand){
	     			serial_log2((unsigned char)(0xB2));	//controller on channel 2: 1011-0001
	     			serial_log2((unsigned char)(0x0C));	//hand controller/effect1: 0-(0xC)
	     			serial_log2((unsigned char)(hand));
	     							}
	     		}
	     	}
	   }


	P1OUT|=BIT5;
	scan(2);
	P1OUT &= ~(BIT4+BIT5+BIT6+BIT7);
	__delay_cycles(500);
	P1OUT|=BIT6;
	scan(3);
	P1OUT &= ~(BIT4+BIT5+BIT6+BIT7);
	__delay_cycles(500);
	//P1OUT|=BIT4;
	//scan(1);
	//P1OUT &= ~(BIT4+BIT5+BIT6+BIT7);
	//__delay_cycles(1000);
	P1OUT|=BIT7;
	scan(4);
	P1OUT &= ~(BIT4+BIT5+BIT6+BIT7);
	__delay_cycles(500);
	}
}


void scan(unsigned char k)
{
if (k==1)	//polling
{	kaprev=ka; //8 bits of 'kx' (x=a/b/c/d) represent the status of 8 keys of port 'x'
	 ka=P2IN;
	 if (kaprev!=ka){
	 if ((kaprev^ka) & BIT0) send(0,ka&BIT0);
	 if ((kaprev^ka) & BIT1) send(1,(ka&BIT1)>>1);
	 if ((kaprev^ka) & BIT2) send(2,(ka&BIT2)>>2);
	 if ((kaprev^ka) & BIT3) send(3,(ka&BIT3)>>3);
	 if ((kaprev^ka) & BIT4) send(4,(ka&BIT4)>>4);
	 if ((kaprev^ka) & BIT5) send(5,(ka&BIT5)>>5);
	 if ((kaprev^ka) & BIT6) send(6,(ka&BIT6)>>6);
	 if ((kaprev^ka) & BIT7) send(7,(ka&BIT7)>>7);
	}
}
	else{
	if (k==2)
		{ kbprev=kb;
		 kb=P2IN;
		 if (kbprev!=kb){   if ((kbprev^kb) & BIT0) send(8,kb&BIT0);
		 if ((kbprev^kb) & BIT1) send(9,(kb&BIT1)>>1);
		 if ((kbprev^kb) & BIT2) send(10,(kb&BIT2)>>2);
		 if ((kbprev^kb) & BIT3) send(11,(kb&BIT3)>>3);
		 if ((kbprev^kb) & BIT4) send(12,(kb&BIT4)>>4);
		 if ((kbprev^kb) & BIT5) send(13,(kb&BIT5)>>5);
		 if ((kbprev^kb) & BIT6) send(14,(kb&BIT6)>>6);
		 if ((kbprev^kb) & BIT7) send(15,(kb&BIT7)>>7);
		 	 	 	 	 }
		}

		else{
		if (k==3)
			{ kcprev=kc;
				kc=P2IN;
				 if (kcprev!=kc){if ((kcprev^kc) & BIT0) send(16,kc&BIT0);
				 if ((kcprev^kc) & BIT1) send(17,(kc&BIT1)>>1);
				 if ((kcprev^kc) & BIT2) send(18,(kc&BIT2)>>2);
				 if ((kcprev^kc) & BIT3) send(19,(kc&BIT3)>>3);
				 if ((kcprev^kc) & BIT4) send(20,(kc&BIT4)>>4);
				 if ((kcprev^kc) & BIT5) send(21,(kc&BIT5)>>5);
				 if ((kcprev^kc) & BIT6) send(22,(kc&BIT6)>>6);
				 if ((kcprev^kc) & BIT7) send(23,(kc&BIT7)>>7);
			 	 	 	 	 }
			}
			else{
			if (k==4)
				{ kdprev=kd;
				  kd=P2IN;
				  if (kdprev!=kd){    if ((kdprev^kd) & BIT0) send(24,kd&BIT0);
					 if ((kdprev^kd) & BIT1) send(25,(kd&BIT1)>>1);
					 if ((kdprev^kd) & BIT2) send(26,(kd&BIT2)>>2);
					 if ((kdprev^kd) & BIT3) send(27,(kd&BIT3)>>3);
					 if ((kdprev^kd) & BIT4) send(28,(kd&BIT4)>>4);
					 if ((kdprev^kd) & BIT5) send(29,(kd&BIT5)>>5);
					 if ((kdprev^kd) & BIT6) send(30,(kd&BIT6)>>6);
					 if ((kdprev^kd) & BIT7) send(31,(kd&BIT7)>>7);
				 	 	 	 	 }
				}
			}
		}
	}
}


void send(unsigned char n, unsigned char status)
{
if (status==0) serial_log2(0x81); //status: 1000-nnnn (nnnn=0001; channel 1) for note OFF
else if (status==1) serial_log2(0x91); //status: 1001-nnnn
serial_log2(n+notebegin); //data1: 0kkkkkkk; k=key (0to127);
serial_log2(velocity);
}

#pragma vector=PORT1_VECTOR  //ISR
__interrupt void Port_1(void)
{
		if(P1IFG & BIT3)		//if button is pressed: calibrate
		{ P1IE &= ~BIT3;
			if (!(P1OUT & BIT6)) {i=1; P1OUT |= BIT6;}	//on led if not on
				//calibration is beginning
				__delay_cycles(800000);
				c=0;
			for(i=0;i<1000;i++){
			c+=get_adc_0();
			__delay_cycles(1000);
			}
			lowerlim2=((unsigned int)(c/1000));

			P1OUT &= ~BIT6;
			__delay_cycles(800000);
			P1OUT |= BIT6;
			__delay_cycles(800000);
			c=0;
			for(i=0;i<1000;i++){
						c+=get_adc_0();
						__delay_cycles(1000);
						}
			upperlim2=((unsigned int)(c/1000));

			if (i) { P1OUT &= ~BIT6;}
			P1IFG &= ~BIT3;		//clear flags
			P1IE |= BIT3;
		}

		if((P1IFG & BIT1))  //if echo received
		{
			unsigned int timer=TAR;
		          if(!(P1IES & BIT1)) // is this the rising edge?
		          {
		          TACTL|=TACLR;   // clears timer A
		          miliseconds = 0;
		          P1IES |= BIT1;  //falling edge
		          }
		          else  {
		          sensor = (long)miliseconds*1000 + timer;	//calculating ECHO lenght= 'miliseconds'(in usec)+'TAR'(counter value in usec)
		          }
		    P1IFG &= ~BIT1;				//clear flag
		}

}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
  miliseconds++;
}

#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
__bic_SR_register_on_exit(CPUOFF);
}

#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCIAB0TX_ISR()
{
__bic_SR_register_on_exit(CPUOFF);
}

#pragma vector=WDT_VECTOR
__interrupt void WDT_ISR()
{
}
