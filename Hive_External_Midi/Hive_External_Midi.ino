// HIVE - 24 note solenoid gamelan
//

// Takes MIDI input from an external source or plays MIDI files from an SD card
// Operates in a jukebox mode playing random tracks until an external MIDI input
// is received. It will then
// 
// Sends data as DMX to a 27 channel DMX MOSFET driver

#include <MIDI.h>  // Midi Library
#include <DMXSerial.h>
#include <SdFat.h>
#include <MD_MIDIFile.h>

#define  SD_SELECT  53  // MEGA chip select pin

// LED definitions for user indicators

#define READY_LED     9 // when finished
#define SMF_ERROR_LED 13 // SMF error
#define SD_ERROR_LED  3 // SD error
#define BEAT_LED      5  // toggles to the 'beat'

// Relay - soft start for DMX driver board and mute function
#define  RELAY      12

// Button Lights  //  COULD REPURPOSE - THEY RUN OFF THE SEPARATE MOSFET BOARD
#define  bl1 49
#define  bl2 48
#define  bl3 47
#define  bl4 46

// fading lights
// define directions for LED fade
#define UP 0
#define DOWN 1
// constants for min and max PWM
const int minPWM = 0;
const int maxPWM = 120;
// State Variable for Fade Direction
byte fadeDirection = UP;
// Global Fade Value
// but be bigger than byte and signed, for rollover
int fadeValue = 0;
// How smooth to fade?
byte fadeIncrement = 5;
// millis() timing Variable, just for fading
unsigned long previousFadeMillis;
// How fast to increment?
int fadeInterval = 50;


#define  WAIT_DELAY  10  // ms

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

// The files in the tune list should be located on the SD card 
// or an error will occur opening the file and the next in the 
// list will be opened (skips errors).
//
// Make 2D array in order to have different tracks for each button press
// based on rotary encoder position...
char *tuneList[4] = 
{
"Track1.mid",            //   CHANGE THIS TO TRACKLIST AS THERE WILL BE NO DIAL TO CONTROL TRACKS
"Track2.mid",
"Track3.mid",
"Track4.mid"
};


// data and status bytes
byte pitch = 0;
byte velocity = 0;
byte stat = 0;

SdFat SD;
MD_MIDIFile SMF;

//note timers
// define note on time (ms)
const int playtime = 5000;
// delay after last midi note until jukebox starts playing (ms)
unsigned long midi_delay = 60000;
// store note on or off and note on times
bool notes[34];
long times[34];


#define LED 13    // set an led pin here ... to flash when sending/receiving


MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDI);
// Create instance for external MIDI input


void setup() {

  delay(2000);        // wait for power on before switching relay
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);
  
  DMXSerial.init(DMXController);

  // External MIDI input handlers
  MIDI.begin(MIDI_CHANNEL_OMNI); // Set MIDI channel, MIDI_CHANNEL_OMNI = all, number = channel 
  MIDI.setHandleNoteOn(MIDINoteOn); // call Note On function - this does MIDI to DMX conversion
  MIDI.setHandleNoteOff(MIDINoteOff); // call Note Off function - could ditch and replace with timer on MEGA???

  //feedback leds
  pinMode (LED, OUTPUT); // Set Arduino board pin 13 to output  
  
  // Set up LED pins
  pinMode(READY_LED, OUTPUT);
  pinMode(SD_ERROR_LED, OUTPUT);
  pinMode(SMF_ERROR_LED, OUTPUT);
  pinMode(bl1, OUTPUT);
  pinMode(bl2, OUTPUT);
  pinMode(bl3, OUTPUT);
  pinMode(bl4, OUTPUT);

  // lights off at start
  digitalWrite(bl1, LOW);
  digitalWrite(bl2, LOW);
  digitalWrite(bl3, LOW);
  digitalWrite(bl4, LOW);     

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    digitalWrite(SD_ERROR_LED, HIGH);
    while (true) ;
  }

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(MIDINoteSD);       // SD card MIDI message handler

  digitalWrite(READY_LED, LOW);
  // generate random number for track number
  randomSeed(analogRead(A10));

}
  
void loop() { // Main loop
  
  // process MIDI from external sources - may need to be called elsewhere to avoid MIDI data being blocked by other functions
  MIDI.read();
  // fade lights
  // note off timers - switch off note after defined time  
  // play random track from array
  //play(random(3));
  timers();
  
}

