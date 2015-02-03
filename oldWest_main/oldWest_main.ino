/////////////////////////////////////////////////////////////////////////////////////////
// This software is the heart of the Old West Office and fulfills many functions.
//
// First and most importantly, it randomly determines a phase of day: Sunrise, Day,
// Sunset, Night; it then determines the weather: 70% chance of clear, 30% of rain.
// Based on the phase, it displays information on an LCD, plays ambient sounds, and
// controls LED lighting.
//
// After a set amount of time (default: 25 minutes for Day/Night, 5 mins Sunrise/Set)
// the phase changes to the next phase in the order.
//
// If the weather is rainy, a relay is triggered allowing a strobe to flash for 
// lightning. This happens at random intervals.
//
// Secondly, the software monitors two motion sensors to determine if anyone enters or
// leaves the office (whether they are coming or going). Based on this information, 
// various elements are triggered. These include: sound from the outhouse,
// a knocker in the outhouse, sound from the saloon, animatronic prairie dog. The sounds
// are influenced by the phase and weather. There is a reset time between triggers where
// any motion sensor activity is ignored (default: 3 minutes). This minimizes annoyance.
//
// An animatronic vulture perched atop the outhouse is always looking around. It picks
// an interval between 4 and 8 seconds, and then chooses a random angle, then turns its
// head.
//
// The board (Arduino Mega 2560) allows for a few expansion options. The lights are 
// controlled by a separate Arduino Uno with its own code. Ambient audio is also handled
// from a separate Arduino Uno.
//
// This sketch also processes input from an Android device with Amarino installed,
// running my OldWestOffice Java app. The app allows for an override of weather, time of
// day, audio triggers, motion sensor sleep time, and sensor reset.
/////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////
// Version
///////////////////////////////////////////////////
/*
Version 10:
Known to be good (1/5/12)
Preferred version (1/5/12)
This version contains:
Time of day decision
Weather decision
LCD output
Slave: Ambient Audio
Slave: Ambient Light
Lightning relay control
PIR Input
(Untested) Button menu manipulation
Triggered audio queues to slave board
Two PIR Sensors
Vulture head servo
Amarino Overrides (+new)
*/

///////////////////////////////////////////////////
// Libraries
///////////////////////////////////////////////////
#include <LiquidCrystal.h>     // for LCD
#include <Servo.h>             // for vultureHead servo
// Audio -- v.10: this has been off-loaded onto separate board due to SPI problems
//#include "WaveHC.h"            // for Audio Shield
//#include "WaveUtil.h"          // for Audio Shield
#include <MeetAndroid.h>
// declare MeetAndroid so that you can call functions with it
MeetAndroid meetAndroid;

///////////////////////////////////////////////////
// Global Defines
///////////////////////////////////////////////////
#define PHASELENGTH 600                 // time in seconds that day/night lasts
#define TRANSLENGTH 300                 // time in seconds that sunset/sunrise lasts
#define TRIGGERTIME 180                 // time in seconds to wait between triggering events

// Audio
//#define error(msg) error_P(PSTR(msg))   // Define macro to put audio error messages in flash memory

///////////////////////////////////////////////////
// Pin definitions
///////////////////////////////////////////////////

//-------------------------
// PIR Sensors
#define pirPin 22
#define pir2pin 23
#define vultureHeadPin 7

//-------------------------
// Lightning relay
#define lightningPin 24

// --------------------------------------------------
// LCD Initialization and Custom Character definition
//  LCD pin definitions:
// GND (16)
// 47 LCD Backlight (15)
// 42 LCD Data 7 (14)
// 43 LCD Data 6 (13)
// 44 LCD Data 5 (12)
// 45 LCD Data 4 (11)
// 46 LCD Enable (6)
// 48 LCD RS (4)
// 49 LCD R/W (5)
// ressistor/potentiometer (3)
// +5 Power (2)
// GND (1)
LiquidCrystal lcd(48, 49, 46, 45, 44, 43, 42);

int backLight = 47;    // pin 51 will control the backlight

byte lcdSunUp[8] = {
 B00100,
 B01110,
 B10101,
 B00100,
 B00000,
 B01110,
 B01010,
 B01110
};

