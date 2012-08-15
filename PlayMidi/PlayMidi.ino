/*
 * Square wave tune with an Arduino and a PC speaker.
 * The calculation of the tones is made following the mathematical
 * operation:
 *
 *	timeUpDown = 1/(2 * toneFrequency) = period / 2
 * )c( Copyleft 2009 Daniel Gimpelevich
 * Inspired from AlexandreQuessy's http://www.arduino.cc/playground/Code/MusicalAlgoFun
 */

const byte ledPin = 13;
const byte speakerOut = 8;

/* 10.5 octaves :: semitones. 60 = do, 62 = re, etc. */
/* MIDI notes from 0, or C(-1), to 127, or G9. */
/* Rests are note number -1. */

unsigned int timeUpDown[128];

/* our song. Each number pair is a MIDI note and a note symbol. */
/* Symbols are 1 for whole, -1 for dotted whole, 2 for half, */
/* -2 for dotted half, 4 for quarter, -4 for dotted quarter, etc. */

// Happy Birthday
const char song[] = {      60,4,60,4, 62,2,60,2,65,2, 64,1,60,4,60,4, 62,2,60,2,67,2, 
                      65,1,60,4,60,4, 72,2,69,2,65,2, 64,2,62,2,70,4,70,4, 69,2,65,2,67,2, 65,1
};

int period, i;
unsigned int timeUp, beat;
byte statePin = LOW;
const byte BPM = 140;
const float TEMPO_SECONDS = 60.0 / BPM; 
const unsigned int MAXCOUNT = sizeof(song) / 2;

void setup() {
	pinMode(ledPin, OUTPUT); 
	pinMode(speakerOut, OUTPUT);
	for (i = 0; i <= 127; i++)
		timeUpDown[i] = 1000000 / (pow(2, (i - 69) / 12.0) * 880);
}

void loop() {
	digitalWrite(speakerOut, LOW);     
	for (beat = 0; beat < MAXCOUNT; beat++) {
		statePin = !statePin;
		digitalWrite(ledPin, statePin);

		i = song[beat * 2];
		timeUp = (i < 0) ? 0 : timeUpDown[i];

		period = (timeUp ? (1000000 / timeUp) / 2 : 250) * TEMPO_SECONDS
			* 4 / song[beat * 2 + 1];
		if (period < 0)
			period = period * -3 / 2;
		for (i = 0; i < period; i++) {
			digitalWrite(speakerOut, timeUp ? HIGH : LOW);
			delayMicroseconds(timeUp ? timeUp : 2000);
			digitalWrite(speakerOut, LOW);
			delayMicroseconds(timeUp ? timeUp : 2000);
		}
		delay(50);
	}
	digitalWrite(speakerOut, LOW);
	delay(1000);
}
