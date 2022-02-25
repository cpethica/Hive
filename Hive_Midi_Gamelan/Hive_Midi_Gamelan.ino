// HIVE - 24 note solenoid gamelan
//

// Takes MIDI input from an external source or plays MIDI files from an SD card
// 4 buttons trigger 4 different MIDI files stored on the SD
// 
// Sends data as DMX to a 27 channel DMX MOSFET driver

#include <MIDI.h>  // Midi Library
#include <DMXSerial.h>
#include <SdFat.h>
#include <MD_MIDIFile.h>

#define  SD_SELECT  53  // MEGA chip select pin

// LED definitions for user indicators
#define READY_LED     7 // when finished
#define SMF_ERROR_LED 6 // SMF error
#define SD_ERROR_LED  13 // SD error
#define BEAT_LED      5  // toggles to the 'beat'

// Relay - soft start for DMX driver board and mute function
#define  RELAY      12

// Buttons and sliders
#define  BUTTON_1      33
#define  BUTTON_2      31
#define  BUTTON_3      29
#define  BUTTON_4      27
#define  SLIDER_1      A0

// Button Lights
#define  bl1 49
#define  bl2 48
#define  bl3 47
#define  bl4 46

#define  WAIT_DELAY  0  // ms

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

// The files in the tune list should be located on the SD card 
// or an error will occur opening the file and the next in the 
// list will be opened (skips errors).
//
// Make 2D array in order to have different tracks for each button press
// based on rotary encoder position...
char *tuneList[][4] = 
{
  {"Track1-1.mid",
  "Track1-2.mid",
  "Track1-3.mid",
  "Track1-4.mid"},
  {"Track2-1.mid",
  "Track2-2.mid",
  "Track2-3.mid",
  "Track2-4.mid"},
  {"Track3-1.mid",
  "Track3-2.mid",
  "Track3-3.mid",
  "Track3-4.mid"},
  {"Track4-1.mid",
  "Track4-2.mid",
  "Track4-3.mid",
  "Track4-4.mid"}
};

// button and slider initial states
int buttonState[4];
int sliderValue1 = 0;
int alb = 0;

SdFat SD;
MD_MIDIFile SMF;

//note timers
const long safety = 1500;     // maximum on time for any output is 1.5 seconds
unsigned long note[26];

#define LED 13    // set an led pin here ... to flash when sending/receiving


MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDI);
// Create instance for external MIDI input

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
  MIDINoteSD(pev->data, pev->size);   // pass data array to midi to dmx function 
 // send MIDI data from file to MIDINoteOn function in order to output as DMX
 // MIDINoteOn needs : MIDINoteOn(byte channel, byte pitch, byte velocity)
//  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))   // if status byte...
//  {
//    MIDINoteSD(pev->data[0] | pev->channel, &pev->data[1], pev->size-1)
//    //Serial.write(pev->data[0] | pev->channel);
//    //Serial.write(&pev->data[1], pev->size-1);
//  }
//  else
//    MIDINoteSD(pev->data, pev->size);
//    //Serial.write(pev->data, pev->size);
 
}

void sysexCallback(sysex_event *pev)
// Called by the MIDIFile library when a system Exclusive (sysex) file event needs 
// to be processed through the midi communications interface. Most sysex events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
  midi_event  ev;

  // All sound off
  // When All Sound Off is received all oscillators will turn off, and their volume
  // envelopes are set to zero as soon as possible.
  ev.size = 0;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 120;
  ev.data[ev.size++] = 0;

  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}

void setup() {

  delay(2000);        // wait for power on before switching relay
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);

  DMXSerial.init(DMXController);

  // External MIDI input handlers
  MIDI.begin(MIDI_CHANNEL_OMNI); // Set MIDI channel, MIDI_CHANNEL_OMNI = all, number = channel 
  MIDI.setHandleNoteOn(MIDINoteOn); // call Note On function - this does MIDI to DMX conversion
  MIDI.setHandleNoteOff(MIDINoteOff); // call Note Off function - could ditch and replace with timer on MEGA???

  pinMode (LED, OUTPUT); // Set Arduino board pin 13 to output  
  
   // Set up LED pins
  pinMode(READY_LED, OUTPUT);
  pinMode(SD_ERROR_LED, OUTPUT);
  pinMode(SMF_ERROR_LED, OUTPUT);
  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_2, INPUT);
  pinMode(BUTTON_3, INPUT);
  pinMode(BUTTON_4, INPUT);  
  pinMode(SLIDER_1, INPUT);
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
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);

  digitalWrite(READY_LED, HIGH);
}

