#include <Servo.h>
#include <EEPROM.h>

// RCArduinoYawControl
//
// RCArduinoYawControl by DuaneB is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// Based on a work at rcarduino.blogspot.com.
//
// rcarduino.blogspot.com
//
// A simple approach for reading two RC Channels from a hobby quality receiver
// and outputting to the common motor driver IC the L293D to drive a tracked vehicle
//
// We will use the Arduino to mix the channels to give car like steering using a standard two stick
// or pistol grip transmitter. The Aux channel will be used to switch and optional momentum mode on and off
//
// See related posts -
//
// Reading an RC Receiver - What does this signal look like and how do we read it - 
// http://rcarduino.blogspot.co.uk/2012/01/how-to-read-rc-receiver-with.html
//
// The Arduino library only supports two interrupts, the Arduino pinChangeInt Library supports more than 20 - 
// http://rcarduino.blogspot.co.uk/2012/03/need-more-interrupts-to-read-more.html
//
// The Arduino Servo Library supports upto 12 Servos on a single Arduino, read all about it here - 
// http://rcarduino.blogspot.co.uk/2012/01/can-i-control-more-than-x-servos-with.html
//
// The wrong and then the right way to connect servos to Arduino
// http://rcarduino.blogspot.com/2012/04/servo-problems-with-arduino-part-1.html
// http://rcarduino.blogspot.com/2012/04/servo-problems-part-2-demonstration.html
//
// Using pinChangeInt library and Servo library to read three RC Channels and drive 3 RC outputs (mix of Servos and ESCs)
// http://rcarduino.blogspot.com/2012/04/how-to-read-multiple-rc-channels-draft.html
//
// rcarduino.blogspot.com
//

// Two channels in
// Two channels out
// One program button
// One throttle intervention LED
// One Steering Intervention LED
// One Steering Sensitivity POT
// One Throttle Sensitivity POT
// One decay/damping POT

#define RC_NEUTRAL 1500
#define RC_MAX 2000
#define RC_MIN 1000
#define RC_DEADBAND 1

#define ROTATION_CENTER 380

uint16_t unSteeringMin = RC_MIN;
uint16_t unSteeringMax = RC_MAX;
uint16_t unSteeringCenter = RC_NEUTRAL;

uint16_t unThrottleMin = RC_MIN;
uint16_t unThrottleMax = RC_MAX;
uint16_t unThrottleCenter = RC_NEUTRAL;

uint16_t unRotationCenter = ROTATION_CENTER; // the gyro I am using outputs a center voltage of <> 1.24 volts 
// full range is 0 to 2*1.24 = 2.48
// Using the Arduino voltage reference of 3.3 volts gives a center point of (1.24/(3.3/1024)) = 384 
// In tests I see 380 so I will use 380 as my default value.

//////////////////////////////////////////////////////////////////
// PIN ASSIGNMENTS
//////////////////////////////////////////////////////////////////
// ANALOG PINS
//////////////////////////////////////////////////////////////////
#define THROTTLE_SENSITIVITY_PIN 0
#define STEERING_SENSITIVITY_PIN 1
#define THROTTLE_DECAY_PIN 2
#define STEERING_DECAY_PIN 3
#define GYRO_PIN 5
//////////////////////////////////////////////////////////////////
// DIGITAL PINS
//////////////////////////////////////////////////////////////////
#define PROGRAM_PIN 10
#define INFORMATION_INDICATOR_PIN 7
#define ERROR_INDICATOR_PIN 8
#define THROTTLE_IN_PIN 2
#define STEERING_IN_PIN 3
#define THROTTLE_OUT_PIN 4
#define STEERING_OUT_PIN 5

// These bit flags are set in bUpdateFlagsShared to indicate which
// channels have new signals
#define THROTTLE_FLAG 1
#define STEERING_FLAG 2

// holds the update flags defined above
volatile uint8_t bUpdateFlagsShared;

