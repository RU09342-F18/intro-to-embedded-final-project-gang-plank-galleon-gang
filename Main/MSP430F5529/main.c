#include <msp430.h> 
/**
 * main.c
 *
 * certain discrete integer values coordinate to certain notes being played
 *
 * music for songs
 * Song of Storms: a down up a down up (D - F - D) (D - F - D)
 * Suns Song: right down up right down up (E - C - A) (E - C - A)
 *
 * For logic testing purposes the notes being played correlate to specific Button Inputs
 * all of it is Port1 bits 0-4
 *
 * Ocarina is in the 5th Octave */

//The notes are powers of 2 as the port bits are read as binary powers of 2, simplifies code
#define upNote 4
#define downNote 8
#define leftNote 16
#define rightNote 32
#define ANote 64
#define noNote 0

//For the logic testing the song with buttons
#define upNoteIn (P1IN & BIT2)
#define downNoteIn (P1IN & BIT3)
#define leftNoteIn (P1IN & BIT4)
#define rightNoteIn (P1IN & BIT5)
#define ANoteIn (P1IN & BIT6)

//This button will start the system
#define startPolling (P2IN & BIT1)

//Defines for the
#define transferByte UCA0TXBUF
#define songOfStorms 0xFF
#define sunsSong 0x11

//These registers control the RGB value of the LED
#define redValue TA1CCR1    //Port 2.0
#define greenValue TA2CCR1  //Port 2.4
#define blueValue TA2CCR2   //Port 2.5

unsigned int SongOfStorms[6];
unsigned int SunSong[6];
unsigned int currentSong[6];

int currentNote = 0;
int readInput = 0;
int advance = 0;
unsigned int missedNotes = 0;
unsigned int isSongSoS = 0;
unsigned int isSongSS = 0;
int averageFrequency = 0;
float currentAmplitude = 0;
float pastAmplitude = 0;
float dAmpl = 0;

int ADCclk;

/* This function loads the data for all of the predefined songs */
void InstantiateSongs()
{
    currentSong[0] = noNote;
    currentSong[1] = noNote;
    currentSong[2] = noNote;
    currentSong[3] = noNote;
    currentSong[4] = noNote;
    currentSong[5] = noNote;

    SongOfStorms[0] = ANote;
    SongOfStorms[1] = downNote;
    SongOfStorms[2] = upNote;
    SongOfStorms[3] = ANote;
    SongOfStorms[4] = downNote;
    SongOfStorms[5] = upNote;

    SunSong[0] = rightNote;
    SunSong[1] = downNote;
    SunSong[2] = upNote;
    SunSong[3] = rightNote;
    SunSong[4] = downNote;
    SunSong[5] = upNote;
}

/* Tests each of the predefined songs to the current song loaded by incrementing through the array
 * and comparing each value of the predefined song to the current song loaded in the array */
void CompareSongs()
{
    isSongSoS = 1;
    isSongSS = 1;

    int testNote;
    for(testNote = 0; (testNote <= 5) && ((isSongSoS || isSongSS) == 1); testNote++)
    {
        if ((SongOfStorms[testNote] != currentSong[testNote]) || (isSongSoS == 0))
            isSongSoS = 0;
        if ((SunSong[testNote] != currentSong[testNote]) || (isSongSS == 0))
            isSongSS = 0;
    }
}

/*This function increments through the array which holds the current song
 * and clears all of previously stored notes and instead stores
 * the default "noNote" value of 0 */
void ClearCurrentSong()
{
    int i;
    for (i = 0; i <= 7; i++)
    {
        currentSong[i] = noNote;
    }
}

/* This function simply configures the Ports in the MSP430F559 which acts as the decoder */
void ConfigurePorts()
{
    P1SEL = 0x00;               //Port 1 buttons are all IO and in port 1
    P3SEL = BIT3;               //Secondary Function for UART Transmission register
    P2SEL = BIT0 | BIT5 | BIT4; //PWM Output for the RGB
    P1DIR = 0x00;   //Port1 pins 0 to 5 are all input

    /*TA1.1 TA2.2 TA2.1*/
    /*P2.0  P2.5  P2.4 */
}

