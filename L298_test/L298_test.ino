
/***************
*
* Sample code to test L298 DC motor controller board
* 
* Code copied from http://www.geekonfire.com/wiki/index.php?title=Dual_H-Bridge_Motor_Driver
*
****************/

int ENA=A0;
int IN1=A1;
int IN2=A2;
int ENB=A3;
int IN3=A4;
int IN4=A5;

void setup()
{
 pinMode(ENA,OUTPUT);//output
 pinMode(ENB,OUTPUT);
 pinMode(IN1,OUTPUT);
 pinMode(IN2,OUTPUT);
 pinMode(IN3,OUTPUT);
 pinMode(IN4,OUTPUT);
 digitalWrite(ENA,LOW);
 digitalWrite(ENB,LOW);//stop driving
 digitalWrite(IN1,LOW); 
 digitalWrite(IN2,HIGH);//setting motorA's directon
 digitalWrite(IN3,HIGH);
 digitalWrite(IN4,LOW);//setting motorB's directon
}
void loop()
{
  analogWrite(ENA,255);//start driving motorA
  analogWrite(ENB,255);//start driving motorB
 
}