// shared variables are updated by the ISR and read by loop.
// In loop we immediatley take local copies so that the ISR can keep ownership of the
// shared ones. To access these in loop
// we first turn interrupts off with noInterrupts
// we take a copy to use in loop and the turn interrupts back on
// as quickly as possible, this ensures that we are always able to receive new signals
volatile uint16_t unThrottleInShared;
volatile uint16_t unSteeringInShared;

// These are used to record the rising edge of a pulse in the calcInput functions
// They do not need to be volatile as they are only used in the ISR. If we wanted
// to refer to these in loop and the ISR then they would need to be declared volatile
uint32_t ulThrottleStart;
uint32_t ulSteeringStart;

// used to ensure we are getting regular throttle signals
uint32_t ulLastThrottleIn;

#define MODE_FORCEPROGRAM 0
#define MODE_RUN 1
#define MODE_QUICK_PROGRAM 2
#define MODE_FULL_PROGRAM 3
#define MODE_ERROR 4

uint8_t gMode = MODE_RUN;
uint32_t ulProgramModeExitTime = 0; 

// Index into the EEPROM Storage assuming a 0 based array of uint16_t
// Data to be stored low byte, high byte
#define EEPROM_INDEX_STEERING_MIN 0
#define EEPROM_INDEX_STEERING_MAX 1
#define EEPROM_INDEX_STEERING_CENTER 2
#define EEPROM_INDEX_THROTTLE_MIN 3
#define EEPROM_INDEX_THROTTLE_MAX 4
#define EEPROM_INDEX_THROTTLE_CENTER 5
#define EEPROM_INDEX_ROTATION_CENTER 6

Servo servoThrottle;
Servo servoSteering;

uint16_t unThrottleSensitivity = 0;
uint16_t unSteeringSensitivity = 0;
uint16_t unThrottleDecay = 0;
uint16_t unSteeringDecay = 0;

void setup()
{
  Serial.begin(115200);

  pinMode(PROGRAM_PIN,INPUT);
  pinMode(INFORMATION_INDICATOR_PIN,OUTPUT);
  pinMode(ERROR_INDICATOR_PIN,OUTPUT);

  attachInterrupt(0 /* INT0 = THROTTLE_IN_PIN */,calcThrottle,CHANGE);
  attachInterrupt(1 /* INT1 = STEERING_IN_PIN */,calcSteering,CHANGE);

  readAnalogSettings();

  servoThrottle.attach(THROTTLE_OUT_PIN);
  servoSteering.attach(STEERING_OUT_PIN);

  if(false == readSettingsFromEEPROM())
  {
    Serial.println("MODE_FORCEPROGRAM");
    gMode = MODE_FORCEPROGRAM;
  }

  ulLastThrottleIn = millis();
}

