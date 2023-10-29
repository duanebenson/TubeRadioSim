/*
##########################################################
#
# AM (amplitude modulated) Tube radio simulator
# by Duane Benson, September, October 2023
#
# This configuration uses: 
#   EspressIF ESP32-PICO-Kit D4
#   Adafruit MAX98357A I2S class-D amplifier
#   Five Adafruit RGBW NeoPixels
#   LiPoly power supply / charger (my design)
#   Bluetooth Aruino libraries from: Tierneytim - https://github.com/tierneytim/btAudio
#   Read more in the accompanying article in Design News, by Duane Benson
#
# The intent is to use the "on/off volume" control potentiometer from the tube radio as a 
# simulated on/off control and volume control for this device. The NeoPixels will simulated
# the warm up time and color of the old radio's pilot lamps and vacumm tubes. It utilizes the
# Bluetooth audio capabilities of the ESP32-PICO-Kit D4.
#
# In operation, turning the radio will start the "warm up" sequence as would be seen in analog
# authentic tube radio. It will then act as a Bluetooth speaker connected to your audio
# source of choice. The audio will start to come up about halfway through the tube warm up.
#
# The General Eletric F-65 rabio in the example uses six vacuume tubes. However, three are in
# metal envelopes, so only three filaments are visible. To simulate additional tubes, just add
# more NeoPixels, duplicate the "tubeFilament" code lines and and adjust the pixel numbers
# accordingly. he two pilot lamps shouls be the last two NeoPixels in the string.
#
# To connect this into a radio, desolder the five wires from the potentiometer/switch in the radio
# and wire up the switch to ground in the ESP32 system and pin 3y. Connect the outside solder
# lugs of your potentiometer to ground and power in the ESP32 system. Connect the potentiometer
# wiper to pin 38. You may use other pins, as appropriate. Just make sure to change the declarations
# in the "Hardware pin declarations" below. The two outside pins on the potentiometer should go to
# power and ground. If theradio comes up at full volume and gets quietr as you "increase" the volume,
# you will need to reverse power and ground to the potentiometer outside pins.
#
# Diagnostic "print" functions will only show if you open a terminal in the Arduione IDE.
#
##########################################################
*/

//#############################
//# External libraries
//#############################
#include <Adafruit_NeoPixel.h>    // Adafruit NeoPixel libriaries
#include <btAudio.h>              // From: https://github.com/tierneytim/btAudio


//#############################
//# Hardware pin declarations
//#############################
#define pNeoPin  19                 // Pin for NeoPixel data
#define pF65s3   37                 // Simulated hard power switch. Goes to F-65 radio switch S3, SPST on switched volume pot
#define pF65r5   38                 // Volume control. Goes to F-65 radio potentiometer R5, main volume control
#define pI2Sdin  25                 // Connects to DIN/DOUT on Adafruit MAX98357A
#define pI2Sbclk 26                 // Connects to BCLK on Adafruit MAX98357A
#define pI2Slrc  27                 // Connects to LRC/WSEL on Adafruit MAX98357A

//#############################
//# Constant declarations
//#############################
#define NeoCount 5                  // Total number of NeoPixels used
#define pilotInc 5                  // Each warm up step increses by this number for the pilot lamps
#define tubeInc  1                  // Each warm up step increses by this number for the tube filaments
#define filamentMax 175

// RGBW maximum values for pilot lamps. These values give a bright white with a slight yellow tint
#define pilotMaxR 126
#define pilotMaxG 126
#define pilotMaxB 64
#define pilotMaxW 255

// RGBW maximum values for tube filaments. These values will give a warm orange glow
#define tubeMaxR 64
#define tubeMaxG 255
#define tubeMaxB 0
#define tubeMaxW 0

//#############################
//# Global variable declarations
//#############################
int powerSwitch = 0;              // Power switch, off by default
float volumeControl = 0;          // Volume, zero by default. Will be normalized to a float value between zero and 1
int tubesWarm = 0;                // Indicates state of tube filaments. 0 = cold and off. 1 = fully warm and glowing
int pilotWarm = 0;                // Indicates state of pilot lamps. 0 = cold and off. 1 = fully on

// Values used to set the RGBW NeoPixesl as they are "warming up"
int pilotR = 0;
int pilotG = 0;
int pilotB = 0;
int pilotW = 0;
int tubeR = 0;
int tubeG = 0;
int tubeB = 0;
int tubeW = 0;

//#############################
//# Object declarations
//#############################
Adafruit_NeoPixel filament = Adafruit_NeoPixel(NeoCount, pNeoPin, NEO_RGBW + NEO_KHZ800); // Create NeoPixel filament object
btAudio audio = btAudio("ESP_Speaker"); // Create Bluetooth audio object


