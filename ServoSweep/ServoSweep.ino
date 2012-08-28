// Sweep
// by BARRAGAN <http://barraganstudio.com> 
// This example code is in the public domain.


#include <Servo.h> 
 
Servo myservo;  // create servo object to control a servo 
                // a maximum of eight servo objects can be created 
Servo myServo2;
 
int pos = 0;    // variable to store the servo position 
 
void setup() 
{ 
  myservo.attach(4);  // attaches the servo on pin 9 to the servo object 
  myServo2.attach(5);
  myservo.write(90);
  myServo2.write(90);
} 
 
 
void loop() 
{ 

//  for(pos = 0; pos < 180; pos += 1)  // goes from 0 degrees to 180 degrees 
//  {                                  // in steps of 1 degree 
//    myservo.write(pos);              // tell servo to go to position in variable 'pos' 
//    delay(15);                       // waits 15ms for the servo to reach the position 
//  } 
//  for(pos = 180; pos>=1; pos-=1)     // goes from 180 degrees to 0 degrees 
//  {                                
//    myservo.write(pos);              // tell servo to go to position in variable 'pos' 
//    delay(15);                       // waits 15ms for the servo to reach the position 
//  }

  for(pos = MIN_PULSE_WIDTH; pos <= MAX_PULSE_WIDTH; pos += 1)  // goes from 0 degrees to 180 degrees 
  {                                  // in steps of 1 degree 
    myservo.writeMicroseconds(pos);              // tell servo to go to position in variable 'pos' 
    delay(1);                       // waits 15ms for the servo to reach the position 
  } 
  for(pos = MAX_PULSE_WIDTH; pos>=MIN_PULSE_WIDTH; pos-=1)     // goes from 180 degrees to 0 degrees 
  {                                
    myservo.writeMicroseconds(pos);              // tell servo to go to position in variable 'pos' 
    delay(1);                       // waits 15ms for the servo to reach the position 
  } 

//  myservo.write(0);
//  delay(2700);
//  myservo.write(180);
//  delay(2700);

} 