void loop()
{
  // create local variables to hold a local copies of the channel inputs
  // these are declared static so that their values will be retained
  // between calls to loop.
  static uint16_t unThrottleIn;
  static uint16_t unSteeringIn;
  // local copy of rotation rate
  static uint16_t unRotation;
  // local copy of update flags
  static uint8_t bUpdateFlags;

  static uint16_t unThrottleInterventionPeak;
  static uint16_t unSteeringInterventionPeak;


  uint32_t ulMillis = millis();

  // check shared update flags to see if any channels have a new signal
  if(bUpdateFlagsShared)
  {
    uint8_t oldSREG = SREG; // Saves interrupt registers state
    cli(); // turn interrupts off quickly while we take local copies of the shared variables

      // take a local copy of which channels were updated in case we need to use this in the rest of loop
    bUpdateFlags = bUpdateFlagsShared;

    // in the current code, the shared values are always populated
    // so we could copy them without testing the flags
    // however in the future this could change, so lets
    // only copy when the flags tell us we can.

    if(bUpdateFlags & THROTTLE_FLAG)
    {
      unThrottleIn = unThrottleInShared;
    }

    if(bUpdateFlags & STEERING_FLAG)
    {
      unSteeringIn = unSteeringInShared;
    }

    // clear shared copy of updated flags as we have already taken the updates
    // we still have a local copy if we need to use it in bUpdateFlags
    bUpdateFlagsShared = 0;

    SREG = oldSREG; // we have local copies of the inputs, so now we can turn interrupts back on
    // as soon as interrupts are back on, we can no longer use the shared copies, the interrupt
    // service routines own these and could update them at any time. During the update, the
    // shared copies may contain junk. Luckily we have our local copies to work with :-)
  }

  if(digitalRead(PROGRAM_PIN) && gMode != MODE_FULL_PROGRAM)
  {
    Serial.println("MODE_QUICK_PROGRAM");

    // give 10 seconds to program
    gMode = MODE_QUICK_PROGRAM;

    // turn indicators off for QUICK PROGRAM mode
    digitalWrite(INFORMATION_INDICATOR_PIN,LOW);
    digitalWrite(ERROR_INDICATOR_PIN,LOW);

    // wait two seconds and test program pin again
    // if pin if still held low, enter full program mode
    // otherwise read sensitivity pots and return to run
    // mode with new sensitivity readings.
    delay(2000);

    if(digitalRead(PROGRAM_PIN))
    {
      Serial.println("MODE_FULL_PROGRAM");
      gMode = MODE_FULL_PROGRAM;
      ulProgramModeExitTime = ulMillis + 10000;

      unThrottleMin = RC_NEUTRAL;
      unThrottleMax = RC_NEUTRAL;
      unSteeringMin = RC_NEUTRAL;
      unSteeringMax = RC_NEUTRAL; 

      unThrottleCenter = unThrottleIn;
      unSteeringCenter = unSteeringIn;
    }
    else
    {
      gMode = MODE_RUN;  
      ulMillis = millis();    
      ulLastThrottleIn = ulMillis;
    }

    // Take new sensitivity and decay readings for quick program and full program modes here
    readAnalogSettings();
  }

  if(gMode == MODE_FULL_PROGRAM)
  {
    if(ulProgramModeExitTime < ulMillis)
    {
      // set to 0 to exit program mode
      ulProgramModeExitTime = 0;
      gMode = MODE_RUN;

      analogRead(GYRO_PIN);
      uint32_t ulTotal = 0;
      for(int nCount = 0;nCount < 50;nCount++)
      {
        ulTotal += analogRead(GYRO_PIN);
      }
      unRotationCenter = ulTotal/50;

      writeSettingsToEEPROM();

      ulLastThrottleIn = ulMillis;
    }
    else
    {
      if(unThrottleIn > unThrottleMax && unThrottleIn <= RC_MAX)
      {
        unThrottleMax = unThrottleIn;
      }
      else if(unThrottleIn < unThrottleMin && unThrottleIn >= RC_MIN)
      {
        unThrottleMin = unThrottleIn;
      }

      if(unSteeringIn > unSteeringMax && unSteeringIn <= RC_MAX)
      {
        unSteeringMax = unSteeringIn;
      }
      else if(unSteeringIn < unSteeringMin && unSteeringIn >= RC_MIN)
      {
        unSteeringMin = unSteeringIn;
      }
    }
  }
  else if(gMode == MODE_RUN)
  {
    if((ulLastThrottleIn + 500) < ulMillis)
    {
      gMode = MODE_ERROR;
    }
    else
    {
      // we are checking to see if the channel value has changed, this is indicated 
      // by the flags. For the simple pass through we don't really need this check,
      // but for a more complex project where a new signal requires significant processing
      // this allows us to only calculate new values when we have new inputs, rather than
      // on every cycle.
      if(bUpdateFlags)
      {
        unRotation = analogRead(GYRO_PIN);
      }

      if(bUpdateFlags & THROTTLE_FLAG)
      {
        ulLastThrottleIn = ulMillis;

        // A good idea would be to check the before and after value, 
        // if they are not equal we are receiving out of range signals
        // this could be an error, interference or a transmitter setting change
        // in any case its a good idea to at least flag it to the user somehow
        uint16_t unThrottleIntervention = 0;

//        if(unRotation != unRotationCenter)
//        {
//          uint32_t ulRotationWithGain = 0;
//          if(unRotation > unRotationCenter)
//          {
//            ulRotationWithGain = (long)(unRotation - unRotationCenter)*unThrottleSensitivity;
//            unThrottleIntervention = map(ulRotationWithGain,0,(long)unRotationCenter*128L,0,500); 
//          }
//          else 
//          {
//            ulRotationWithGain = (long)(unRotationCenter - unRotation)*unThrottleSensitivity;
//            unThrottleIntervention = map(ulRotationWithGain,0,(long)unRotationCenter*128L,0,500);
//          }
//        }
//
//        if(unThrottleIntervention > unThrottleInterventionPeak)
//        {
//          unThrottleInterventionPeak = unThrottleIntervention;
//        }
//        else
//        {
//          if(unThrottleInterventionPeak >= unThrottleDecay)
//          {
//            unThrottleInterventionPeak -= unThrottleDecay;
//          }
//          else
//          {
//            unThrottleInterventionPeak = 0;
//          }
//
//          if(unThrottleIntervention < unThrottleInterventionPeak)
//          {
//            unThrottleIntervention = unThrottleInterventionPeak;
//          }
//        }
//
//        if(unThrottleIn >= unThrottleCenter)
//        {
//          unThrottleIn = constrain(unThrottleIn - unThrottleIntervention,unThrottleCenter,unThrottleIn);
//        }
//        else
//        {
//          unThrottleIn = constrain(unThrottleIn + unThrottleIntervention,unThrottleMin,unThrottleCenter);
//        }

        servoThrottle.writeMicroseconds(unThrottleIn);
        
        writeErrorToSerial(unThrottleCenter, unThrottleIn, ulLastThrottleIn, 0, 0, 0);

      }
    }

    if(bUpdateFlags & STEERING_FLAG)
    {
      uint16_t unSteeringIntervention = 0;
      uint8_t bInvert = false;

//      if(unRotation != unRotationCenter)
//      {
//        uint32_t ulRotationWithGain = 0;
//
//        // note steering offers gain from 0 to 4
//        uint16_t unInterventionMaxMinusInput = 0;
//        if(unSteeringIn >= unSteeringCenter)
//        {
//          unInterventionMaxMinusInput = (unSteeringMax-unSteeringCenter) - (unSteeringIn - unSteeringCenter);
//          unInterventionMaxMinusInput = constrain(unInterventionMaxMinusInput,0,unSteeringMax-unSteeringCenter);
//        }
//        else
//        {
//          unInterventionMaxMinusInput = (unSteeringCenter - unSteeringMin) - (unSteeringCenter - unSteeringIn);
//          unInterventionMaxMinusInput = constrain(unInterventionMaxMinusInput,0,unSteeringCenter - unSteeringIn);
//        }
//
//        if(unRotation > unRotationCenter)
//        {
//          bInvert = true; 
//          ulRotationWithGain = (long)(unRotation - unRotationCenter)*unSteeringSensitivity;
//          unSteeringIntervention = map(ulRotationWithGain,0,(long)unRotationCenter*128L,0,unInterventionMaxMinusInput); 
//        }
//        else 
//        {
//          ulRotationWithGain = (long)(unRotationCenter - unRotation)*unSteeringSensitivity;
//          unSteeringIntervention = map(ulRotationWithGain,0,(long)unRotationCenter*128L,0,unInterventionMaxMinusInput);
//        }
//      }
//
//      if(bInvert)
//      {
//        unSteeringIn = constrain(unSteeringIn - unSteeringIntervention,unSteeringMin,unSteeringMax);
//      }
//      else
//      {
//        unSteeringIn = constrain(unSteeringIn + unSteeringIntervention,unSteeringMin,unSteeringMax);
//      }

      servoSteering.writeMicroseconds(unSteeringIn);
      
      writeErrorToSerial(0, 0, 0, unSteeringCenter, unSteeringIn, ulMillis);
    }
  }
  else if(gMode == MODE_ERROR)
  {
    servoThrottle.writeMicroseconds(unThrottleCenter);

    // allow steering to get to safety
    if(bUpdateFlags & STEERING_FLAG)
    {
      unSteeringIn = constrain(unSteeringIn,unSteeringMin,unSteeringMax);

      servoSteering.writeMicroseconds(unSteeringIn);
    }
  }

  bUpdateFlags = 0;

  animateIndicatorsAccordingToMode(gMode,ulMillis);
}

