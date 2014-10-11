/******************************************************************************
* 							alternative software for a 						  *
*							Solarstorm X2 bike flashlight					  *
*							uC Attiny44,84 and Arduino Tiny core			  *
* Author: Space Teddy														  *
*******************************************************************************/

//****************************** includes *************************************
#include <Arduino.h>
#include <TinyDebugSerial.h>
#include <avr/eeprom.h>

//******************************* defines *************************************
#define LED_100_75 				7				//Battery voltage indicator LED 100% to 75%
#define LED_75_50 				4				//Battery voltage indicator LED 75%  to 50%
#define LED_50_25 				8				//Battery voltage indicator LED 50%  to 25%
#define BUTTON					0				//control button input pin
#define MOSFET_GATE				2				//PWM output for front light dimming
#define NTC_VOLTAGE				1				//analog input pin for temperature via NTC
#define VBAT_VOLTAGE			0				//analog input pin for battery undervoltage detection

#define LIGHT_STATE_OFF			0				//various light states
#define LIGHT_STATE_ON_P1		1
#define LIGHT_STATE_ON_P2		2
#define LIGHT_STATE_ON_P3		3
#define LIGHT_STATE_ON_P4		4
#define LIGHT_STATE_SETUP		5
#define LIGHT_STATE_SETUP_P1	6
#define LIGHT_STATE_SETUP_P2	7
#define LIGHT_STATE_SETUP_P3	8
#define LIGHT_STATE_SETUP_P4	9
#define LIGHT_STATE_SETUP_EXIT	10

#define SETUP_ENTRY_TIME		3000			//3 seconds button press to enter profile setup

#define LIGHT_DEFAULT_P1		2				//profile 1-4 default values
#define LIGHT_DEFAULT_P2		64
#define LIGHT_DEFAULT_P3		128
#define LIGHT_DEFAULT_P4		255

#define ADC_RESOLUTION			48				//ADC resulution is 4.8mV
#define ADC_VBAT_DEVIDER		6				//factor of Vbat voltage devider
#define ADC_NSAMPLES			8				//count of ADC avarage samples

#define BATTERY_75				236				//75% battery limit 6,925V aka 1,154V
#define BATTERY_50				220				//50% battery limit 6,45V aka 1.075V
#define BATTERY_25				203				//25% battery limit 5,975V aka 0,996V
#define BATTERY_SHUTOFF			187				//battery undervoltage protection limit 5,5V aka 0,91666V

#define TEMP_70_HIGH			258				//assumption NTC has B=3800 and R=10k@25°C
#define TEMP_75_SHUTOFF			224

#define EEPROM_ADDRESS_PROFILE1	0x10			//profile 1-4 EEPROM adresses
#define EEPROM_ADDRESS_PROFILE2	0x11
#define EEPROM_ADDRESS_PROFILE3	0x12
#define EEPROM_ADDRESS_PROFILE4	0x13

#define VERSION 				1				//Version of X2 firmware
//************************* global variables **********************************
uint8_t light_state = 0;
uint8_t pwm = 0;								//actual pwm in use									
 
//******************************* objects *************************************
TinyDebugSerial Debug = TinyDebugSerial();		//object definition for Debug serial

