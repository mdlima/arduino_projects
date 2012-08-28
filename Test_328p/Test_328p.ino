
/*
  AeroQuad v3.0 - March 2011
  www.AeroQuad.com
  Copyright (c) 2011 Ted Carancho.  All rights reserved.
  An Open Source Arduino based multicopter.
 
  This program is free software: you can redistribute it and/or modify 
  it under the terms of the GNU General Public License as published by 
  the Free Software Foundation, either version 3 of the License, or 
  (at your option) any later version. 

  This program is distributed in the hope that it will be useful, 
  but WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
  GNU General Public License for more details. 

  You should have received a copy of the GNU General Public License 
  along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#include <AQMath.h>
#include <GlobalDefined.h>
#include <Receiver_328p.h>
#include <Servo.h>

#define CH1_OUT_PIN 4
#define CH2_OUT_PIN 5
#define PROGRAM_PIN 10

Servo servo1;
Servo servo2;

uint16_t unCh1Center = MIDCOMMAND;
uint16_t unCh2Center = MIDCOMMAND;


unsigned long timer;

void setup() {
  
  Serial.begin(115200);
  Serial.println("Receiver library test (Receiver_APM)");
  
  pinMode(PROGRAM_PIN,INPUT);

  servo1.attach(CH1_OUT_PIN);
  servo2.attach(CH2_OUT_PIN);

  initializeReceiver(2);
}

void loop() {
  
  if(digitalRead(PROGRAM_PIN) )
  {
    // Program
    delay(500);
    
    if(digitalRead(PROGRAM_PIN) )
    {
      unCh1Center = receiverCommand[XAXIS];
      unCh2Center = receiverCommand[YAXIS];
    }
  }
  
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
    
    servo1.writeMicroseconds(receiverCommand[XAXIS]);    
    servo2.writeMicroseconds(receiverCommand[YAXIS]);
//    servo1.write(90);    
//    servo2.write(90);
    
    writeErrorToSerial( unCh1Center, receiverCommand[XAXIS], timer, unCh2Center, receiverCommand[YAXIS], timer);
    
  }
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