void buttons() {
    // read buttons
  buttonState[0] = digitalRead(BUTTON_1);
  buttonState[1] = digitalRead(BUTTON_2); 
  buttonState[2] = digitalRead(BUTTON_3);
  buttonState[3] = digitalRead(BUTTON_4);
  sliderValue1 = analogRead(SLIDER_1);
  alb = map(sliderValue1, 0, 1023, 0, 3);   // map pot value to 0-3 to select album
}

void loop() { // Main loop
  
  // process MIDI from external sources - may need to be called elsewhere to avoid MIDI data being blocked by other functions
  MIDI.read();
 
  // read button values
  buttons();

  // play MIDI files according to button press - could be 2D array to give multiple track options per button?
  if (buttonState[0] == HIGH)
  {
    digitalWrite(bl1, buttonState[0]);    //light up button   -- dim a bit for incandescents...
    bulbs(0);   // bulbs off
    play(alb,0);   // play first track in first album
    digitalWrite(bl1, LOW);    //light off   
  }
  else if (buttonState[1] == HIGH)
  {
    digitalWrite(bl2, buttonState[1]);    //light up button   -- dim a bit for incandescents...
    bulbs(0);
    play(alb,1);   // play second track first album...
    digitalWrite(bl2, LOW);    //light off        
  }
  else if (buttonState[2] == HIGH)
  {
    digitalWrite(bl3, buttonState[2]);    //light up button
    bulbs(0);
    play(alb,2);
    digitalWrite(bl3, LOW);    //light up button    
  }
  else if (buttonState[3] == HIGH)
  {
    digitalWrite(bl4, buttonState[3]);    //light up button
    bulbs(0);
    play(alb,3);
    digitalWrite(bl4, LOW);    //light up button        
  }  

}

void tickMetronome(void)
// flash a LED to the beat
{
  static uint32_t lastBeatTime = 0;
  static boolean  inBeat = false;
  uint16_t  beatTime;

  beatTime = 60000/SMF.getTempo();    // msec/beat = ((60sec/min)*(1000 ms/sec))/(beats/min)
  if (!inBeat)
  {
    if ((millis() - lastBeatTime) >= beatTime)
    {
      lastBeatTime = millis();
      digitalWrite(BEAT_LED, HIGH);
      inBeat = true;
    }
  }
  else
  {
    if ((millis() - lastBeatTime) >= 100) // keep the flash on for 100ms only
    {
      digitalWrite(BEAT_LED, LOW);
      inBeat = false;
    }
  }
}

//play selected MIDI file, all external MIDI events should now be ignored until next call to MIDI.read()
// possibly could do both, would need MIDI.read() called in middle of this function...
void play(int album, int trk)       // album number, track number
{
  int  err;
  // reset LEDs
  digitalWrite(READY_LED, LOW);
  digitalWrite(SD_ERROR_LED, LOW);

  SMF.setFilename(tuneList[album][trk]);
  err = SMF.load();
  if (err != -1)
  {
  digitalWrite(SMF_ERROR_LED, HIGH);
  delay(WAIT_DELAY);
  }
  else
  {
  // play the file
  while (!SMF.isEOF())
  {
    if (SMF.getNextEvent())
    tickMetronome();
      // Add in button based file close here???
    buttons();
    //check for MIDI// note off timers - safety feature to stop solenoids getting stuck on
    unsigned long current = millis();
    for (int i=0; i <= 25; i++) {
      if (current - note[i] >= safety) {
       // save the last time note was on 
       note[i] = current;
       // set note off
       DMXSerial.write(i, 0);   
       }
    }
      if (buttonState[trk] == LOW)     // button pressed corresponds to track number so can reuse trk variable...
      {
       // if lever is let go then exit...
       SMF.close();
       alloff();
       midiSilence();
       digitalWrite(READY_LED, HIGH);
       delay(WAIT_DELAY);
      }
  }
  // done with this one
  alloff();
  SMF.close();
  midiSilence();

  // signal finish LED with a dignified pause
  digitalWrite(READY_LED, HIGH);
  delay(WAIT_DELAY);
  // maybe flush MIDI input port here to dump anything received while track is playing?

 }
} 