void animateIndicatorsAccordingToMode(uint8_t gMode,uint32_t ulMillis)
{
  static uint32_t ulLastUpdateMillis;
  static boolean bAlternate;

  if(ulMillis > (ulLastUpdateMillis + 1000))
  {
    ulLastUpdateMillis = ulMillis;
    bAlternate = (!bAlternate);
    switch(gMode)
    {
      // flash alternating info and error once a second
    case MODE_FORCEPROGRAM:
      digitalWrite(ERROR_INDICATOR_PIN,bAlternate);    
      digitalWrite(INFORMATION_INDICATOR_PIN,false == bAlternate);    
      break;
      // steady info, turn off error
    case MODE_RUN:
      digitalWrite(ERROR_INDICATOR_PIN,LOW);    
      digitalWrite(INFORMATION_INDICATOR_PIN,HIGH);
      break;
      // flash info once a second, turn off error
    case MODE_FULL_PROGRAM:
      digitalWrite(INFORMATION_INDICATOR_PIN,bAlternate);    
      digitalWrite(ERROR_INDICATOR_PIN,LOW);    
      break;
      // alternate error, turn off info
      // MODE_QUICK_PROGRAM is self contained and should never get here,
      // if it does we have an error.
    default:
    case MODE_ERROR:
      digitalWrite(INFORMATION_INDICATOR_PIN,LOW);
      digitalWrite(ERROR_INDICATOR_PIN,bAlternate);    
      break;
    }
  }
}