byte lcdSun[8] = {
 B00100,
 B10101,
 B01110,
 B11011,
 B01110,
 B10101,
 B00100,
 B00000
};

byte lcdSunDown[8] = {
 B01110,
 B01010,
 B01110,
 B00000,
 B00100,
 B10101,
 B01110,
 B00100
};

byte lcdMoon[8] = {
 B00000,
 B01100,
 B00110,
 B00011,
 B00011,
 B00011,
 B00110,
 B01100
};

byte lcdCactus[8] = {
 B00100,
 B00101,
 B00101,
 B10111,
 B10100,
 B11100,
 B00100,
 B00100
};

byte lcdRain[8] = {
 B01110,
 B11111,
 B11111,
 B01000,
 B01101,
 B00100,
 B10110,
 B00010
};

byte lcdRarrow[8] = {
 B00000,
 B00100,
 B00010,
 B11111,
 B00010,
 B00100,
 B00000,
 B00000
};
// ------------------------------------------

///////////////////////////////////////////////////
// Variables:
///////////////////////////////////////////////////
short randPhase;                    // for holding the random number for Phase
short randElem;                     // for holding the random number for Triggers
short randWeather;                  // for holding the random number for Weather
int currPhase;                    // the current phase (0 Sunrise, 1 Day, 2 Sunset, 3 Night)
int prevPhase;                    // the previous phase (0 Sunrise, 1 Day, 2 Sunset, 3 Night)
const char* currElem;               // the curent triggered element
int currWeather;                  // the current weather (4 Clear, 5 Rain)
int phaseSec= PHASELENGTH;          // phase is 25 mins default (1500 seconds)
int phaseSecPrint;                  // remainder of secs after minutes were deducted
int phaseMin=1234;                  // to hold remaining phase minutes. A value (1234) was given to bypass the 1st phaseShift test
int elemTriggerTime= TRIGGERTIME;   // element sleep time is 3 min default (180 sec)
int elemTriggerSec;                 // remainder of secs after minutes were deducted
int elemTriggerMin;                 // to hold remaining minutes in element sleep time
short pirCalibrationTime = 30;      // seconds given to sensor to calibrate upon startup
/*
Elem 1  = Prairie Dog
Elem 2  = sound: Outhouse Day 1
Elem 3  = sound: Outhouse Night 1
Elem 4  = sound: Outhouse Night 2
Elem 5  = sound: Outhouse Out 1
Elem 6  = sound: Saloon Day 1
Elem 7  = sound: Saloon Night 1
Elem 8  = sound: Saloon Night 2
Elem 9  = sound: Saloon Out 1
Elem 10 = sound: Saloon Out 2
Elem 11 = sound: Saloon Out 3
Elem 12 = sound: Saloon Any 1
Elem 13 = sound: Saloon Rain 1
*/
boolean pirWasTriggered = false;
short pirEnterOrLeave = 0;          // keeps track of whether the PIRs sense nothing (0) someone entering (1) or leaving (2) the office, or unknown (3)
unsigned short lningDelayTime = 3000; // set up a local variable to hold the last time we decremented one second

// Audio
//SdReader card;    // This object holds the information for the card
//FatVolume vol;    // This holds the information for the partition on the card
//FatReader root;   // This holds the information for the filesystem on the card
//FatReader f;      // This holds the information for the file we're play
//WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time
// ------------------------------------------