// send ambient lighting control to bulbs while not playing music
void bulbs (int brightness) {
  
    DMXSerial.write(28, brightness);         // multiply by state to zero light value when off
    DMXSerial.write(29, brightness);
    DMXSerial.write(30, brightness);
    DMXSerial.write(31, brightness);
}

void alloff() {
  for (int i = 0; i <= 25; i++) {
    DMXSerial.write(i, 0);
  }
}

// handle MIDI to DMX for output - needs 2 functions :
    // one for external and one for internal midi
void MIDINoteSD(byte test[], int sizeofArray) { 
  
    digitalWrite(LED,LOW);  //Turn LED on/off when MIDI note is detete
    byte pitch = 0;
    byte velocity = 0;
    if (test[0] >= 0x80 && test[0] <= 0xE0) {      // (pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0)
      pitch = test[1];
      if (test[0] >= 0x90 && test[0] <= 0x9F) {
        velocity = 255;
      }
      else {
        velocity = 0;
      }
    }
    else {
      pitch = test[0];
      if (test[0] >= 0x90 && test[0] <= 0x9F) {
        velocity = 255;
      }
      else {
        velocity = 0;
      }
    }

switch (pitch) {
    case 60:
    note[pitch] = millis();
    DMXSerial.write(1, velocity);   
      break;
    case 61:
    note[pitch] = millis();    
    DMXSerial.write(2, velocity);
      break;
    case 62:
    note[pitch] = millis(); 
    DMXSerial.write(3, velocity);
      break;
    case 63:
    note[pitch] = millis(); 
    DMXSerial.write(4, velocity); 
      break;
    case 64:
    note[pitch] = millis();
    DMXSerial.write(5, velocity); 
      break;
    case 65:
    note[pitch] = millis(); 
    DMXSerial.write(6, velocity);  
      break;
    case 66:
    note[pitch] = millis(); 
    DMXSerial.write(7, velocity);   // midi values are 0-127 so multiply by 2 to get DMX value (0-velocity)
      break;
    case 67:
    note[pitch] = millis(); 
    DMXSerial.write(8, velocity);
      break;
    case 68:
    note[pitch] = millis(); 
    DMXSerial.write(9, velocity); 
      break;
    case 69:
    note[pitch] = millis(); 
    DMXSerial.write(10, velocity); 
      break;
    case 70:
    note[pitch] = millis(); 
    DMXSerial.write(11, velocity); 
      break;
    case 71:
    note[pitch] = millis(); 
    DMXSerial.write(12, velocity); 
      break;
    case 72:
    note[pitch] = millis(); 
    DMXSerial.write(13, velocity);
      break;
    case 73:
    note[pitch] = millis(); 
    DMXSerial.write(14, velocity);
      break;
    case 74:
    note[pitch] = millis(); 
    DMXSerial.write(15, velocity);
      break;
    case 75:
    note[pitch] = millis(); 
    DMXSerial.write(16, velocity); 
      break;
    case 76:
    note[pitch] = millis(); 
    DMXSerial.write(17, velocity); 
      break;
    case 77:
    note[pitch] = millis(); 
    DMXSerial.write(18, velocity);
      break;
    case 78:
    note[pitch] = millis(); 
    DMXSerial.write(19, velocity);
      break;
    case 79:
    note[pitch] = millis(); 
    DMXSerial.write(20, velocity);
      break;
    case 80:
    note[pitch] = millis(); 
    DMXSerial.write(21, velocity);
      break;
    case 81:
    note[pitch] = millis(); 
    DMXSerial.write(22, velocity); 
      break;
    case 82:
    note[pitch] = millis(); 
    DMXSerial.write(23, velocity); 
      break;
    case 83:
    note[pitch] = millis(); 
    DMXSerial.write(24, velocity);
      break;
    case 84:
    note[pitch] = millis(); 
    DMXSerial.write(25, velocity); 
      break;
    case 85:
    note[pitch] = millis(); 
    DMXSerial.write(26, velocity);
      break;
    case 86:
    note[pitch] = millis(); 
    DMXSerial.write(27, velocity);
      break;       
    case 87:
    note[pitch] = millis(); 
    DMXSerial.write(28, velocity);
      break;       
    case 88:
    note[pitch] = millis(); 
    DMXSerial.write(29, velocity);
      break;       
    case 89:
    note[pitch] = millis(); 
    DMXSerial.write(30, velocity);
      break;       
  }
}