// simple interrupt service routine
void calcThrottle()
{
  // if the pin is high, its a rising edge of the signal pulse, so lets record its value
  if(PIND & 4)
  {
    ulThrottleStart = micros();
  }
  else
  {
    // else it must be a falling edge, so lets get the time and subtract the time of the rising edge
    // this gives use the time between the rising and falling edges i.e. the pulse duration.
    unThrottleInShared = (uint16_t)(micros() - ulThrottleStart);
    // use set the throttle flag to indicate that a new throttle signal has been received
    bUpdateFlagsShared |= THROTTLE_FLAG;
  }
}

void calcSteering()
{
  if(PIND & 8)
  {
    ulSteeringStart = micros();
  }
  else
  {
    unSteeringInShared = (uint16_t)(micros() - ulSteeringStart);
    bUpdateFlagsShared |= STEERING_FLAG;
  }
}

uint8_t readSettingsFromEEPROM()
{
  uint8_t bError = false;

  unSteeringMin = readChannelSetting(EEPROM_INDEX_STEERING_MIN);
  if(unSteeringMin < RC_MIN || unSteeringMin > RC_NEUTRAL)
  {
    unSteeringMin = RC_MIN;
    bError = true;
  }
  Serial.print("SteeringMin: ");
  Serial.println(unSteeringMin);

  unSteeringMax = readChannelSetting(EEPROM_INDEX_STEERING_MAX);
  if(unSteeringMax > RC_MAX || unSteeringMax < RC_NEUTRAL)
  {
    unSteeringMax = RC_MAX;
    bError = true;
  }
  Serial.print("SteeringMax: ");
  Serial.println(unSteeringMax);

  unSteeringCenter = readChannelSetting(EEPROM_INDEX_STEERING_CENTER);
  if(unSteeringCenter < unSteeringMin || unSteeringCenter > unSteeringMax)
  {
    unSteeringCenter = RC_NEUTRAL;
    bError = true;
  }
  Serial.print("SteeringCenter: ");
  Serial.println(unSteeringCenter);

  unThrottleMin = readChannelSetting(EEPROM_INDEX_THROTTLE_MIN);
  if(unThrottleMin < RC_MIN || unThrottleMin > RC_NEUTRAL)
  {
    unThrottleMin = RC_MIN;
    bError = true;
  }
  Serial.print("ThrottleMin: ");
  Serial.println(unThrottleMin);

  unThrottleMax = readChannelSetting(EEPROM_INDEX_THROTTLE_MAX);
  if(unThrottleMax > RC_MAX || unThrottleMax < RC_NEUTRAL)
  {
    unThrottleMax = RC_MAX;
    bError = true;
  }
  Serial.print("ThrottleMax: ");
  Serial.println(unThrottleMax);

  unThrottleCenter = readChannelSetting(EEPROM_INDEX_THROTTLE_CENTER);
  if(unThrottleCenter < unThrottleMin || unThrottleCenter > unThrottleMax)
  {
    unThrottleCenter = RC_NEUTRAL;
    bError = true;
  }
  Serial.print("ThrottleCenter: ");
  Serial.println(unThrottleCenter);

  unRotationCenter = readChannelSetting(EEPROM_INDEX_ROTATION_CENTER);
  Serial.print("RotationCenter: ");
  Serial.println(unRotationCenter);

  // ideally we would have 512 as the center
  if(unRotationCenter < 100 || unRotationCenter > 560)
  {
    unRotationCenter = ROTATION_CENTER;
    bError = true;
  }

  return (false == bError);
}

