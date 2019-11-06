#include <Wire.h>
#include "MIDIUSB.h"
#include "FastLED.h"

#define NUM_LEDS 44
#define DATA_PIN 10
CRGB leds[NUM_LEDS];

// MCP23017 registers (everything except direction defaults to 0)

#define IODIRA   0x00   // IO direction  (0 = output, 1 = input (Default))
#define IODIRB   0x01
#define IOPOLA   0x02   // IO polarity   (0 = normal, 1 = inverse)
#define IOPOLB   0x03
#define GPINTENA 0x04   // Interrupt on change (0 = disable, 1 = enable)
#define GPINTENB 0x05
#define DEFVALA  0x06   // Default comparison for interrupt on change (interrupts on opposite)
#define DEFVALB  0x07
#define INTCONA  0x08   // Interrupt control (0 = interrupt on change from previous, 1 = interrupt on change from DEFVAL)
#define INTCONB  0x09
#define IOCON    0x0A   // IO Configuration: bank/mirror/seqop/disslw/haen/odr/intpol/notimp
//#define IOCON 0x0B  // same as 0x0A
#define GPPUA    0x0C   // Pull-up resistor (0 = disabled, 1 = enabled)
#define GPPUB    0x0D
#define INFTFA   0x0E   // Interrupt flag (read only) : (0 = no interrupt, 1 = pin caused interrupt)
#define INFTFB   0x0F
#define INTCAPA  0x10   // Interrupt capture (read only) : value of GPIO at time of last interrupt
#define INTCAPB  0x11
#define GPIOA    0x12   // add1 value. Write to change, read to obtain value
#define GPIOB    0x13
#define OLLATA   0x14   // Output latch. Write to latch output.
#define OLLATB   0x15


#define add1 0x20  // MCP23017 is on I2C add1 0x20
#define add2 0x21
#define add3 0x22
#define add4 0x24
#define add5 0x27

bool State[80] = {0};

const unsigned long colors[] = {
  0x000000,0xfafafa,0xfafafa,0xfafafa,0xf8bbd0,0xef5350,0xe57373,0xef9a9a,
  0xfff3e0,0xffa726,0xffb960,0xffcc80,0xffe0b2,0xffee58,0xfff59d,0xfff9c4,
  0xdcedc8,0x8bc34a,0xaed581,0xbfdf9f,0x5ee2b0,0x00ce3c,0x00ba43,0x119c3f,
  0x57ecc1,0x00e864,0x00e05c,0x00d545,0x7afddd,0x00e4c5,0x00e0b2,0x01eec6,
  0x49efef,0x00e7d8,0x00e5d1,0x01efde,0x6addff,0x00dafe,0x01d6ff,0x08acdc,
  0x73cefe,0x0d9bf7,0x148de4,0x2a77c9,0x8693ff,0x2196f3,0x4668f6,0x4153dc,
  0xb095ff,0x8453fd,0x634acd,0x5749c5,0xffb7ff,0xe863fb,0xd655ed,0xd14fe9,
  0xfc99e3,0xe736c2,0xe52fbe,0xe334b6,0xed353e,0xffa726,0xf4df0b,0x8bc34a,
  0x5cd100,0x00d29e,0x2388ff,0x3669fd,0x00b4d0,0x475cdc,0xfafafa,0xfafafa,
  0xf72737,0xd2ea7b,0xc8df10,0x7fe422,0x00c931,0x00d7a6,0x00d8fc,0x0b9bfc,
  0x585cf5,0xac59f0,0xd980dc,0xb8814a,0xff9800,0xabdf22,0x9ee154,0x66bb6a,
  0x3bda47,0x6fdeb9,0x27dbda,0x9cc8fd,0x79b8f7,0xafafef,0xd580eb,0xf74fca,
  0xea8a1f,0xdbdb08,0x9cd60d,0xf3d335,0xc8af41,0x00ca69,0x24d2b0,0x757ebe,
  0x5388db,0xe5c5a6,0xe93b3b,0xf9a2a1,0xed9c65,0xe1ca72,0xb8da78,0x98d52c,
  0x626cbd,0xcac8a0,0x90d4c2,0xceddfe,0xbeccf7,0xfafafa,0xfafafa,0xfafafa,
  0xfe1624,0xcd2724,0x9ccc65,0x009c1b,0xffff00,0xbeb212,0xf5d01d,0xe37829
  };

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
  MidiUSB.flush();
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
  MidiUSB.flush();
}
volatile bool keyPressed1 = false, keyPressed2 = false, keyPressed3 = false;