void timers()     // call to check whether note needs to be switched off (after playtime variable)
{
  // note off timers - switch off note after defined time
  long current = millis();
  for (int i=0; i < 35; i++) {
    // check if note is on and how long its been on for
    if (notes[i] == true && current - times[i] > playtime) {
     // save the last time note was on 
     //times[i] = current;   //  results in lots of note off's being sent... do we need a boolean note on/off state as well as the timer so if loop checks this first?
     // set note off
     DMXSerial.write(i+1, 0);
//     DMXSerial.write(2, HIGH);
//     delay(500);
//     DMXSerial.write(2, LOW);     
     notes[i] = false;
    }
  }
}

void external_midi()
{
  long time_now = millis();
  while (millis() < time_now + 60000){
      timers();
      if (MIDI.read()) {
        time_now = millis();    // if midi is received then reset time to wait another 10 seconds
        timers();
      }
  }
}

//play selected MIDI file, all external MIDI events should now be ignored until next call to MIDI.read()
// possibly could do both, would need MIDI.read() called in middle of this function...
void play(int trk)       // album number, track number
{
  int  err;
  // reset LEDs
  digitalWrite(SD_ERROR_LED, LOW);

  SMF.setFilename(tuneList[trk]);
  err = SMF.load();
  if (err != -1)
  {
  digitalWrite(SMF_ERROR_LED, HIGH);
  delay(WAIT_DELAY);
  }
  else
  {
  // play the file
  digitalWrite(READY_LED, HIGH);
  while (!SMF.isEOF())
  {
    if (SMF.getNextEvent())
    
    //check for MIDI// note off timers - safety feature to stop solenoids getting stuck on
    if (MIDI.read()) {
      alloff();
      SMF.close();
      timers();
      external_midi();
    }
    timers();
  }
  // done with this one
  alloff();
  SMF.close();
  // signal finish LED with a dignified pause
  digitalWrite(READY_LED, LOW);
  delay(3000);
  }
} 

void alloff() {
  for (int i = 0; i <= 34; i++) {
    DMXSerial.write(i, 0);
  }
}

// handle MIDI to DMX for output - needs 2 functions :
    // one for external and one for internal midi
void MIDINoteSD(midi_event *pev) {

  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))   // >= 128 and <=224 so NOT running status...
  {
    //Serial.write(pev->data[0] | pev->channel);      // status byte
    
    // new:
    stat = pev->data[0];    // status byte (ignore channel data)
     if (stat >= 0x90 && stat <= 0x9F) {    // find note on bytes
        velocity = 255;                           // full velocity for note on
      }
      else {
        velocity = 0;                              // zero velocity for note off
      }
    pitch = pev->data[1];
    
    //Serial.write(&pev->data[1], pev->size-1);       // then data...
    
  }
  else  
  {
    // running status -> no note on/off status just send pitch and velocity with same status as previous
    //Serial.write(pev->data, pev->size);
    
    // new:
    pitch = pev->data[0];
    velocity = pev->data[1];
  }