void writeSettingsToEEPROM()
{
  writeChannelSetting(EEPROM_INDEX_STEERING_MIN,unSteeringMin);
  writeChannelSetting(EEPROM_INDEX_STEERING_MAX,unSteeringMax);
  writeChannelSetting(EEPROM_INDEX_STEERING_CENTER,unSteeringCenter);
  writeChannelSetting(EEPROM_INDEX_THROTTLE_MIN,unThrottleMin);
  writeChannelSetting(EEPROM_INDEX_THROTTLE_MAX,unThrottleMax);
  writeChannelSetting(EEPROM_INDEX_THROTTLE_CENTER,unThrottleCenter);

  writeChannelSetting(EEPROM_INDEX_ROTATION_CENTER,unRotationCenter);

  Serial.print("SteeringMin: ");
  Serial.println(unSteeringMin);

  Serial.print("SteeringMax: ");
  Serial.println(unSteeringMax);

  Serial.print("SteeringCenter: ");
  Serial.println(unSteeringCenter);

  Serial.print("ThrottleMin: ");
  Serial.println(unThrottleMin);

  Serial.print("ThrottleMax: ");
  Serial.println(unThrottleMax);

  Serial.print("ThrottleCenter: ");
  Serial.println(unThrottleCenter);


  Serial.print("RotationCenter: ");
  Serial.println(unRotationCenter);
}


uint16_t readChannelSetting(uint8_t nStart)
{
  uint16_t unSetting = (EEPROM.read((nStart*sizeof(uint16_t))+1)<<8);
  unSetting += EEPROM.read(nStart*sizeof(uint16_t));

  return unSetting;
}

void writeChannelSetting(uint8_t nIndex,uint16_t unSetting)
{
  EEPROM.write(nIndex*sizeof(uint16_t),lowByte(unSetting));
  EEPROM.write((nIndex*sizeof(uint16_t))+1,highByte(unSetting));
}