// set register "reg" on expander to "data"
// for example, IO direction
void expanderWriteBoth (const byte addr, const byte reg, const byte data ) 
{
  Wire.beginTransmission (addr);
  Wire.write (reg);
  Wire.write (data);  // add1 A
  Wire.write (data);  // add1 B
  Wire.endTransmission ();
} // end of expanderWrite

// read a byte from the expander
unsigned int expanderRead (const byte addr, const byte reg) 
{
  Wire.beginTransmission (addr);
  Wire.write (reg);
  Wire.endTransmission ();
  Wire.requestFrom (addr, 1);
  return Wire.read();
} // end of expanderRead

void keypress0()
{
  keyPressed1 = true;
}

void keypress1()
{
  keyPressed2 = true;
}

void keypress2()
{
  keyPressed3 = true;
}

void matrix_ini(const byte addr){
   // expander configuration register
  expanderWriteBoth (addr, IOCON, 0b01100000); // mirror interrupts, disable sequential mode
 
  // enable pull-up on switches
  expanderWriteBoth (addr, GPPUA, 0xFF);   // pull-up resistor for switch - both add1s

  // invert polarity
  expanderWriteBoth (addr, IOPOLA, 0xFF);  // invert polarity of signal - both add1s
  
  // enable all interrupts
  expanderWriteBoth (addr, GPINTENA, 0xFF); // enable interrupts - both add1s
  
  // read from interrupt capture add1s to clear them
  expanderRead (addr, INTCAPA);
  expanderRead (addr, INTCAPB);
}

void setup ()
{
  delay(1000);
  Wire.begin ();  

  //Serial.begin(115200);
  
  matrix_ini(add1);
  matrix_ini(add2);
  matrix_ini(add3);
  matrix_ini(add4);
  matrix_ini(add5);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(30);

  attachInterrupt(2, keypress0, FALLING);
  attachInterrupt(3, keypress1, FALLING);
  attachInterrupt(digitalPinToInterrupt(7), keypress2, FALLING);
}

unsigned int keyValue1 = 0, keyValue2 = 0, keyValue3 = 0, keyValue4 = 0, keyValue5 = 0;

void change_on(byte button, byte change){
  //Serial.println(String(button) + " , " + String(change));
  if(change==1){
    noteOn(0,button,127);
  }
  else{
    noteOff(0,button,0);
  }
}

byte get_index(const byte addr){
  switch(addr){
    case add1:
      return 0;
    case add2:
      return 1;
    case add3:
      return 2;
    case add4:
      return 3;
    case add5:
      return 4;
  }
}

byte get_button_num(const byte button, const byte pad){
  byte good_arr[16]={11,15,10,14,13,9,12,8,4,0,5,1,2,6,3,7};
  if(pad==0 || pad==1){
    return 16*(good_arr[button]/4)+good_arr[button]%4+pad*4;
  }
  else{
    return 64 + 16*(good_arr[button]/4)+good_arr[button]%4+(pad-2)*4;
  }
}

// called from main loop when we know we had an interrupt
void handleKeypress (const byte addr, unsigned int *keyValue)
{
  
  delay (10);  // de-bounce before we re-enable interrupts
  
  // Read add1 values, as required. Note that this re-arms the interrupts.
  if (expanderRead (addr, INFTFA))
    {
    *keyValue &= 0x00FF;
    *keyValue |= expanderRead (addr, INTCAPA) << 8;    // read value at time of interrupt
    }
  if (expanderRead (addr, INFTFB))
    {
    *keyValue &= 0xFF00;
    *keyValue |= expanderRead (addr, INTCAPB);        // add1 B is in low-order byte
    }
}

void led(int i, int velo){
  if(i>111 && i<121){
    leds[i-112]=colors[velo];
  }
  else if(i>95 && i<105){
    leds[113-i]=colors[velo];
  }
  else if(i>79 && i<89){
    leds[i-62]=colors[velo];
  }
  else if(i>63 && i<73){
    leds[99-i]=colors[velo];
  }
  else if(i>47 && i<57){
    leds[i-12]=colors[velo];
  }
}