switch (pitch) {
    case 60:
    times[0] = millis();
    notes[0] = true;
    DMXSerial.write(1, 255);   
      break;
    case 61:
    times[1] = millis();
    notes[1] = true;
    DMXSerial.write(2, 255);
      break;
    case 62:
    times[2] = millis();
    notes[2] = true;
    DMXSerial.write(3, 255);
      break;
    case 63:
    times[3] = millis();
    notes[3] = true;
    DMXSerial.write(4, 255); 
      break;
    case 64:
    times[4] = millis();
    notes[4] = true;
    DMXSerial.write(5, 255); 
      break;
    case 65:
    times[5] = millis();
    notes[5] = true;
    DMXSerial.write(6, 255);  
      break;
    case 66:
    times[6] = millis();
    notes[6] = true;
    DMXSerial.write(7, 255);   // midi values are 0-127 so multiply by 2 to get DMX value (0-255)
      break;
    case 67:
    times[7] = millis();
    notes[7] = true;
    DMXSerial.write(8, 255);
      break;
    case 68:
    times[8] = millis();
    notes[8] = true;
    DMXSerial.write(9, 255); 
      break;
    case 69:
    times[9] = millis();
    notes[9] = true;
    DMXSerial.write(10, 255); 
      break;
    case 70:
    times[10] = millis();
    notes[10] = true;
    DMXSerial.write(11, 255); 
      break;
    case 71:
    times[24] = millis();           /// channel dead on brain so now wired to channel: 25
    notes[24] = true;
    DMXSerial.write(25, 255); 
      break;
    case 72:
    times[12] = millis();
    notes[12] = true;
    DMXSerial.write(13, 255);
      break;
    case 73:
    times[13] = millis();
    notes[13] = true; 
    DMXSerial.write(14, 255);
      break;
    case 74:
    times[14] = millis();
    notes[14] = true;
    DMXSerial.write(15, 255);
      break;
    case 75:
    times[15] = millis();
    notes[15] = true;
    DMXSerial.write(16, 255); 
      break;
    case 76:
    times[16] = millis();
    notes[16] = true;
    DMXSerial.write(17, 255); 
      break;
    case 77:
    times[17] = millis();
    notes[17] = true;
    DMXSerial.write(18, 255);
      break;
    case 78:
    times[18] = millis();
    notes[18] = true;
    DMXSerial.write(19, 255);
      break;
    case 79:
    times[19] = millis();
    notes[19] = true;
    DMXSerial.write(20, 255);
      break;
    case 80:
    times[20] = millis();
    notes[20] = true;
    DMXSerial.write(21, 255);
      break;
    case 81:
    times[21] = millis();
    notes[21] = true;
    DMXSerial.write(22, 255); 
      break;
    case 82:
    times[22] = millis();
    notes[22] = true;
    DMXSerial.write(23, 255); 
      break;
    case 83:
    times[23] = millis();
    notes[23] = true;
    DMXSerial.write(24, 255);
      break;
    case 84:
    times[27] = millis();
    notes[27] = true;
    DMXSerial.write(28, 255);
      break;
    case 85:
    times[28] = millis();
    notes[28] = true;
    DMXSerial.write(29, 255);
      break;
    case 86:
    times[29] = millis();
    notes[29] = true;
    DMXSerial.write(30, 255);
      break;      
    case 87:
    times[31] = millis();
    notes[31] = true;
    DMXSerial.write(32, 255);
      break;
    case 88:
    times[32] = millis();
    notes[32] = true;
    DMXSerial.write(33, 255);
      break;      
    case 89:
    times[33] = millis();
    notes[33] = true;
    DMXSerial.write(34, 255);
      break;
      
  }
}



