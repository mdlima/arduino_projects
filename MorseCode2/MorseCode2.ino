//
// Based on:
// Simple Arduino Morse Beacon by Mark VandeWettering K6HX - k6hx@arrl.net
//


#include "tone_pitches.h"
#include "morse_table.h"
 
#define N_MORSE  (sizeof(morsetab)/sizeof(morsetab[0]))
#define SPEED  (20)
#define DOTLEN  (1200/SPEED)
#define DASHLEN  (3*(1200/SPEED))

int LEDpin = 13;
int speakerPin = 8;
int toneNote = NOTE_A4;

boolean bShowPrompt = true;

String strMsg = ""; // String input from command prompt
char cInByte; // Byte input from command prompt

void dash()
{
  // LED output
  digitalWrite(LEDpin, HIGH);
  // Audio output
  tone(speakerPin, toneNote, DASHLEN);
      
  delay(DASHLEN);
  digitalWrite(LEDpin, LOW);
  noTone(speakerPin);
  delay(DOTLEN);
}

void dit()
{
  // LED output
  digitalWrite(LEDpin, HIGH);
  // Audio output
  tone(speakerPin, toneNote, DOTLEN);
  
  delay(DOTLEN);
  digitalWrite(LEDpin, LOW) ;
  noTone(speakerPin);
  delay(DOTLEN);
}

void send(char c)
{
  int i ;
  if (c == ' ') {
//    Serial.print(c);
    Serial.print("  ");
    delay(7*DOTLEN) ;
    return ;
  }
  for (i=0; i<N_MORSE; i++) 
  {
    if (morsetab[i].c == c) 
    {
      unsigned char p = morsetab[i].pat;
      Serial.print(morsetab[i].strMorse);
      Serial.print(' ');
      while (p != 1) {
          if (p & 1)
            dash() ;
          else
            dit() ;
          p = p / 2 ;
      }
      delay(2*DOTLEN) ;
      return ;
    }
  }
  /* if we drop off the end, then we send a space */
  Serial.print("?") ;
}

void sendmsg(String str)
{
  for(int i=0; i < str.length(); i++)
  {
    send(str.charAt(i));
  }
  
  Serial.println("");
  bShowPrompt = true;
}

void setup() {
  pinMode(LEDpin, OUTPUT) ;
  Serial.begin(9600) ;
  Serial.println("Simple Arduino Morse Beacon v0.1") ;
  Serial.println("Written by Marcio Lima based on tutorials available online.") ;
  Serial.println("---\n") ;
  Serial.println("") ;
}

void loop() {  
  if( bShowPrompt )
  {
    Serial.println("Enter the text to send and press ENTER:") ;
    bShowPrompt = false;
  }

  // if there's any serial available, read it:
  while (Serial.available() > 0) {

    cInByte = Serial.read();
    // only input if a letter, number, ".", ",", "?", "/" or space are typed! 
    if ((cInByte >= 65 && cInByte <= 90) || (cInByte >=97 && cInByte <= 122) || (cInByte >= 48 && cInByte <=57) || 
          cInByte == 46 || cInByte == 44 || cInByte == 63 || cInByte == 47 || cInByte == 32)
    {
      strMsg.concat(cInByte);
    }

    // look for the newline. That's the end of your sentence:
    if (cInByte == '\n') {
      cInByte = 0;
      
      strMsg.toUpperCase();
      
      // Sends message
      sendmsg(strMsg);
      strMsg = "";
    }
  }

  delay(300) ;
}