// ------------------------------------------
// Override Button Press variables:
int lastDebounceTime = 0;            // the last time the output pin was toggled
int debounceDelay = 50;              // the debounce time; increase if the output flickers
boolean buttonOnePressed   = false;  // holds T or F for tracking if line 1 button was pressed
boolean buttonThreePressed = false;  // holds T or F for tracking if line 3 button was pressed
boolean buttonFourPressed  = false;  // holds T or F for tracking if line 4 button was pressed
short buttonOneChoice[4][11] = {     // the enumerated value of the choice to execute for line 1:
  { 0, 1, 2, 3 },
  { 100, 101, 102, 103 },
  { 200, 201 },
  { 300, 301, 302, 303, 304, 305, 306, 307, 308, 309 }
};
/*
0   = Cancel
1   = Change Phase
2   = Change Weather
3   = Change Phase Time
100 = Sunup
101 = Day
102 = Sundown
103 = Night
200 = Clear
201 = Rainy
300 = 1m  for day/night, 20s for sunrise/set <- test mode
301 = 5m  for day/night, 2m  for sunrise/set
302 = 10m for day/night, 3m  for sunrise/set
303 = 15m for day/night, 5m  for sunrise/set
304 = 25m for day/night, 5m  for sunrise/set <- default
305 = 30m for day/night, 7m  for sunrise/set
306 = 45m for day/night, 10m for sunrise/set
307 = 60m for day/night, 15m for sunrise/set
308 = 75m for day/night, 22m for sunrise/set
309 = 90m for day/night, 30m for sunrise/set
*/
int buttonThreeChoice;               // the enumerated value of the choice to execute for line 3:
/*
0   = Cancel
100  = Prairie Dog
200  = sound: Outhouse Day 1
201  = sound: Outhouse Night 1
202  = sound: Outhouse Night 2
203  = sound: Outhouse Out 1
300  = sound: Saloon Day 1
301  = sound: Saloon Night 1
302  = sound: Saloon Night 2
303  = sound: Saloon Out 1
304  = sound: Saloon Out 2
305  = sound: Saloon Out 3
306  = sound: Saloon Any 1
307  = sound: Saloon Rain 1
*/
int buttonFourChoice;                // the enumerated value of the choice to execute for line 4:
/*
0   = Cancel
100 = 0s
101 = 30s
102 = 1m
103 = 3m <- default
104 = 5m
105 = 10m
106 = 15m
107 = 20m
108 = 30m
*/

Servo vultureHeadServo;
unsigned short vultureHeadDelay = 0;

///////////////////////////////////////////////////
//  Setup
///////////////////////////////////////////////////
void setup()
{
  Serial.begin(9600);         // Serial communication
  randomSeed(analogRead(0));  // Define random seed off of empty analog pin

  // ------------------------------------------
  // Pin Modes
  // ------------------------------------------
  pinMode(pirPin, INPUT);
  pinMode(pir2pin, INPUT);
  pinMode(lightningPin, OUTPUT);
  vultureHeadServo.attach(vultureHeadPin);
  vultureHeadServo.write(90);
  digitalWrite(pirPin, LOW);
  digitalWrite(pir2pin, LOW);
  
  // ------------------------------------------
  // Determine Phase and Weather
  // ------------------------------------------
  chooseRandPhase();

  // ------------------------------------------
  // LCD setup
  // ------------------------------------------
  lcd.createChar(0, lcdSunUp);
  lcd.createChar(1, lcdSun);
  lcd.createChar(2, lcdSunDown);
  lcd.createChar(3, lcdMoon);
  lcd.createChar(4, lcdCactus);
  lcd.createChar(5, lcdRain);
  lcd.createChar(6, lcdRarrow);

  pinMode(backLight, OUTPUT);
  digitalWrite(backLight, HIGH);  // turn backlight on. Replace 'HIGH' with 'LOW' to turn it off.
  lcd.begin(20,4);                // columns, rows.  use 16,2 for a 16x2 LCD, etc.
  lcd.clear();                    // start with a blank screen
  lcd.setCursor(0,0);             // set cursor to column 0, row 0
  //Serial.println("initializing");
  lcd.print("Initializing...");

  lcd.setCursor(0,1);       //line 2
  lcd.print("This may take some");

  lcd.setCursor(0,2);      //line 3
  lcd.print("time. Please be ");

  lcd.setCursor(0,3);      //line 4
  lcd.print("patient. Thanks! :-D");
  // ------------------------------------------

  // ------------------------------------------
  // PIR sensor calibration
  // ------------------------------------------
  Serial.print("calibrating sensors ");
  for(int i = 0; i < pirCalibrationTime; i++){
      Serial.print(".");
    delay(1000);
  }
    Serial.println(" done");
//    Serial.println("SENSOR ACTIVE");
  delay(50);
  // --------------------------------
  // Report free RAM for debugging: 
  //  putstring("Free RAM ");       // This can help with debugging, running out of RAM is bad
//  Serial.println(FreeRam());

  // ------------------------------------------
  // Amarino function declaration
  // ------------------------------------------
  meetAndroid.registerFunction(overrideWeather, 'w');
  meetAndroid.registerFunction(overrideTimeOfDay, 'p');
  meetAndroid.registerFunction(overrideTriggerTime, 't');
  meetAndroid.registerFunction(overrideTriggeredEvent, 'e');
  meetAndroid.registerFunction(resetTriggerTimer, 'r');
  meetAndroid.registerFunction(queryState, 'q');
}
// END SETUP
///////////////////////////////////////////////////