/////////////////////////////////////////////////////////////////////////////
//
// The following function reads the analog settings.
// These adjustments are read at startup and anytime that the program button
// is pressed including QUICK_PROGRAM and FULL_PROGRAM 
//
// Note the 1-1023 range for sensitivity and 1-100 range for decay 1 = 500/(50*1) per sec = 10 seconds. 100 = 500/(50*100) per sec = 0.1 seconds
// 
/////////////////////////////////////////////////////////////////////////////
void readAnalogSettings()
{
  return;

  // dummy read to settle ADC
  analogRead(THROTTLE_SENSITIVITY_PIN);
  unThrottleSensitivity  = constrain(analogRead(THROTTLE_SENSITIVITY_PIN),1,1023);  

  // dummy read to settle ADC
  analogRead(STEERING_SENSITIVITY_PIN);
  unSteeringSensitivity = constrain(analogRead(STEERING_SENSITIVITY_PIN),1,1023);

  // dummy read to settle ADC
  analogRead(THROTTLE_DECAY_PIN);
  unThrottleDecay = analogRead(THROTTLE_DECAY_PIN);
  // if its at the bottom end of the range, turn it off
  if(unThrottleDecay <= 50) // instant
  {
    unThrottleDecay = 500; // = 500/1
  }
  else if(unThrottleDecay <= 100) // 2 tenths of a second
  {
    unThrottleDecay = 50; // 2 tenths = (2*(1/10))/(1/50) 
  }
  else if(unThrottleDecay <= 150) // 3 tenths
  {
    unThrottleDecay = 33; 
  }
  else if(unThrottleDecay <= 200) // 4 tenths
  {
    unThrottleDecay = 25; 
  }
  else if(unThrottleDecay <= 250) // 5 tenths
  {
    unThrottleDecay = 20; 
  }
  else if(unThrottleDecay <= 300) // 6 tenths
  {
    unThrottleDecay = 17; 
  }
  else if(unThrottleDecay <= 350) // 7 tenths
  {
    unThrottleDecay = 14; 
  }
  else if(unThrottleDecay <= 400) // 8 tenths
  {
    unThrottleDecay = 12; 
  }
  else if(unThrottleDecay <= 500) // 9 tenths
  {
    unThrottleDecay = 11;  
  }
  else if(unThrottleDecay <= 600) // 10 tenths
  {
    unThrottleDecay = 10; 
  }
  else if(unThrottleDecay <= 700) // 11 tenths
  {
    unThrottleDecay = 9; 
  }
  else if(unThrottleDecay <= 800) // 12 tenths
  {
    unThrottleDecay = 8; 
  }
  else if(unThrottleDecay <= 850) // 16 tenths
  {
    unThrottleDecay = 7; 
  }
  else if(unThrottleDecay <= 900) // 2 seconds 
  {
    unThrottleDecay = 5; 
  }
  else if(unThrottleDecay <= 1000)
  {
    unThrottleDecay = 4; 
  }
  else
  {
    unThrottleDecay = 1;
  }

  Serial.print("ThrottleDecay: ");
  Serial.println(unThrottleDecay);

  // dummy read to settle ADC
  analogRead(STEERING_DECAY_PIN);
  unSteeringDecay = constrain(analogRead(STEERING_DECAY_PIN),1,500);
}

void writeErrorToSerial(uint32_t unCh1Center, uint32_t unCh1Value, uint32_t ulCh1UpdateMillis, uint32_t unCh2Center, uint32_t unCh2Value, uint32_t ulCh2UpdateMillis)
{
  static uint32_t ulLastErrorWrite = 0;
  static uint32_t ulLastCh1Update = 0;
  static uint32_t ulLastCh2Update = 0;
  static uint32_t uCh1SqError;
  static uint32_t uCh2SqError;

  if( ulCh1UpdateMillis > ulLastCh1Update)
  {
    uCh1SqError = unCh1Center*unCh1Center + unCh1Value*unCh1Value - 2 * unCh1Center * unCh1Value; // Calculates quadratic error
    ulLastCh1Update = ulCh1UpdateMillis;
  }

  if( ulCh2UpdateMillis > ulLastCh2Update)
  {
    uCh2SqError = unCh2Center*unCh2Center + unCh2Value*unCh2Value - 2 * unCh2Center * unCh2Value; // Calculates quadratic error
    ulLastCh2Update = ulCh2UpdateMillis;
  }

  if( (ulLastCh1Update > ulLastErrorWrite) && (ulLastCh2Update > ulLastErrorWrite) )
  {
    Serial.print(uCh1SqError);
    Serial.print(",");
    Serial.println(uCh2SqError);
    
    ulLastErrorWrite = millis();
  }
  
  
}

