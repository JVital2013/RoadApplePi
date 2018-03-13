/*
* Simple example showing how to set the Sleepy Pi to wake on button press
* and then power up the Raspberry Pi. To switch the RPi off press the button
* again. If the button is held dwon the Sleepy Pi will cut the power to the
* RPi regardless of any handshaking.
*
* This is a modified version of ButtonOnOff adding the functionality
* of detecting whether the Rpi is running or not. If it detects that 
* is has been shutdown (possibly manually by the User) then it will
* cut the power to the Rpi and go into a sleep state.
*
* Also, this version will turn on if it detects a HIGH signal on pin 14, and
* turns back off when the value returns to LOW. Pin 14 is ignored if turned on
* by the button
*/

#include "SleepyPi2.h"
#include <Time.h>
#include <LowPower.h>
#include <PCF8523.h>
#include <Wire.h>


#define kBUTTON_POWEROFF_TIME_MS2000
#define kBUTTON_FORCEOFF_TIME_MS8000

// States
typedef enum
{
	eWAIT = 0,
	eBUTTON_PRESSED,
	eBUTTON_WAIT_ON_RELEASE,
	eBUTTON_HELD,
	eBUTTON_RELEASED
}eBUTTONSTATE;

typedef enum
{
	ePI_OFF = 0,
	ePI_BOOTING,
	ePI_ON,
	ePI_SHUTTING_DOWN
}ePISTATE;

const int LED_PIN = 13;
const int DTP_PIN = 14;

volatile bool  buttonPressed = false;
eBUTTONSTATEbuttonState = eWAIT;
ePISTATE	pi_state = ePI_OFF;
bool onByButton, state = LOW;
unsigned long  time, timePress;


//Setup the Periodic Timer
//Use either eTB_SECOND or eTB_MINUTE or eTB_HOUR
eTIMER_TIMEBASE PeriodicTimer_Timebase = eTB_SECOND;// e.g. Timebase set to seconds
uint8_t PeriodicTimer_Value = 5; // Timer Interval in units of Timebase e.g 5 seconds


void button_isr()
{
	// A handler for the Button interrupt.
	buttonPressed = true;
}

void alarm_isr()
{
	// A handler for the Alarm interrupt.
}


void setup()
{
	// Don't actually shutdown
	SleepyPi.simulationMode = false;

	//Configure pins
	pinMode(LED_PIN, OUTPUT);
	pinMode(DTP_PIN, INPUT);
	digitalWrite(LED_PIN,LOW);

	SleepyPi.enablePiPower(false);  
	SleepyPi.enableExtPower(false);
	  
	// Allow wake up triggered by button press
	attachInterrupt(1, button_isr, LOW); 

	SleepyPi.rtcInit(true);
}