///////////////////////////////////////////////////
//  Loop
///////////////////////////////////////////////////
void loop()
{

  //--------------------------------------------------
  // Current Phase, Weather, and remaining time
  //--------------------------------------------------
  lcd.setCursor(0,0);             // line 0
  lcd.write(currPhase);           // Phase icon (rise/sun/set/moon)
  lcd.print(" ");
  lcd.write(currWeather);         // Weather icon (cactus, raincloud)
  lcd.print(" Remaining: ");
  phaseCountdown();               // Call Countdown function for remaining phase time
  lcd.setCursor(15,0);            // Set cursor to start of time
  if(phaseMin < 10){              // Pad minutes to 2 bytes if needed
    lcd.print("0");
  }
  lcd.print(phaseMin);            // Print minutes remaining
  lcd.print(":");
  if(phaseSecPrint < 10){         // Pad seconds to 2 bytes if needed
    lcd.print("0");
  }
  lcd.print(phaseSecPrint);       // Print seconds remaining

  // Set next phase at 0, set currPhase as prev phase
  if(phaseSec == 0){              // Transition to next phase at 00:00
    nextPhase();
  } else {
    prevPhase = currPhase;        // Alert ambLight that this is not a transition
  }
  
  //------------------------------------------------
  // Listen for Amarino for overrides
  //------------------------------------------------
  meetAndroid.receive();
    
  //------------------------------------------------
  // Listen for PIR, trigger elements
  //------------------------------------------------
  triggerElem();

  //--------------------------------------------------
  // Element Sleep Time
  //--------------------------------------------------
  elemCountdown();
  lcd.setCursor(0,3);          // line 3
  lcd.print("Trigger Timer: ");
  if(elemTriggerMin < 10){              // Pad minutes to 2 bytes if needed
    lcd.print("0");
  }
  lcd.print(elemTriggerMin);            // Print minutes remaining
  lcd.print(":");
  if(elemTriggerSec < 10){         // Pad seconds to 2 bytes if needed
    lcd.print("0");
  }
  lcd.print(elemTriggerSec);       // Print seconds remaining

  //-------------------
  // Lightning
  //-------------------
  if(currWeather == 5){
    lightningDisplay();
  }
  
  //-------------------
  // Vulture
  //-------------------
  moveVultureHead();
}
// END LOOP
///////////////////////////////////////////////////


///////////////////////////////////////////////////
//  Choose Phase
///////////////////////////////////////////////////
void chooseRandPhase() {
  randPhase = random(0,100);  // choose a random number from 00 to 99
  if(randPhase < 24){         // 25% chance of Sunrise
    currPhase = 0;
  }
  else if(randPhase < 49){   // 25% chance of Day
    currPhase = 1;
  }
  else if(randPhase < 74){   // 25% chance of Sunset
    currPhase = 2;
  }
  else if(randPhase < 100){  // 25% chance of Night
    currPhase = 3;
  }

  randWeather = random(0,100);  // choose a random number from 00 to 99
  if(randWeather < 70){         // 70% chance of Clear Skies
    currWeather = 4;
  }
  else {               // 30% chance of Rain
    currWeather = 5;
  }
  setPhaseWeather(currPhase, currWeather, true, false);
}


///////////////////////////////////////////////////
//  Next Phase - go to the next phase after time
///////////////////////////////////////////////////
void nextPhase() {
  prevPhase = currPhase;      // set the previous Phase to the current phase b4 change

  if(currPhase == 0){         // if it's sunrise, go to day
    currPhase = 1;
  }
  else if(currPhase == 1){  // if it's day, go to sunset
    currPhase = 2;
  }
  else if(currPhase == 2){  // if it's sunset, go to night
    currPhase = 3;
  }
  else if(currPhase == 3){  // if it's night, go to sunrise
    currPhase = 0;
  }

  randWeather = random(0,100);  // choose a random number from 00 to 99
  if(currWeather == 4){         // if it's sunny, there's a 70/30 chance of clear/rain
    if(randWeather > 69){
      currWeather = 5;
    } else {
      currWeather = 4;
    }
  }
  else if(currWeather == 5){  // if it's rainy, there's a 50/50 chance of rain
    if(randWeather > 49){
      currWeather = 4;
    } else {
      currWeather = 5;
    }
  }
  setPhaseWeather(currPhase, currWeather, false, false);
}