//****************************** functions ************************************
//----------------------------- ADV average -----------------------------------
uint16_t analogRead_avg( uint8_t channel, uint8_t nsamples )
{
  uint32_t sum = 0;
 
  for (uint8_t i = 0; i < nsamples; ++i ) {
    sum += analogRead( channel );
  }
  //Debug.print("ADC:");Debug.println((uint16_t)( sum / nsamples ),DEC);	//ADC value Debug
  
  return (uint16_t)( sum / nsamples );
}
//-------------------------- set pwm dutycycle -------------------------------
uint8_t set_pwm_dutycycle(uint8_t pwm_value)							//set front lights with fading
{
  
  if(pwm_value == 0)
	{
		analogWrite(MOSFET_GATE,0);
		return pwm_value;
	}
  
  if(pwm_value > pwm)
	{
		while(pwm != pwm_value)
		{
			analogWrite(MOSFET_GATE,pwm++);
			delay(5);
		}
	}
	
  if(pwm_value <= pwm)
	{
		while(pwm != pwm_value)
		{
			analogWrite(MOSFET_GATE,pwm--);
			delay(5);
		}
	}
  //Debug.print("PWM:");Debug.println(pwm_value,DEC);
  return pwm_value;
}
//---------------------- setup profile pwm dutycycle --------------------------
uint8_t setup_profile_pwm_dutycycle(uint8_t profile)					//set front lights with fading
{
  uint8_t pwm_value;
  
  while(digitalRead(BUTTON) == LOW)
    {
  		analogWrite(MOSFET_GATE,pwm_value++);
		Debug.print("PWM:");Debug.println(pwm_value,DEC);
		delay(25);
		if(pwm_value > 255)
		{
			pwm_value = 0;
		}
	}
	return pwm_value;
}
//------------------------ check battery voltage ------------------------------
uint16_t get_battery_voltage(void)
{
  uint16_t battery_adc = 0;
  static uint8_t ledState = LOW;             							//ledState used to set the LED
  static uint32_t previousMillis = 0;       							//will store last time LED was updated
  const uint16_t interval = 500;           								//interval at which to blink (milliseconds)
 
  
  battery_adc = analogRead_avg(VBAT_VOLTAGE, ADC_NSAMPLES);
  //Debug.print("Vbat:");Debug.println(battery_adc,DEC);delay(5);
  
  if(battery_adc >= BATTERY_75)
	{
		digitalWrite(LED_100_75, HIGH);
		digitalWrite(LED_75_50,  HIGH);
		digitalWrite(LED_50_25,  HIGH);
		//Debug.println("Batt_100-75%");
	}

	if(battery_adc < BATTERY_75 && battery_adc >= BATTERY_50)
	{
		digitalWrite(LED_100_75, LOW);
		digitalWrite(LED_75_50,  HIGH);
		digitalWrite(LED_50_25,  HIGH);
		//Debug.println("Batt_75-50%");
	}

	if(battery_adc < BATTERY_50 && battery_adc >= BATTERY_25)
	{
		digitalWrite(LED_100_75, LOW);
		digitalWrite(LED_75_50,  LOW);
		digitalWrite(LED_50_25,  HIGH);
		//Debug.println("Batt_50-25%");
	}
	
	if(battery_adc < BATTERY_25 && battery_adc >= BATTERY_SHUTOFF)
	{
		uint32_t currentMillis = millis();
 
		if(currentMillis - previousMillis > interval) 
		{
			previousMillis = currentMillis;   						//save the last time you blinked the LED 
		
			if (ledState == LOW)									//if the LED is off turn it on and vice-versa:
			  ledState = HIGH;
			else
			  ledState = LOW;
			
			digitalWrite(LED_50_25, ledState);
			//Debug.print("Batt_<25% ");Debug.println(ledState,DEC);
		}
	}
	
	if(battery_adc < BATTERY_SHUTOFF)
	{
		pwm = set_pwm_dutycycle(0);									//battery undervoltage switch OFF front lights 
		
		for(uint8_t i=0;i<10;i++)
			{
				//Debug.println("Battery shutoff");
	
				digitalWrite(LED_100_75, HIGH);
				digitalWrite(LED_75_50,  HIGH);
				digitalWrite(LED_50_25,  HIGH);
				
				delay(500);
				
				digitalWrite(LED_100_75, LOW);
				digitalWrite(LED_75_50,  LOW);
				digitalWrite(LED_50_25,  LOW);
				
				delay(500);
			}
		while(1);									//halt
	}
  return 1;
}
//-------------------------- check temperature -------------------------------
uint16_t get_temperature(void)
{
  uint16_t temperature_adc = 0;
    
  temperature_adc = analogRead_avg(NTC_VOLTAGE, ADC_NSAMPLES);
  //Debug.print("T_ADC:");Debug.println(temperature_adc,DEC);delay(1000);	//Temperature ADC value Debug
  
  if(temperature_adc <= TEMP_70_HIGH && temperature_adc > TEMP_75_SHUTOFF)
	{
		//Debug.println("TEMP HIGH");
		pwm = set_pwm_dutycycle(10);							//Temp switch OFF front lights indication
	}
  
  if(temperature_adc <= TEMP_75_SHUTOFF)
	{
		//Debug.println("TEMP shutoff");
		for(uint8_t i=0;i<10;i++)
			{
				pwm = set_pwm_dutycycle(2);					//Temp switch OFF front lights indication
				digitalWrite(LED_100_75, HIGH);
				digitalWrite(LED_75_50,  HIGH);
				digitalWrite(LED_50_25,  HIGH);
				
				delay(500);
				
				pwm = set_pwm_dutycycle(0);					//Temp switch OFF front lights indication
				digitalWrite(LED_100_75, LOW);
				digitalWrite(LED_75_50,  LOW);
				digitalWrite(LED_50_25,  LOW);
				
				delay(500);
			}
		pwm = set_pwm_dutycycle(0);							//finally switch OFF front lights 
		delay(5000);										//wait 5 sec. for cooling down
	}
  
  return temperature_adc;
}
//----------------------- set_bike_light_profiles ------------------------------
uint8_t set_bike_light_profile(uint8_t profile, uint8_t pwm_value)
{
  if(profile == 1)
	{
		eeprom_write_byte((uint8_t*)EEPROM_ADDRESS_PROFILE1,pwm_value);		//
	}
  if(profile == 2)
	{
		eeprom_write_byte((uint8_t*)EEPROM_ADDRESS_PROFILE2,pwm_value);		//
	}
  if(profile == 3)
	{
		eeprom_write_byte((uint8_t*)EEPROM_ADDRESS_PROFILE3,pwm_value);		//
	}
  if(profile == 4)
	{
		eeprom_write_byte((uint8_t*)EEPROM_ADDRESS_PROFILE4,pwm_value);		//
	}
 
  //Debug.print("Profile");Debug.print(profile,DEC);Debug.print(":");Debug.println(pwm_value,DEC);
  
  return 1;
}
//----------------------- get_bike_light_profiles ------------------------------
uint8_t get_bike_light_profile(uint8_t profile)
{
  uint8_t profilex;
  //reads setting parameters from eeprom
	if(profile == 0)
	{
		profilex = 0;																		//OFF
	}
	if(profile == 1)
	{
		profilex = eeprom_read_byte((const uint8_t*)EEPROM_ADDRESS_PROFILE1);				//reads out saved My_addr byte
	}
	if(profile == 2)
	{
		profilex = eeprom_read_byte((const uint8_t*)EEPROM_ADDRESS_PROFILE2);				//reads out saved My_addr byte
	}
	if(profile == 3)
	{
		profilex = eeprom_read_byte((const uint8_t*)EEPROM_ADDRESS_PROFILE3);				//reads out saved My_addr byte
	}
	if(profile == 4)
	{
		profilex = eeprom_read_byte((const uint8_t*)EEPROM_ADDRESS_PROFILE4);				//reads out saved My_addr byte
	}
	
	//Debug.print("PWM:");Debug.println(profilex,DEC);
  
  return profilex;
}
//----------------------------- check default profile -----------------------------------
void check_default_profile(void)
{
  uint8_t profile_arr[5];
  
  for(uint8_t i=1;i<5;i++)
	{
		profile_arr[i] = get_bike_light_profile(i);
		//Debug.print(profile_arr[i],DEC);Debug.print(";");
	}
  //Debug.println("");
 
  if( profile_arr[1] == 255 && profile_arr[2] == 255 && profile_arr[3] == 255 && profile_arr[4] == 255)
	{
		set_bike_light_profile(1, LIGHT_DEFAULT_P1);
		set_bike_light_profile(2, LIGHT_DEFAULT_P2);
		set_bike_light_profile(3, LIGHT_DEFAULT_P3);
		set_bike_light_profile(4, LIGHT_DEFAULT_P4);
	}
}
//----------------------------- check button -----------------------------------
uint8_t check_button(void)									//set front lights with fading
{
  uint32_t time_start = 0;
  uint32_t time_stop = 0;
  
  if(digitalRead(BUTTON) == LOW)
    {
      delay(150);
	  light_state++;
	  if(light_state > LIGHT_STATE_ON_P4)
		{
			light_state = LIGHT_STATE_OFF;
			pwm = set_pwm_dutycycle(0);
		}
	  Debug.print("Light State:");Debug.println(light_state,DEC);
	  pwm = set_pwm_dutycycle(get_bike_light_profile(light_state));
	  time_start = millis();
      //Debug.print("time_start:");Debug.println(time_start,DEC);
	  
	  while(digitalRead(BUTTON) == LOW){								//checks long button press		
		  time_stop = millis();
		  //Debug.print("time_stop:");Debug.println(time_stop,DEC);
		 
		  if(time_stop > (time_start + SETUP_ENTRY_TIME) && light_state < LIGHT_STATE_SETUP)			//if button is pressed for 3 seconds then go to setup
			{
				light_state = LIGHT_STATE_SETUP;						//now you're in general profile setup
				pwm = set_pwm_dutycycle(0);
				Debug.println("Light PWM SETUP:");
				
				digitalWrite(LED_100_75, LOW);
				digitalWrite(LED_75_50,  LOW);
				digitalWrite(LED_50_25,  HIGH);
				delay(1000);
				
				for(uint8_t i=1;i<5;i++)
				{
					while(digitalRead(BUTTON) == HIGH){
						if(i==1)
						{
							digitalWrite(LED_100_75, LOW);
							digitalWrite(LED_75_50,  LOW);
							digitalWrite(LED_50_25,  HIGH);
						}
						if(i==2)
						{
							digitalWrite(LED_100_75, LOW);
							digitalWrite(LED_75_50,  HIGH);
							digitalWrite(LED_50_25,  HIGH);
						}
						if(i==3)
						{
							digitalWrite(LED_100_75, HIGH);
							digitalWrite(LED_75_50,  LOW);
							digitalWrite(LED_50_25,  HIGH);
						}
						if(i==4)
						{
							digitalWrite(LED_100_75, HIGH);
							digitalWrite(LED_75_50,  HIGH);
							digitalWrite(LED_50_25,  LOW);
						}
					}
					delay(200);
					if(digitalRead(BUTTON) == LOW )	
						{
							light_state = LIGHT_STATE_SETUP + i;
							Debug.print("Light SETUP:");Debug.println(light_state - 5,DEC);
							pwm = setup_profile_pwm_dutycycle(i);
							//Debug.print(" PWM:");Debug.println(pwm,DEC);
							set_bike_light_profile(i, pwm);
						}
					delay(500);
				}
				light_state = LIGHT_STATE_OFF;
				return light_state;
			}
		}
	}
  return light_state;
}
//******************************* setup ***************************************
void setup()
{
  pinMode(LED_100_75,OUTPUT);							//*********************************
  pinMode(LED_75_50,OUTPUT);							//*								  *
  pinMode(LED_50_25,OUTPUT);							//*								  *
  pinMode(MOSFET_GATE,OUTPUT);							//*		I/O pin configuration	  *
  pinMode(BUTTON,INPUT_PULLUP);							//*			  					  *
														//*********************************
  Debug.begin(115200);									//setup TinyDebugSerial to 115200baud
    
  analogReference(DEFAULT);								//analog reference voltage 5V

  Debug.println("X2");									//welcome message
  
  check_default_profile();								//checks if EEPROM has no profile saved
   
  Debug.print("Light State:");Debug.println(LIGHT_STATE_OFF,DEC);
  light_state = LIGHT_STATE_OFF;						//light is OFF

  while(digitalRead(BUTTON) == HIGH){					//connected to battery but in OFF mode
		get_battery_voltage();							//only temp and battery voltage is measured
		get_temperature();								//for battery energy indicator LED's
	}									

  delay(100);											//100ms button debounce delay						
}
//******************************* main loop ***********************************
void loop()
{
  check_button();
  get_battery_voltage();
  get_temperature();
}
