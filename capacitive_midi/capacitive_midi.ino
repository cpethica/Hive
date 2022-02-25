/*********************************************************
24 channel MIDI sender - reads capacitive touch sensors and sends data as MIDI
**********************************************************/

#include <Wire.h>
#include "Adafruit_MPR121.h"
#include <MIDI.h>

#ifndef _BV1
#define _BV1(bit) (1 << (bit)) 
#endif

#ifndef _BV2
#define _BV2(bit) (1 << (bit)) 
#endif

#define ledPin 8    // indicator LED - ON/OFF AND MIDI

// You can have up to 4 on one i2c bus but one is enough for testing!
Adafruit_MPR121 cap1 = Adafruit_MPR121();
Adafruit_MPR121 cap2 = Adafruit_MPR121();

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t lasttouched1 = 0;
uint16_t currtouched1 = 0;
uint16_t lasttouched2 = 0;
uint16_t currtouched2 = 0;

// define note on time
const int playtime = 180;
unsigned long note[24];

MIDI_CREATE_DEFAULT_INSTANCE();

void setup() {
   MIDI.begin(4); // Set MIDI channel, MIDI_CHANNEL_OMNI = all, number = channel 
   //Serial.begin(115200);

  //Serial.println("Adafruit MPR121 Capacitive Touch sensor test"); 
  
  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if (!cap1.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  if (!cap2.begin(0x5B)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  //Serial.println("MPR121 found!");
  pinMode(ledPin, OUTPUT);

}

void loop() {
  // Get the currently touched pads
  currtouched1 = cap1.touched();
  currtouched2 = cap2.touched();

  unsigned long current = millis();  
  
  for (uint8_t i=0; i<12; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched1 & _BV1(i)) && !(lasttouched1 & _BV1(i)) ) {
      MIDI.sendNoteOn(60+i, 127, 1);
      note[i] = millis();
      digitalWrite(ledPin, HIGH);
      //Serial.print(i); Serial.println(" touched");
    }
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched2 & _BV2(i)) && !(lasttouched2 & _BV2(i)) ) {
      MIDI.sendNoteOn(72+i, 127, 1);
      note[12+i] = millis();
      digitalWrite(ledPin, HIGH);
      //Serial.print(i); Serial.println(" touched");
    }    
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched1 & _BV1(i)) && (lasttouched1 & _BV1(i)) ) {
      MIDI.sendNoteOff(60+i, 0, 1);
      note[i] = current;
      digitalWrite(ledPin, LOW);
      //Serial.print(i); Serial.println(" released");
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched2 & _BV2(i)) && (lasttouched2 & _BV2(i)) ) {
      MIDI.sendNoteOff(72+i, 0, 1);
      note[12+i] = current;
      digitalWrite(ledPin, LOW);
      //Serial.print(i); Serial.println(" released");
    }    
  }


  // reset our state
  lasttouched1 = currtouched1;
  lasttouched2 = currtouched2;

}