///////////////////////////////////////////////////
//  Set Phase and Weather
///////////////////////////////////////////////////
void setPhaseWeather(int phaseInt, int weatherInt, boolean newPhase, boolean onlyWeatherChange){
  byte tellPhase;
  byte tellWeather;
  // Convert ints into meaningful bytes
  if(phaseInt == 0){
    tellPhase = 85;
  } else if(phaseInt == 1){
    tellPhase = 68;
  } else if(phaseInt == 2){
    tellPhase = 84;
  } else if(phaseInt == 3){
    tellPhase = 78;
  }
  if(weatherInt == 4){
    tellWeather = 67;
  } else {
    tellWeather = 82;
  }

  //------------------------------------------------
  // Tell Slave:AmbientSound to play appropriate ambient sound
  //------------------------------------------------
  Serial.print(":");
  Serial.write(tellPhase);
  Serial.write(tellWeather);
  Serial.println("");
  
  if(newPhase == true){
    //------------------------------------------------
    // Tell Slave:AmbientLight to light the correct phase
    //------------------------------------------------
    Serial.print("+");
    Serial.write(tellPhase);
    Serial.write(tellWeather);
    Serial.println("");
  }

  // reset the clock to the appropriate phase length
  //  but only if the phase is new. If this is only a weather change,
  //  then leave the phase alone
  if(onlyWeatherChange == false){
    if((currPhase == 0) || (currPhase == 2)){
      phaseSec = TRANSLENGTH;    // Shorten Phase Length if sunrise/sunset
    }
    else {
      phaseSec = PHASELENGTH;
    }
  }
}

///////////////////////////////////////////////////
//  Phase Countdown Timer
///////////////////////////////////////////////////
void phaseCountdown(){
 static unsigned long lastTick = 0;            // set up a local variable to hold the last time we decremented one second
 // decrement one second every 1000 milliseconds
 if (phaseSec > 0) {
  if (millis() - lastTick >= 1000) {
    lastTick = millis();
    phaseSec--;
  }
  phaseMin = (phaseSec/60);                    // calculate remaining minutes
  phaseSecPrint = (phaseSec - (phaseMin*60));
 }
} //close phaseCountdown();


///////////////////////////////////////////////////
//  Elem Trigger Countdown Timer
///////////////////////////////////////////////////
void elemCountdown(){
 static unsigned long lastTick = 0; // set up a local variable to hold the last time we decremented one second
 if (pirWasTriggered == true){                // if the PIR sensed something, start countdown. or continue countdown until 0
  if (elemTriggerTime > 0) {
      if (millis() - lastTick >= 1000) {
          lastTick = millis();
          elemTriggerTime--;
      }
  } else {
    pirWasTriggered = false;
    elemTriggerTime = TRIGGERTIME;
  }
 }
 elemTriggerMin = (elemTriggerTime/60);   // calculate remaining minutes
 elemTriggerSec = (elemTriggerTime - (elemTriggerMin*60));
}