//#############################
//#############################
// the setup function runs once when you press reset or power the board
//#############################
//#############################
void setup() {
  pinMode(pF65s3, INPUT);         // Set Power switch GPIO to input, with internal pull up resistor

  filament.begin();               // Start NeoPixel "filament" object
  filament.clear();               // Turn NeoPixels all off
  filament.show();                // Display the current NeoPixel values (all off at this point)
  filament.setBrightness(filamentMax);    // Set NeoPixel overall

  audio.begin();                  // Start up audio
  audio.reconnect();              // Automatically reconnect with the last connected BlueTooth device
  audio.I2S(pI2Sbclk, pI2Sdin, pI2Slrc);  // Connect audio object to I2S output digital amplifier
  audio.volume(0);                // Start with I2S amp at zero volume
}

//#############################
//#############################
// the main loop
//#############################
//#############################
void loop() {

  // Wait for power switch to be turned on
  powerSwitch = digitalRead(pF65s3);      // Power switch is tied high when off, tied to ground when on
  while (powerSwitch == 1) {      // Wait in loop while power is turned off
    powerSwitch = digitalRead(pF65s3);
    filament.clear();             // Make sure NeoPixels are off
    filament.show();
	  if (tubesWarm != 0) {	        // If filaments are on, turn them off. Do this when the radio had been on but was just turned off
	    tubesWarm = coolDown(10);
	    volumeControl = 0;
	  }
  }
 
  if (tubesWarm == 0) {           // If out of the power-on wait and tubes are off, them warm them up
    volumeControl = analogRead(pF65r5);
    volumeControl = volumeControl / 4096;
    pilotWarm = pilotOn();
    tubesWarm = warmUp(100);
  }

  volumeControl = analogRead(pF65r5);   // Continuously read volume
  volumeControl = volumeControl / 4096; // Arduino analog read gives value between 0 and 4096. Convert to 0 to 1
  audio.volume(volumeControl);    // Set volume in BlueTooth module

  delay(10);
}


//#############################
//# Function definitions and code
//#############################


// Simulate warm up time for pilot lamps
int pilotOn() {
  int pilotIncR = 2; // 126
  int pilotIncG = 2; // 126
  int pilotIncB = 1; // 64
  int pilotIncW = 4; // 255

	for (int i = 0; i <= 59; i++) {
    if (pilotR < pilotMaxR) {pilotR =  pilotR + pilotIncR;}
    if (pilotW < pilotMaxG) {pilotG =  pilotG + pilotIncG;}
    if (pilotW < pilotMaxW) {pilotB =  pilotB + pilotIncB;}
    if (pilotW < pilotMaxW) {pilotW =  pilotW + pilotIncW;}

    filament.setPixelColor(3, pilotR, pilotG, pilotB, pilotW);
    filament.setPixelColor(4, pilotR, pilotG, pilotB, pilotW);

    delay(20);
    filament.show();
	}

  return 1;	// Return value of 1 indicates that pilots are now warmed up
}


// Simulate warm up time for pilot lamp and tube filaments
int warmUp(int milliSecWarm) {
  float soundUp = 0;
  int tubeIncR = 1; // 64
  int tubeIncG = 4; // 255

	for (int i = 0; i <= 59; i++) {
    if (tubeR < tubeMaxR) {tubeR =  tubeR + tubeIncR;} else {tubeR = tubeMaxR;}
    if (tubeG < tubeMaxG) {tubeG =  tubeG + tubeIncG;} else {tubeG = tubeMaxG;}

    filament.setPixelColor(0, tubeR, tubeG, tubeB, tubeW);		// Tube filament
    filament.setPixelColor(1, tubeR, tubeG, tubeB, tubeW);		// Tube filament
    filament.setPixelColor(2, tubeR, tubeG, tubeB, tubeW);		// Tube filament
    // To add more tube filaments, duplicate these lines.
    // "X" is the NelPixel number in the string. Set X accordingly
    // Don't forget to chnage the pilot NeoPixel numbers in the function above 
    // from 3 and 4 to be the last two numbers in your NeoPixel string
//  filament.setPixelColor(X, tubeR, tubeG, tubeB, tubeW);		// Tube filament

    volumeControl = analogRead(pF65r5);
    volumeControl = volumeControl / 4096;

// Halfway through the tube warmup, bring up the audio volume
    if (i >= 25) {
      if (soundUp < volumeControl) {
        soundUp = soundUp + 0.0075;
      } else {
        soundUp = volumeControl;
      }
      audio.volume(soundUp);
    }
    filament.show();
    if (i < 15) { delay((2 * milliSecWarm)); } else { delay(milliSecWarm); }
	}
  audio.volume(volumeControl);
  return 1;	// Return value of 1 indicates that tubes are now warmed up
}


// Turn off pilot and tube filaments and volume
int coolDown (int milliSecCool) {
  filament.clear();
  filament.show();
  audio.volume(0);
  volumeControl = 0;
  pilotR = 0;
  pilotG = 0;
  pilotB = 0;
  pilotW = 0;
  tubeR = 0;
  tubeG = 0;
  tubeB = 0;
  tubeW = 0;
	return 0;	// Return a value of 0 that indicates that all filaments are off
}