/* Configures the sampling timer for the analog voltages along with the PWM
 * The timer for the RGB LED
 * The time for sampling to occur
 * The time for how long sampling can be*/
void ConfigureTimer()
{
    //Configure timer A1 and A2 to do the PWM
    TA1CTL = TASSEL_2 + MC_1 + TACLR;
    TA2CTL = TASSEL_2 + MC_1 + TACLR;
    TA1CCTL1 = OUTMOD_3;
    TA2CCTL1 = OUTMOD_3;
    TA2CCTL2 = OUTMOD_3;
    TA1CCR0 = 255;  //This is the max value the clock will count to
    TA2CCR0 = 255;  //This is the max value the clock will count to

    //Configure TimerA0 to run for one and a half seconds
    //AClk 32768 * 1.5 48
    TA0CTL = TASSEL_1 + MC_1 + ID_0;//Control Registers;
    TA0CCR0 = 49152;
    TA0CCR1 = 49151;
}

/* This functions configures the UART modules to be compatible
 * with the UART on the other board*
void ConfigureUART()
{
    UCA0CTL1 = ~USCWRST;
    //Configure a parity bit, with odd parity, send MSB first, 8 bits of data, one stop bit, in UART mode, Asynchronous
    UCA0CTL0 = UCPEN + UCPAR + UCMSB;
    UCA0CTL1 = UCSSEL_1 + UCBRKIE + USCWRST;
}
*/
//Turn the RGB LED Green
void LEDGreenOn()
{
    redValue = 0;
    greenValue = 255;
    blueValue = 0;
}

//Turn the RGB LED Red
void LEDRedOn()
{
    redValue = 255;
    greenValue = 0;
    blueValue = 0;
}

//Turn the RGB LED Blue
void LEDBlueOn()
{
    redValue = 0;
    greenValue = 0;
    blueValue = 255;
}

//Use the RGB LED and turn it off
void LEDOff()
{
    redValue = 0;
    greenValue = 0;
    blueValue = 0;
}

//Will flash the RGB LED red
void LEDRedFlash(int flashes)
{
    int flashNum;   //C is stupid and can't declare variables in a for loop
    for (flashNum = 0; flashNum < flashes; flashNum++)
    {
        LEDRedOn();                 //Turn the RGB LED red
        __delay_cycles(150000);     //Delay the system by about a tenth a second
        LEDOff();                   //Turn the LED off
        __delay_cycles(150000);     //Delay the system again
    }

}

//Will flash the RGB LED blue
void LEDBlueFlash(int flashes)
{
    int flashBNum;
    for (flashBNum = 0; flashBNum < flashes; flashBNum++)   //Run this loop flashes times
    {
        LEDBlueOn();                 //Turn the RGB LED red
        __delay_cycles(150000);     //Delay the system by about a tenth a second
        LEDOff();                   //Turn the LED off
        __delay_cycles(150000);     //Delay the system again
    }

}

/* This function reads the current input of the buttons, should stall the */
void SampleForNote(int currentNote)
{
    P1IE = 0x1F;    //This enables the interrupt on the buttons which will allow the value of the
    TA0CTL |= TACLR + TAIE;  //This ensures the timer value is at 0 when the sequence starts
    //Enable the timer interrupt vector;
    while(advance != 1) {}   //This code ensures that the code will stall until the value of TB1CCR0 is reached
    advance = 0;    //reset the advance variable to 0
    P1IE = 0x00;    //disables the interrupt on the input buttons
    TA0CTL &= ~(TACLR | TAIE);  //disables the interrupt on the
}

/* Will convert the frequency read by the adc to a note, then save to an array
 * While in button mode this will simply save the note read to the currentSong array */
void DecodeFrequency()
{

    currentSong[currentNote] = readInput;

    /* This is the code for the analog frequencies read */
    /*
     switch(averageFrequency)
     {
         case(580 <= averageFreq <= 610)
             currentSong[currentNote] = ANote;
         break;

         case(695 <= averageFreq <= 710)
             currentSong[currentNote] = downNote;
         break;

         case(1055 <= averageFreq <= 1075)
             currentSong[currentNote] = upNote;
         break;

         default:
             currentSong[currentNote] = noNote;
     }
     */
}