///////////////////////////////////////////////////
//  Trigger Random Element if PIR tripped
///////////////////////////////////////////////////
void triggerElem(){
//  Serial.print("PIR pins 1 and 2 are: ");
//  Serial.print(digitalRead(pirPin));
//  Serial.println(digitalRead(pir2pin));
  static unsigned long lastTick = 0;     // set up a local variable to hold the last time we detected motion
  if((digitalRead(pirPin) == HIGH) && (digitalRead(pir2pin) == LOW)){        // if the PIR 1 senses something before PIR 2
    pirEnterOrLeave = 2;                                                    // Someone is leaving
  } else if((digitalRead(pirPin) == LOW) && (digitalRead(pir2pin) == HIGH)){ // if the PIR 2 senses something before PIR 1
    pirEnterOrLeave = 1;                                                    // Someone is entering
  } else if((digitalRead(pirPin) == HIGH) && (digitalRead(pir2pin) == HIGH)){// if the PIR 1 and PIR 2 sense something at the same time
    pirEnterOrLeave = 3;                                                    // something was sensed, but we don't know the order
  } else {
    pirEnterOrLeave = 0;
  }
  if (pirEnterOrLeave != 0){
//    Serial.print("pirEnterOrLeave is ");
//    Serial.println(pirEnterOrLeave);
    if((elemTriggerTime == TRIGGERTIME) && (pirWasTriggered == false)){  // makes sure we wait the required time before anything is triggered
      pirWasTriggered = true;
//      Serial.println("---");
//      Serial.print("motion detected at ");
//      Serial.print(millis()/1000);
//      Serial.println(" sec");
      randElem = random(1,101);
//      Serial.print("Elem catagory chosen: ");
//      Serial.println(randElem);
      lcd.setCursor(0,2);                // line 2
      // ---------- Outhouse category ---------------------
      if(randElem <= 35){                                 // 35% chance of an Outhouse sound
        if(pirEnterOrLeave == 2){                         // if someone is detected leaving the office
          lcd.println("> Outhouse Out 1    ");            // play Outhouse Leaving sound
//          playByName("OHA01.WAV");
          Serial.print(":%a");  // this triggers a sound in the slave board
        } else if((currPhase == 1) || (currPhase == 2)){  // if not leaving, and if it's Day or Sundown
          lcd.println("> Outhouse Day 1    ");            // play Outhouse Day sound
//          playByName("OHD01.WAV");
          Serial.print(":%b");  // this triggers a sound in the slave board
        } else {                                          // if it's Night or Sunup
          randElem = random(1,3);                         // randomly select a Night sound
          if (randElem == 1){
            lcd.println("> Outhouse Night 1  ");          // play Outhouse Night1 sound
//            playByName("OHN01.WAV");
            Serial.print(":%c");  // this triggers a sound in the slave board
          } else if (randElem == 2){
            lcd.println("> Outhouse Night 2  ");          // play Outhouse Night2 sound
//            playByName("OHA01.WAV");
            Serial.print(":%a");  // this triggers a sound in the slave board
          } // end play Outhouse Night 1 or 2
        }  // end if enter or leave
        // ---------- Saloon category ------------------------
      } else if(randElem <= 85){                          // 50% chance of Saloon sound
        if(pirEnterOrLeave == 2){                         // if someone is detected leaving the office
          randElem = random(1,4);                         // randomly select a Leaving sound
          if (randElem == 1){
            lcd.println("> Saloon Out 1      ");          // play Outhouse Out1 sound
//            playByName("HLSL01.WAV");
            Serial.print(":%d");  // this triggers a sound in the slave board
          } else if (randElem == 2){
            lcd.println("> Saloon Out 2      ");          // play Outhouse Out2 sound
//            playByName("HLSL02.WAV");
            Serial.print(":%e");  // this triggers a sound in the slave board
          } else if (randElem == 3){
            lcd.println("> Saloon Out 3      ");          // play Outhouse Out2 sound
//            playByName("HLSL03.WAV");
            Serial.print(":%f");  // this triggers a sound in the slave board
          } // end play Saloon Leaving 1 2 or 3
        } else if((currPhase == 1) || (currPhase == 2)){  // if not leaving, and if it's Day or Sundown
          if(currWeather == 5){                           // if it's raining
            randElem = random(1,5);                       // determine random # for Rain sound (we don't *always* play a rain sound when raining)
            if(randElem <=2){                             // 50% chance of a Rain sound
              lcd.println("> Saloon Rain 1     ");        // play Saloon Rain1 sound
//              playByName("HLSR01.WAV");
              Serial.print(":%g");  // this triggers a sound in the slave board
            } // end 50% chance of rain
          } else {                                        // if it is not raining, and it's Day or Sundown
            randElem = random(1,7);                       // play either a Day or Any sound
            if (randElem <= 1){
              lcd.println("> Saloon Any 1      ");        // play Saloon Day1 sound
//              playByName("HLSA01.WAV");
              Serial.print(":%h");  // this triggers a sound in the slave board
            } else if (randElem == 2){
              lcd.println("> Saloon Any 2      ");        // play Saloon Any1 sound
//              playByName("HLSA02.WAV");
              Serial.print(":%i");  // this triggers a sound in the slave board
            } else if (randElem == 3){
              lcd.println("> Saloon Any 3      ");        // play Saloon Any1 sound
//              playByName("HLSA03.WAV");
              Serial.print(":%j");  // this triggers a sound in the slave board
            } else if (randElem == 4){
              lcd.println("> Saloon Day 1      ");        // play Saloon Any1 sound
//              playByName("HLSD01.WAV");
              Serial.print(":%l");  // this triggers a sound in the slave board
            } else if (randElem == 5){
              lcd.println("> Saloon Day 2      ");        // play Saloon Any1 sound
//              playByName("HLSD02.WAV");
              Serial.print(":%m");  // this triggers a sound in the slave board
            } else if (randElem == 6){
              lcd.println("> Saloon Day 3      ");        // play Saloon Any1 sound
//              playByName("HLSD03.WAV");
              Serial.print(":%n");  // this triggers a sound in the slave board
            } // end choose Day or Any
          } // end If not raining
        } else {                                          // if it's Night or Sunup
          if(currWeather == 5){                           // if it's raining
            randElem = random(1,5);                       // determine random # for Rain sound
            if(randElem <=2){                             // 50% chance of a Rain sound
              lcd.println("> Saloon Rain 1     ");        // play Saloon Rain1 sound
//              playByName("HLSR01.WAV");
              Serial.print(":%g");  // this triggers a sound in the slave board
            } // end 50% chance of rain
          } else {                                        // if it is not raining at Night or Sunup
            randElem = random(1,4);                       // randomly select a Night or Any sound
            if (randElem == 1){
              lcd.println("> Saloon Night 1    ");        // play Saloon Night1 sound
//              playByName("HLSN01.WAV");
              Serial.print(":%k");  // this triggers a sound in the slave board
            } else if (randElem == 2){
              lcd.println("> Saloon Any 3    ");        // play Saloon Night2 sound
//              playByName("HLSA03.WAV");
              Serial.print(":%j");  // this triggers a sound in the slave board
            } else if (randElem == 3){
              lcd.println("> Saloon Any 2      ");        // play Saloon Any1 sound
//              playByName("HLSA02.WAV");
              Serial.print(":%i");  // this triggers a sound in the slave board
            } // end choose Night or Any
          } // end If not raining
        } // end If leaving or day or night
      // ---------- Animatronics category ------------------
      } else if(randElem <= 100){                         // 15% chance of Animatronics
        lcd.println("> Prairie Dog       ");        // play Saloon Any1 sound
      } // end randElem category identification
      // ---------------------------------------------------
    } // end if elemTriggerTime
    lastTick = millis();
  } else if (elemTriggerTime != TRIGGERTIME){  // if elemTriggerTime is still counting down
    if (millis() - lastTick >= 15000) {
      lastTick = millis();
      // update the LCD screen every 15 seconds
      lcd.setCursor(0,1);           // line 1
      lcd.println("Awaiting Countdown  ");
      lcd.setCursor(0,2);          // line 2
      lcd.print("Manual Trigger ");
      lcd.write(6);
    }
  } else {   // if no motion is detected
    lcd.setCursor(0,1);           // line 1
    lcd.println("Awaiting Signal...  ");
    lcd.setCursor(0,2);          // line 2
    lcd.print("Manual Trigger ");
    lcd.write(6);
  }
}