void loop() 
{
	bool pi_running;
	unsigned long buttonTime;

	//Enter power down state with ADC and BOD module disabled.
	//Wake up when wake button is pressed.
	//Once button is pressed stay awake - this allows the timer to keep running
	switch(buttonState)
	{
		case eWAIT:
		
			SleepyPi.rtcClearInterrupts();  
			
			attachInterrupt(0, alarm_isr, FALLING);	// Alarm pin
			SleepyPi.setTimer1(PeriodicTimer_Timebase, PeriodicTimer_Value);
			SleepyPi.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 

			if(buttonPressed == false)
			{		
				//Begin routine checks
				digitalWrite(LED_PIN,HIGH);
				pi_running = SleepyPi.checkPiStatus(false);

				//Power on pin 14 - turn on
				if(pi_running == false && digitalRead(DTP_PIN) == 1)
				{  
					SleepyPi.enablePiPower(true);
					SleepyPi.enableExtPower(true);
					pi_state = ePI_BOOTING;
					onByButton = false;  
				}
				//Power lost on pin 14 - turn off
				if(pi_running == true && digitalRead(DTP_PIN) == 0 && onByButton == false)
				{ 
					SleepyPi.enableExtPower(false);
					SleepyPi.piShutdown();
					pi_state = ePI_SHUTTING_DOWN; 
				}

				switch(pi_state)
				{			
				  case ePI_BOOTING:
					// Check if we have finished booting
					if(pi_running == true)
					{
						// We have booted up!
						pi_state = ePI_ON;				  
					}
					else 
					{
						// Still not completed booting so lets carry on waiting
						pi_state = ePI_BOOTING;				
					} 
					break;
				  case ePI_ON:
					// Check if it is still on?
					if(pi_running == false)
					{
						// Shock horror! it's not running!!
						// Assume it has been manually shutdown, so lets cut the power
						// Force Pi Off			
						SleepyPi.enablePiPower(false);
						SleepyPi.enableExtPower(false);
						pi_state = ePI_OFF;						
					}
					else 
					{
						// Still on - all's well - keep this state
						pi_state = ePI_ON;				
					} 
					break;
				  case ePI_SHUTTING_DOWN:
					  // Is it still shutting down? 
					if(pi_running == false)
					{
						// Finished a shutdown
						// Force the Power Off			
						SleepyPi.enablePiPower(false);
						SleepyPi.enableExtPower(false);
						pi_state = ePI_OFF;						
					}
					else 
					{
						// Still shutting down - keep this state
						pi_state = ePI_SHUTTING_DOWN;				
					} 
					  break;
				  case ePI_OFF:			 
				  default:
					// intentional drop thru
					// RPi off, so we'll continue to wait
					// for a button press to tell us to switch it on
					delay(10);
					pi_state = ePI_OFF;	
					break; 
				}

				// Loop back around and go to sleep again
				buttonState = eWAIT;
				digitalWrite(LED_PIN,LOW);
			
				// Disable external pin interrupt on wake up pin.
				detachInterrupt(0);
				SleepyPi.ackTimer1();				 
			}
			else
			{
				  buttonPressed = false;
				  // This was a button press so change the button state (and stay awake)
				  // Disable the alarm interrupt
				  detachInterrupt(0);			  
				  // Disable external pin interrupt on wake up pin.
				  detachInterrupt(1);
				  buttonState = eBUTTON_PRESSED; 
			}  
			break;
		case eBUTTON_PRESSED:
			buttonPressed = false;  
			timePress = millis();	// Log Press time				
			pi_running = SleepyPi.checkPiStatus(false);
			if(pi_running == false)
			{  
				// Switch on the Pi
				onByButton = true;
				SleepyPi.enablePiPower(true);
				SleepyPi.enableExtPower(true);
				pi_state = ePI_BOOTING;
			}		  
			buttonState = eBUTTON_WAIT_ON_RELEASE;
			digitalWrite(LED_PIN,HIGH);		 
			attachInterrupt(1, button_isr, HIGH);		
			break;
		case eBUTTON_WAIT_ON_RELEASE:
			if(buttonPressed == true)
			{
				detachInterrupt(1); 
				buttonPressed = false;							
				time = millis();	 //  Log release time
				buttonState = eBUTTON_RELEASED;
			}
			else
			{
				// Carry on waiting
				buttonState = eBUTTON_WAIT_ON_RELEASE;  
			}
			break;
		case eBUTTON_RELEASED:		
			pi_running = SleepyPi.checkPiStatus(false);
			if(pi_running == true)
			{
				// Check how long we have held button for
				buttonTime = time - timePress;
				if(buttonTime > kBUTTON_FORCEOFF_TIME_MS)
				{
				// Force Pi Off			
				SleepyPi.enablePiPower(false);
				SleepyPi.enableExtPower(false);
				pi_state = ePI_OFF;		  
				} 
				else if (buttonTime > kBUTTON_POWEROFF_TIME_MS)
				{
					// Start a shutdown
					pi_state = ePI_SHUTTING_DOWN; 
					SleepyPi.piShutdown();	  // true
					SleepyPi.enableExtPower(false);			
				} 
				else 
				{ 
					 // Button not held off long - Do nothing
				} 
			} 
			else 
			{
				// Pi not running  
			}
			digitalWrite(LED_PIN,LOW);			
			attachInterrupt(1, button_isr, LOW);	// button pin
			buttonState = eWAIT;		
			break;
		default:
			break;
	}
}