/* Will send a predeterming package to the */
void SendSong()
{
    if(isSongSoS)
        transferByte = songOfStorms;
    if(isSongSS)
        transferByte = sunsSong;
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer

    InstantiateSongs(); //Loads the songs to compare to in arrays
    ConfigurePorts();   //Configures secondary port functions and IOs
    ConfigureTimer();   //Configures the timer for sampling and the PWM of the RGB LED
    //ConfigureUART();    //Configures UART to send the data to the other board

    /* This is the infinite recursive loop that will occur after the registers were instantiated
     * Have start polling be a button that will start the entire sequence, possibly in the same
     * port as buttons */
    while(1)
    {
        //Once a start button is pressed, begin the script to record a song
        if(startPolling == 1)
        {
            //Here the polling process will begin
            int currentNote = 0;
            for(currentNote = 0; currentNote <= 5; currentNote++)
            //while(currentNote <= 5)
            {
                LEDGreenOn();       //Turn the LED green to let the user know it wants an input
                SampleForNote(currentNote);    //After the button is pressed, the code will sample for a note over the course of one section
                LEDOff();           //After the note is sampled turn off the LED to let the user know they don't want an input
                DecodeFrequency();  //After an average frequency is recorded, decode if that is a note, and save to array
                if(currentSong[currentNote] != noNote)  //If a note was played correctly then the fail safe for missing a note is set to 0
                {
                    missedNotes = 0;    //If a note was played correctly then the fail safe for missing a note is set to 0
                }
                else    //This script will run if there was no note detected
                {
                    LEDRedFlash(3);                     //Use a red LED to show the user they missed an input, the number is how many times
                    currentSong[currentNote] = noNote;  //Assign noNote to the current song, may not be necessary
                    currentNote = currentNote - 1;      //Decrement the current note to give the user another chance to input correct
                    missedNotes++;                      //add one to the missed notes fail safe
                    if (missedNotes == 2)
                    {
                        LEDRedFlash(5);     //If the player missed two notes, the red LED will flash 5 times
                        ClearCurrentSong(); //This clears the current song loaded
                        currentNote = 0;    //The for loop resets allowing the user to attempt to record another song
                    }
                }
            }

            //Once the for loop has finished running, it should have stored 6 notes in the currentSong Array
            LEDBlueOn();    //Turn the LED to blue to let the user know they completed a song
            CompareSongs(); //Once it is complete compare the song to the pre-loaded songs
            if((isSongSoS || isSongSS) == 1)
            {
                LEDBlueFlash(6);    //Flash the LED blue if a song is seen 6 times
                SendSong();         //If the song is seen to be one of the preloaded songs, send a signal of which song over UART
            }
            else
            {
                LEDRedFlash(6);     //If a song isn't read flash the LED red 6 times
            }
            ClearCurrentSong();
        }
    }

    //The loop will never reach the end
    return 0;
}

/* This interrupt will occur exclusively when the sample song function is enabled*/
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A(void)
{
    advance = 1;
}

//This is the interrupt that occurs when the
#pragma vector = PORT1_VECTOR
__interrupt void PORT1(void)
{
    readInput = P1IN & 0x1F;
}

//The RGB LED will trigger through hardware PWM so we chillin on a function for that

/*
#pragma vector = ADC12_VECTOR
__interrupt void ADC12(void)
{
    currentAmplitude = ADC12MEM;   //Check the current value of the adc
    dAmplCurrent = currentAmplitude - pastAmplitude;   //find the rise in amplitude

    //if the rise is positive, doesn't matter, if its negative set a timestamp and find the current frequency
    if (dAmplCurrent < 0)
    {
        if (dAmplPast > 0)
        {
            timeOfPeak2 = TA0R;                             //Save a timestamp of when the peak voltage occured
            delTime = timeOfPeak2 - timeOfPeak1;            //Calculate the difference in the time of the two peaks, this 1/2 a period

            frequencyCurrent = fclk/(2(delTime))    //This calculates the frequency of the current sine wave
            averageFreq = (frequencyCurrent + frequencyPast)/2; //calculates the current average frequency
            frequencyPast = frequencyCurrent;   //Saving current variables to past variables
            timeOfPeak1 = timeOfPeak2
        }
    }
    dAmplPast = dAmplCurrent;
}
*/