//////////////////////////////////
// Lightning for rainy weather
//////////////////////////////////
void lightningDisplay(){
  static unsigned long lastTick = 0; // set up a local variable to hold the last time we decremented one second
  randomSeed(analogRead(0));  // Define random seed off of empty analog pin
  if (millis() - lastTick >= lningDelayTime) {
    lastTick = millis();
    digitalWrite(lightningPin, HIGH);
    lningDelayTime = random(1000,2000);  // ALLEN - CHANGED FOR TESTING
    delay(lningDelayTime);
    lningDelayTime = random(3000,30000);
//    Serial.print("Next lightning strike in ");
//    Serial.print(lningDelayTime);
//    Serial.println(" miliseconds.");
  } else {
    digitalWrite(lightningPin, LOW);
  }
}

/////////////////////////////////////////////////////////////
//  Move the vulture's head servo
/////////////////////////////////////////////////////////////
void moveVultureHead(void){
  static unsigned long lastTick = 0; // set up a local variable to hold the delay last time we moved the servo
  static short servoAngle = 90;
//  randomSeed(analogRead(0));         // define random seed off of an empty analog pin
  if (millis() - lastTick >= vultureHeadDelay){
    lastTick = millis();
//    Serial.print("Servo is at ");
//    Serial.println(vultureHeadServo.read());
    servoAngle = random(5,180);      // pick a random angle for the servo to go to
    vultureHeadDelay = random(4000,8000); // pick a random time between 4 and 8 seconds for the next time the vulture head moves
    vultureHeadServo.write(servoAngle);
//    Serial.print("Servo set to ");
//    Serial.println(vultureHeadServo.read());
  }
}