void MIDINoteOn(byte channel, byte pitch, byte velocity) {

switch (pitch) {
    case 60:
    times[0] = millis();
    notes[0] = true;
    DMXSerial.write(1, velocity*2);   
      break;
    case 61:
    times[1] = millis();
    notes[1] = true;  
    DMXSerial.write(2, velocity*2);
      break;
    case 62:
    times[2] = millis();
    notes[2] = true;
    DMXSerial.write(3, velocity*2);
      break;
    case 63:
    times[3] = millis();
    notes[3] = true;
    DMXSerial.write(4, velocity*2); 
      break;
    case 64:
    times[4] = millis();
    notes[4] = true;
    DMXSerial.write(5, velocity*2); 
      break;
    case 65:
    times[5] = millis();
    notes[5] = true; 
    DMXSerial.write(6, velocity*2);  
      break;
    case 66:
    times[6] = millis();
    notes[6] = true;
    DMXSerial.write(7, velocity*2);   // midi values are 0-127 so multiply by 2 to get DMX value (0-velocity*2)
      break;
    case 67:
    times[7] = millis();
    notes[7] = true;
    DMXSerial.write(8, velocity*2);
      break;
    case 68:
    times[8] = millis();
    notes[8] = true; 
    DMXSerial.write(9, velocity*2); 
      break;
    case 69:
    times[9] = millis();
    notes[9] = true; 
    DMXSerial.write(10, velocity*2); 
      break;
    case 70:
    times[10] = millis();
    notes[10] = true; 
    DMXSerial.write(11, velocity*2); 
      break;
    case 71:
    times[24] = millis();
    notes[24] = true;
    DMXSerial.write(25, velocity*2); 
      break;
    case 72:
    times[12] = millis();
    notes[12] = true; 
    DMXSerial.write(13, velocity*2);
      break;
    case 73:
    times[13] = millis();
    notes[13] = true; 
    DMXSerial.write(14, velocity*2);
      break;
    case 74:
    times[14] = millis();
    notes[14] = true; 
    DMXSerial.write(15, velocity*2);
      break;
    case 75:
    times[15] = millis();
    notes[15] = true; 
    DMXSerial.write(16, velocity*2); 
      break;
    case 76:
    times[16] = millis();
    notes[16] = true;
    DMXSerial.write(17, velocity*2); 
      break;
    case 77:
    times[17] = millis();
    notes[17] = true;
    DMXSerial.write(18, velocity*2);
      break;
    case 78:
    times[18] = millis();
    notes[18] = true;
    DMXSerial.write(19, velocity*2);
      break;
    case 79:
    times[19] = millis();
    notes[19] = true;
    DMXSerial.write(20, velocity*2);
      break;
    case 80:
    times[20] = millis();
    notes[20] = true;
    DMXSerial.write(21, velocity*2);
      break;
    case 81:
    times[21] = millis();
    notes[21] = true; 
    DMXSerial.write(22, velocity*2); 
      break;
    case 82:
    times[22] = millis();
    notes[22] = true;
    DMXSerial.write(23, velocity*2); 
      break;
    case 83:
    times[23] = millis();
    notes[23] = true;
    DMXSerial.write(24, velocity*2);
      break;
    case 84:
    times[27] = millis();
    notes[27] = true;
    DMXSerial.write(28, velocity*2);
      break;
    case 85:
    times[28] = millis();
    notes[28] = true;
    DMXSerial.write(29, velocity*2);
      break;
    case 86:
    times[29] = millis();
    notes[29] = true;
    DMXSerial.write(30, velocity*2);
      break;      
    case 87:
    times[31] = millis();
    notes[31] = true;
    DMXSerial.write(32, velocity*2);
      break; 
    case 88:
    times[32] = millis();
    notes[32] = true;
    DMXSerial.write(33, velocity*2);
      break;      
    case 89:
    times[33] = millis();
    notes[33] = true;
    DMXSerial.write(34, velocity*2);
      break;          
  }
} // void MIDINoteOn


void MIDINoteOff(byte channel, byte pitch, byte velocity) { 

// RESET timers when note off received???
  
  //Zero DMX channel (make sure you zero ALL your DMX channels here)
switch (pitch) {
    case 60:
    DMXSerial.write(1, 0);   
      break;
    case 61:
    DMXSerial.write(2, 0);
      break;
    case 62:
    DMXSerial.write(3, 0);
      break;
    case 63:
    DMXSerial.write(4, 0); 
      break;
    case 64:
    DMXSerial.write(5, 0); 
      break;
    case 65:
    DMXSerial.write(6, 0);   
      break;
    case 66:
    DMXSerial.write(7, 0);   // midi values are 0-127 so multiply by 2 to get DMX value (0-255)
      break;
    case 67:
    DMXSerial.write(8, 0);
      break;
    case 68:
    DMXSerial.write(9, 0);
      break;
    case 69:
    DMXSerial.write(10, 0); 
      break;
    case 70:
    DMXSerial.write(11, 0); 
      break;
    case 71:
    DMXSerial.write(25, 0); 
      break;
    case 72:
    DMXSerial.write(13, 0);
      break;
    case 73:
    DMXSerial.write(14, 0);
      break;
    case 74:
    DMXSerial.write(15, 0);
      break;
    case 75:
    DMXSerial.write(16, 0); 
      break;
    case 76:
    DMXSerial.write(17, 0); 
      break;
    case 77:
    DMXSerial.write(18, 0); 
      break;
    case 78:
    DMXSerial.write(19, 0);  
      break;
    case 79:
    DMXSerial.write(20, 0);
      break;
    case 80:
    DMXSerial.write(21, 0);
      break;
    case 81:
    DMXSerial.write(22, 0); 
      break;
    case 82:
    DMXSerial.write(23, 0); 
      break;
    case 83:
    DMXSerial.write(24, 0);
      break;     
    case 84:
    DMXSerial.write(28, 0);
      break;
    case 85:
    DMXSerial.write(29, 0);
      break;
    case 86:
    DMXSerial.write(30, 0);
      break;      
    case 87:
    DMXSerial.write(32, 0);
      break;  
    case 88:
    DMXSerial.write(33, 0);
      break;      
    case 89:
    DMXSerial.write(34, 0);
      break;               
  }
  
      
} // void MIDINoteOff
