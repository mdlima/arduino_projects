#include <AQMath.h>
#include <GlobalDefined.h>
#include <Receiver_328p.h>

/*

  RC 2WD Car 
  v0.1 - August 2012
  Marcio Lima
  
  This is an R/C controller for the 2WD Arduino Kit http://dx.com/p/2wd-arduino-ultrasonic-smart-car-kits-128971
  
  This code uses the AQ_Receiver library from AeroQuad (www.aeroquad.com) 
  and is based on code available on the RC Arduino blog by DuaneB (rcarduino.blogspot.com).


*/

#define RECEIVER_CHANNELS 2 // How many channels will be read

// AQ_Receiver Library use the folloqing pins
// static byte receiverPin[6] = {2, 5, 6, 4, 7, 8}; // pins used for XAXIS, YAXIS, ZAXIS, THROTTLE, MODE, AUX

unsigned int unChCenter[RECEIVER_CHANNELS]; // Used to calibrate radio signals


// L298 Controller Pins
// Motor A is the left motor and Motor B is the right motor
#define ENA A // Drive/enable motor A (PWM)
#define IN1 A1 // Direction 1 for motor A
#define IN2 A2 // Direction 2 for motor A
#define IN3 A3 // Direction 1 for motor B
#define IN4 A4 // Direction 2 for motor B
#define ENB A5 // Drive/enable motor B (PWM)


unsigned long timer;

void setup() {
  
  Serial.begin(115200);
  Serial.println("Receiver library test (Receiver_APM)");
  
  // Setup L298 Motor Controller
  pinMode(ENA,OUTPUT);//output
  pinMode(ENB,OUTPUT);
  pinMode(IN1,OUTPUT);
  pinMode(IN2,OUTPUT);
  pinMode(IN3,OUTPUT);
  pinMode(IN4,OUTPUT);
  digitalWrite(ENA,LOW);
  digitalWrite(ENB,LOW);
  digitalWrite(IN1,LOW);
  digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW);
  digitalWrite(IN4,LOW);
  
  initializeReceiver(RECEIVER_CHANNELS);
  
  delay(1000); // Wait for 1 second before reading neutral positions
  
  readReceiver(); // Read commands from Receiver Library
  
  // Store neutral positions
  for( int i=0; i < RECEIVER_CHANNELS; i++ )
  {
    unChCenter[i] = receiverCommand[i];
  }
}

void loop() {
  
  if((millis() - timer) > 50) // 20Hz
  {
    timer = millis();
    readReceiver();
    
    // Serial.print(" Roll: ");
    // Serial.print(receiverCommand[XAXIS]);
    // Serial.print(" Pitch: ");
    // Serial.print(receiverCommand[YAXIS]);
    // Serial.print(" Yaw: ");
    // Serial.print(receiverCommand[ZAXIS]);
    // Serial.print("Throttle: ");
    // Serial.print(receiverCommand[THROTTLE]);
    // Serial.print(" Mode: ");
    // Serial.print(receiverCommand[MODE]);
    // Serial.print(" Aux: ");
    // Serial.print(receiverCommand[AUX1]);
    // Serial.println();

//    writeErrorToSerial( unCh1Center, receiverCommand[XAXIS], timer, unCh2Center, receiverCommand[YAXIS], timer);

    drive(receiverCommand[XAXIS], receiverCommand[YAXIS]);
    
  }
}


// Receives radio commands and commands the motors accordingly
// This function does a mixing to transform a throttle and a
// steering input to a command to 2 motors, one on each wheel.
void drive( int iRadioThrottle, int iRadioSteering )
{
  int iThrottle, iSteering;
  int iMotorA, iMotorB;
  
  // Maps received commands to PWM actuation values
  // MINCOMMAND and MAXCOMMAND are defined in AQ_Receiver Library
  iThrottle = map(iRadioThrottle, MINCOMMAND, MAXCOMMAND, -255, 255);
  iSteering = map(iRadioSteering, MINCOMMAND, MAXCOMMAND, -255, 255);
  
  // Positive steering is a right turn.
  iMotorA = iThrottle + iSteering;
  iMotorB = iThrottle - iSteering;
  
  // Set motor direction
  if( iMotorA >= 0 )
  {
    digitalWrite(IN1,HIGH);
    digitalWrite(IN2,LOW);
  }
  else
  {
    digitalWrite(IN1,LOW);
    digitalWrite(IN2,HIGH);
  }
  
  if( iMotorB >= 0 )
  {
    digitalWrite(IN3,HIGH);
    digitalWrite(IN4,LOW);
  }
  else
  {
    digitalWrite(IN3,LOW);
    digitalWrite(IN4,HIGH);
  }
  
  iMotorA = constrain(abs(iMotorA), 0, 255);
  iMotorB = constrain(abs(iMotorB), 0, 255);
  
  analogWrite(ENA, iMotorA);
  analogWrite(ENB, iMotorB);
  
  Serial.print(iMotorA);  
  Serial.print(",");
  Serial.print(iMotorB);
  Serial.println();

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