/////////////////////////////////////////////////////////////
//  Amarino Overrides (Time of Day, Trigger Time, Triggered Events, Weather)
/////////////////////////////////////////////////////////////
void overrideWeather(byte flag, byte numOfValues)
{
  // aquire new weather
  int length = meetAndroid.stringLength();
  char data[length];
  meetAndroid.getString(data);
  // report new weather
  //Serial.print("override weather: ");  
  //Serial.println(data[0]);
  lcd.setCursor(0,1);                // line 1
  lcd.println("Override: Weather   ");
  
  // initiate new weather
  if(data[0] == 67){
    currWeather = 4;                 // set weather to clear
  } else if(data[0] == 82){
    currWeather = 5;                 // set weather to rainy
  } else {
    Serial.print("Problem with weather override. Signal is neither C nor R");
  }
  setPhaseWeather(currPhase, currWeather, false, true);
}
void overrideTimeOfDay(byte flag, byte numOfValues)
{
  // aquire new time of day
  int length = meetAndroid.stringLength();
  char data[length];
  meetAndroid.getString(data);
  // report new time of day
  //Serial.print("override phase: ");
  //Serial.println(data[0]);
  lcd.setCursor(0,1);                // line 1
  lcd.println("Override: Phase     ");
  // initiate new time of day
  if(data[0] == 85){
    currPhase = 0;                 // set phase to sunUp
  } else if(data[0] == 68){
    currPhase = 1;                 // set phase to Day
  } else if(data[0] == 84){
    currPhase = 2;                 // set phase to sunseT
  } else if(data[0] == 78){
    currPhase = 3;                 // set phase to Night
  } else {
    Serial.print("Problem with phase override. Signal is neither U,D,T, nor N");
  }
  setPhaseWeather(currPhase, currWeather, true, false);
}
void overrideTriggerTime(byte flag, byte numOfValues)
{
  Serial.println("override: countdown");
  lcd.setCursor(0,1);                // line 1
  lcd.println("Override: Countdown ");
//  elemTriggerTime = meetAndroid.getString());
}
void overrideTriggeredEvent(byte flag, byte numOfValues)
{
  // aquire trigger event code
  int length = meetAndroid.stringLength();
  char data[length];
  meetAndroid.getString(data);
  // report new trigger code
  //Serial.print("override trigger: ");
  //Serial.print(data[0]);
  lcd.setCursor(0,1);                // line 1
  lcd.println("Override: Trigger   ");
  // initiate trigger
  char input = data[0];
  if(input == 111){
//    prairieDog();
  } else if(input == 112){
//    rattleSnake();
  } else {
    Serial.print(":%");
    Serial.print(data[0]);
  }
}
void resetTriggerTimer(byte flag, boolean state)
{
  // report reset
  //Serial.println("Resetting trigger timer");
  lcd.setCursor(0,1);                // line 1
  lcd.println("Override: Timer     ");
  // initiate reset
  elemTriggerTime = 0;
}
void queryState(byte flag, boolean state)
{
  Serial.println("Sending state to Amarino");
  meetAndroid.send(currPhase);
  meetAndroid.send(currWeather);
  meetAndroid.send(elemTriggerTime);
}