void MIDINoteOn(byte channel, byte pitch, byte velocity) {

  digitalWrite(bl1, HIGH);    // test MIDI received...

switch (pitch) {
    case 60:
    note[pitch] = millis();
    DMXSerial.write(1, velocity);   
      break;
    case 61:
    note[pitch] = millis();    
    DMXSerial.write(2, velocity);
      break;
    case 62:
    note[pitch] = millis(); 
    DMXSerial.write(3, velocity);
      break;
    case 63:
    note[pitch] = millis(); 
    DMXSerial.write(4, velocity); 
      break;
    case 64:
    note[pitch] = millis();
    DMXSerial.write(5, velocity); 
      break;
    case 65:
    note[pitch] = millis(); 
    DMXSerial.write(6, velocity);  
      break;
    case 66:
    note[pitch] = millis(); 
    DMXSerial.write(7, velocity);   // midi values are 0-127 so multiply by 2 to get DMX value (0-velocity)
      break;
    case 67:
    note[pitch] = millis(); 
    DMXSerial.write(8, velocity);
      break;
    case 68:
    note[pitch] = millis(); 
    DMXSerial.write(9, velocity); 
      break;
    case 69:
    note[pitch] = millis(); 
    DMXSerial.write(10, velocity); 
      break;
    case 70:
    note[pitch] = millis(); 
    DMXSerial.write(11, velocity); 
      break;
    case 71:
    note[pitch] = millis(); 
    DMXSerial.write(12, velocity); 
      break;
    case 72:
    note[pitch] = millis(); 
    DMXSerial.write(13, velocity);
      break;
    case 73:
    note[pitch] = millis(); 
    DMXSerial.write(14, velocity);
      break;
    case 74:
    note[pitch] = millis(); 
    DMXSerial.write(15, velocity);
      break;
    case 75:
    note[pitch] = millis(); 
    DMXSerial.write(16, velocity); 
      break;
    case 76:
    note[pitch] = millis(); 
    DMXSerial.write(17, velocity); 
      break;
    case 77:
    note[pitch] = millis(); 
    DMXSerial.write(18, velocity);
      break;
    case 78:
    note[pitch] = millis(); 
    DMXSerial.write(19, velocity);
      break;
    case 79:
    note[pitch] = millis(); 
    DMXSerial.write(20, velocity);
      break;
    case 80:
    note[pitch] = millis(); 
    DMXSerial.write(21, velocity);
      break;
    case 81:
    note[pitch] = millis(); 
    DMXSerial.write(22, velocity); 
      break;
    case 82:
    note[pitch] = millis(); 
    DMXSerial.write(23, velocity); 
      break;
    case 83:
    note[pitch] = millis(); 
    DMXSerial.write(24, velocity);
      break;
    case 84:
    note[pitch] = millis(); 
    DMXSerial.write(25, velocity); 
      break;
    case 85:
    note[pitch] = millis(); 
    DMXSerial.write(26, velocity);
      break;
     case 86:
    note[pitch] = millis(); 
    DMXSerial.write(27, velocity);
      break;       
    case 87:
    note[pitch] = millis(); 
    DMXSerial.write(28, velocity);
      break;       
    case 88:
    note[pitch] = millis(); 
    DMXSerial.write(29, velocity);
      break;       
    case 89:
    note[pitch] = millis(); 
    DMXSerial.write(30, velocity);
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
    DMXSerial.write(12, 0); 
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
    DMXSerial.write(25, 0);
      break;
    case 85:
    DMXSerial.write(26, 0);
      break;
    case 86:
    DMXSerial.write(27, 0); 
      break;
    case 87:
    DMXSerial.write(28, 0); 
      break;
    case 88:
    DMXSerial.write(29, 0);
      break; 
    case 89:
    DMXSerial.write(30, 0);
      break;                
  }
  
      
} // void MIDINoteOff