void set_leds(midiEventPacket_t rx){
  byte comm = rx.header;
  byte sig = rx.byte1;
  byte note = rx.byte2;
  byte velo = rx.byte3;
  led(note,velo);
}

void loop ()
{
  if (keyPressed1){
    keyPressed1 = false;
    handleKeypress(add1, &keyValue1);
    handleKeypress(add2, &keyValue2);
    //Serial.println("Interrupt 1: "+String(keyValue1)+" , "+String(keyValue2));
    for (byte button = 0; button < 16; button++){
      if (keyValue1 & (1 << button)){
        if(State[button]!=1){
          State[button]=1;
          change_on(get_button_num(button,0),1);
        }
      }
      else{
        if(State[button]!=0){
          State[button]=0;
          change_on(get_button_num(button,0),0);
        }
      }
      
      if (keyValue2 & (1 << button)){
        if(State[button+16]!=1){
          State[button+16]=1;
          change_on(get_button_num(button,1),1);
        }
      }
      else{
        if(State[button+16]!=0){
          State[button+16]=0;
          change_on(get_button_num(button,1),0);
        }
      }
    }
  }
  else if(keyPressed2){
    keyPressed2 = false;
    handleKeypress(add3, &keyValue3);
    handleKeypress(add4, &keyValue4);
    //Serial.println("Interrupt 2: "+String(keyValue3)+" , "+String(keyValue4));
    for (byte button = 0; button < 16; button++){
      if (keyValue3 & (1 << button)){
        if(State[button+32]!=1){
          State[button+32]=1;
          change_on(get_button_num(button,2),1);
        }
      }
      else{
        if(State[button+32]!=0){
          State[button+32]=0;
          change_on(get_button_num(button,2),0);
        }
      }
      
      if (keyValue4 & (1 << button)){
        if(State[button+48]!=1){
          State[button+48]=1;
          change_on(get_button_num(button,3),1);
        }
      }
      else{
        if(State[button+48]!=0){
          State[button+48]=0;
          change_on(get_button_num(button,3),0);
        }
      }
    }
  }
  
  else if(keyPressed3){
    keyPressed3 = false;
    handleKeypress(add5, &keyValue5);
    for (byte button = 0; button < 16; button++){
      if (keyValue5 & (1 << button)){
        if(State[button+64]!=1){
          State[button+64]=1;
          leds[button]=colors[(button+1)*7];
          //change_on(get_button_num(button,2),1);
        }
      }
      else{
        if(State[button+64]!=0){
          State[button+64]=0;
          leds[button]=colors[0];
          //change_on(get_button_num(button,2),0);
        }
      }
    }
  }
  midiEventPacket_t rx;
  do {
    rx = MidiUSB.read();
    if (rx.header != 0) {
      set_leds(rx);
    }
  } while (rx.header != 0);
  
  FastLED.show();
}
/*
// Author: Nick Gammon
// Date: 19 February 2011

// Demonstration of an interrupt service routine connected to the MCP23017

#include <Wire.h>
#include "MIDIUSB.h"
#include "FastLED.h"

// MCP23017 registers (everything except direction defaults to 0)

#define IODIRA   0x00   // IO direction  (0 = output, 1 = input (Default))
#define IODIRB   0x01
#define IOPOLA   0x02   // IO polarity   (0 = normal, 1 = inverse)
#define IOPOLB   0x03
#define GPINTENA 0x04   // Interrupt on change (0 = disable, 1 = enable)
#define GPINTENB 0x05
#define DEFVALA  0x06   // Default comparison for interrupt on change (interrupts on opposite)
#define DEFVALB  0x07
#define INTCONA  0x08   // Interrupt control (0 = interrupt on change from previous, 1 = interrupt on change from DEFVAL)
#define INTCONB  0x09
#define IOCON    0x0A   // IO Configuration: bank/mirror/seqop/disslw/haen/odr/intpol/notimp
//#define IOCON 0x0B  // same as 0x0A
#define GPPUA    0x0C   // Pull-up resistor (0 = disabled, 1 = enabled)
#define GPPUB    0x0D
#define INFTFA   0x0E   // Interrupt flag (read only) : (0 = no interrupt, 1 = pin caused interrupt)
#define INFTFB   0x0F
#define INTCAPA  0x10   // Interrupt capture (read only) : value of GPIO at time of last interrupt
#define INTCAPB  0x11
#define GPIOA    0x12   // Port value. Write to change, read to obtain value
#define GPIOB    0x13
#define OLLATA   0x14   // Output latch. Write to latch output.
#define OLLATB   0x15
#define NUM_LEDS 8
#define DATA_PIN 10
CRGB leds[NUM_LEDS];

#define port 0x24  // MCP23017 is on I2C port 0x20

volatile bool keyPressed;
bool State[80] = {0};

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
  MidiUSB.flush();
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
  MidiUSB.flush();
}

// set register "reg" on expander to "data"
// for example, IO direction
void expanderWriteBoth (const byte reg, const byte data ) 
{
  Wire.beginTransmission (port);
  Wire.write (reg);
  Wire.write (data);  // port A
  Wire.write (data);  // port B
  Wire.endTransmission ();
} // end of expanderWrite

// read a byte from the expander
unsigned int expanderRead (const byte reg) 
{
  Wire.beginTransmission (port);
  Wire.write (reg);
  Wire.endTransmission ();
  Wire.requestFrom (port, 1);
  return Wire.read();
} // end of expanderRead

// interrupt service routine, called when pin D2 goes from 1 to 0
void keypress ()
{
  keyPressed = true;   // set flag so main loop knows
}  // end of keypress

void setup ()
{
  delay(800);
  Wire.begin ();  
  Serial.begin (115200); 

  // expander configuration register
  expanderWriteBoth (IOCON, 0b01100000); // mirror interrupts, disable sequential mode
 
  // enable pull-up on switches
  expanderWriteBoth (GPPUA, 0xFF);   // pull-up resistor for switch - both ports

  // invert polarity
  expanderWriteBoth (IOPOLA, 0xFF);  // invert polarity of signal - both ports
  
  // enable all interrupts
  expanderWriteBoth (GPINTENA, 0xFF); // enable interrupts - both ports
  
  // no interrupt yet
  keyPressed = false;

  // read from interrupt capture ports to clear them
  expanderRead (INTCAPA);
  expanderRead (INTCAPB);
  
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(30);
  // pin 19 of MCP23017 is plugged into D2 of the Arduino which is interrupt 0
  attachInterrupt(3, keypress, FALLING);
  
}  // end of setup

// time we turned LED on
unsigned int keyValue = 0;

// called from main loop when we know we had an interrupt
void handleKeypress ()
{
  
  delay (10);  // de-bounce before we re-enable interrupts
  
  keyPressed = false;  // ready for next time through the interrupt service routine
  
  // Read port values, as required. Note that this re-arms the interrupts.
  if (expanderRead (INFTFA))
    {
    keyValue &= 0x00FF;
    keyValue |= expanderRead (INTCAPA) << 8;    // read value at time of interrupt
    }
  if (expanderRead (INFTFB))
    {
    keyValue &= 0xFF00;
    keyValue |= expanderRead (INTCAPB);        // port B is in low-order byte
    }
  
}

void change_on(byte button, byte change){
  //Serial.println(String(button) + " , " + String(change));
  if(change==1){
    noteOn(0,button,127);
    leds[button-83]=0x00d7a6;
  }
  else{
    noteOff(0,button,0);
    leds[button-83]=0x000000;
  }
  FastLED.show();
}

byte get_button_num(const byte button, const byte pad){
  byte good_arr[16]={11,15,10,14,13,9,12,8,4,0,5,1,2,6,3,7};
  if(pad==0 || pad==1){
    return 16*(good_arr[button]/4)+good_arr[button]%4+pad*4;
  }
  else{
    return 64 + 16*(good_arr[button]/4)+good_arr[button]%4+(pad-2)*4;
  }
}

void loop ()
{
  // was there an interrupt?
  if (keyPressed){
    handleKeypress ();
    for (byte button = 0; button < 16; button++){
      if (keyValue & (1 << button)){
        if(State[button+48]!=1){
          State[button+48]=1;
          change_on(get_button_num(button,3),1);
        }
      }
      else{
        if(State[button+48]!=0){
          State[button+48]=0;
          change_on(get_button_num(button,3),0);
        }
      }
    }
  }
  midiEventPacket_t rx;
  do {
    rx = MidiUSB.read();
    if (rx.header != 0) {
      //set_leds(rx);
    }
  } while (rx.header != 0);
}  // end of loop
 */
