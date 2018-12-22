#include <msp430.h> 
#include <msp430g2553.h>
/* Coded by Ian Moffitt and Joe McNatt
 * Presented October 18th 2018
 * Board used: MSP430G2553
 *
 * main.c
 *
 * Receive the package at the receive port ()
 * First Byte is how many bytes are in the package
 * next 3 bytes detail how the the LED operates (what pulse width modulation for each color of the LED)
 * The Rest of the package is sent out through a transmission port
 *
 * Steps to make this work
 * 1.) Initialize the port to receive data
 * 2.) Initialize the port to transmit data
 * 3.) Read how many bytes are seen and how many bytes you need to transfer (Set a clock interrupt
 * 4.) Receive each byte and save it to a register during a clock cycle
 * 5.) Send the data to the output register
 *
 * Use secondary function for bits that are connected to the timer.
*/
//Some global variables
volatile signed int byte = 0; //Current byte being read by UART
int bytesRemaining;           //How many bytes need to be transmitted after the initial 3 bytes are read
//Some define statements to be able to read the code a little easier
#define Recieve BIT1
#define Transmit BIT2
#define RedLED BIT6     //Port 1.6
#define GreenLED BIT1   //Port 2.1
#define FanOut BIT4    //Port 2.4
#define RedTimer TA0CCR1
#define GreenTimer TA1CCR1
#define FanTimer TA1CCR2
#define currentByte UCA0RXBUF
#define transferByte UCA0TXBUF
#define storms 0xFF
#define sun 0x11
//The main function is going to simply initialize the Pins and UART
int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    /* First is to set up the Port for the LEDs
     * First set Port 1 Select, this will configure the functions of everything
     * Sets ports where Tx and Rx are as secondary function (UART terminal)
     * Port 1 direction is output when 1, all the Macros for LED are 1 at the location of the LED
     * The secondary function is selected for the timer modules
     * Then they are all designated as output
     * Initialize all of the values at High Voltage
    */
    P1SEL = Recieve + Transmit;
    P1SEL |= RedLED;
    P1SEL2 = Recieve + Transmit;
    P2SEL = FanOut + GreenLED;
    P1DIR = RedLED + Transmit;
    P2DIR = FanOut + GreenLED;
    P1OUT = 0;
    P2OUT = 0;
    /* Here the Timer is set up
     * The max value for any RGB is 255, so CCR0 is set to that as it simplifies setting the PWM
     * The control register is sourced from Small Clock, and in Up mode as to have the clock reset to 0 every time CCR0 is reached
     * The CCR values correlate to each individual LED
     * CCR1 is the Red LED
     * CCR2 is the Green LED
     * TA1CCR1 is the Blue LED
     * All three CCTL Registers are set to Reset/Set mode, so that the
     * All 3 CCRs are set to 255 as to be off (Duty cycle is 0%)
     * Since the MSP430G2 only has 2 capture control register we needed to use a second timer A1
    */
    TA0CTL = TASSEL_2 + MC_1 + TACLR;
    TA1CTL = TASSEL_2 + MC_1 + TACLR;
    TA0CCTL1 = OUTMOD_3;
    TA0CCTL2 = OUTMOD_3;
    TA1CCTL2 = OUTMOD_3;
    TA1CCTL1 = OUTMOD_3;
    TA0CCR0 = 255;
    TA1CCR0 = 255;
    RedTimer = 255;
    GreenTimer = 255;
    FanTimer = 255;
    /* This section of code configures the UART
     * UCA0CTL is the control register for the UART control, which is set to be controlled by the Small Clock
     * UCA0MCTL is the modulation control register which is set to be 5
    */
    UCA0CTL1 = UCSWRST;
    DCOCTL = 0;
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;
    UCA0CTL1 |= UCSSEL_2;
    UCA0BR0 = 104;
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS0 + UCBRS2;
    UCA0CTL1 &= ~UCSWRST;
    UC0IE |= UCA0RXIE;
    //At the end of initializing enter Low Power mode and enable global interrupts
    __bis_SR_register(GIE);
    while(1)
    {}
//    __bis_SR_register(LPM0_bits + GIE);
    return 0;
}
/* UART Interrupt vector is what trips the entire cycle
 * The interrupt triggers when a full byte is stored is the UCA0RXBUF
 * Since the output mode is set to toggle reset for the capture compare registers, once the value is
 * reached in CCRX the output is instantly toggled, and then when CCR0 is reached it sets it to 0
 * Byte is the current byte number the UART is reading, starts at 0
 * currentByte is macro'd to the UCA0RXBUF register which stores a full byte of data at a time
 * Case 0 is when the first byte of data comes through which details how many bytes need to be transfered
 * Case 1 is the value of the byte which details the pwm of the RedLED
 * Case 2 is the value of the byte which details the pwm of the GreenLED
 * Case 3 is the value of the byte which details the pwm of the BlueLED
 * Case 3 also transfers the first byte of data which is the size of the rest package
 * The default case occurs after the first 4 bytes are read
 * After the switch case occurs the byte value is incremented by 1 to read the next byte
 * After the entire switch case occurs, the LED will remain on using the capture compare mode
*/
#pragma vector = USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
    int song = currentByte;
    if(song == 0xFF){
        RedTimer = 255;
        GreenTimer = 255;
        FanTimer = 0;
    }
    else if ((song == 0x11)||(song == 0x91)){
        RedTimer = 0;
        GreenTimer = 0;
        FanTimer = 255;
    }
}
