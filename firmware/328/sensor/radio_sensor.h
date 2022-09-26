
#include <Arduino.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <Wire.h>  //Two-Wire (I2C)
#include <SPI.h>

#include <OneWire.h>
#include <DallasTemperature.h>

// Singleton instance of the radio driver
#include <RH_RF95.h>


#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__


#define ONE_WIRE_BUS 4

#define RF95_RST 9 //not used
#define RF95_CS   10
#define RF95_INT  2
#define RF95_FREQ 915.0
RH_RF95 rf95(RF95_CS, RF95_INT);

#define EE_SYSBYTE  1023 //System Bits - misc flag/status/mode bits
//bit#0: reset max's
//bit#1: reset min's
//bit#2: erase this ID (make new one) 
//bit#3: erase all eeprom
//bit#4: Stream data mode/count
//bit#5: 
//bit#6: 
//bit#7: 

#define EE_SEQNUM   EE_SYSBYTE-1  //TX sequence # for detecting missing data - not implemented yet.
#define EE_INTERVAL EE_SEQNUM-1 //  'seconds/16' WDT counter
#define EE_HRTBEAT EE_INTERVAL-1 //'seconds/64
#define EE_POWER    EE_HRTBEAT-1 //1 byte for tx power 0-9
#define EE_RefMAX1  EE_POWER-1     
#define EE_RefMIN1  EE_RefMAX1-2  //2-bytes to cover a2d's 1024 values, msb,lsb
#define EE_RefMAX2  EE_RefMIN1-2  //2 bytes 
#define EE_RefMIN2  EE_RefMAX2-2  //2-bytes 
#define EE_KEY      EE_RefMIN2-2  //2-bytes 
#define EE_ID   EE_KEY-33     //up to 32 (+null) byte KEY
//bottomless assignment for EE-ID's as they are 'per sensor' unique persistant ID's

#define pinRF95_INT  2
#define pinEVENT  3
#define pinSWITCH 4
#define pinPAIR_SW  7
#define pinPAIR_LED 6
#define pinLED    8
#define pinBOOST  9 
#define pinRF95_CS  10
#define pinMOSI   11
#define pinMISO   12
#define pinSCK    13
#define pinBV   7
#define pinTrimPot  6
#define pinSBID  A6
#define pinSDA  A4
#define pinSCL  A5

byte sysBYTE; //updated with param_REQ, reset to 0 when done
//bit#0: erase this ID (make new one)
//bit#1: reset max's (to 0)
//bit#2: reset min's (to 1023) 
//bit#3: 
//bit#4: Stream data count B4 //assume you want at least 16
//bit#5: Stream data count B5 // so, top four bits get you
//bit#6: Stream data count B6 // multiples of 16 up to 256
//bit#7: Stream data count B7

//sendig of params is an 'on request' thing.
//request format 'IDxxxx:param0' 0 to make six char and room for other param categories.
//(length of each half is equal)

word MAX1,MIN1,MAX2,MIN2;
word CAL_VAL; //trimpot set value

byte SBN;
 
int txPWR; //1-10 default - updateable by gateway?
int txINTERVAL; //wdt counts - 8 sec. per
int txHRTBEAT;

bool HrtBtON;
volatile int txCOUNTER; //counter of 8-sec. Heart Beats
volatile int txTIMER; //either interval or heartbeat depending on sensor

bool RF95_UP=false;

const float mV_bit= float(3000.0 / 1024.0); //assuming 3000 mV aRef;
float dBV; //Battery Voltage  

enum TYPE {BEACON=0, EVENT_LOW, EVENT_CHNG, EVENT_RISE, EVENT_FALL, ANALOG, DIGITAL_SPI, DIGITAL_I2C} DATA_TYPE;

byte sendDATA=0;
  
String TX_ID((char *)0); 
String TX_KEY((char *)0); 
String sSTR8((char *)0);
String sSTR18((char *)0);
String sSTR34((char *)0);
String sMSG((char *)0);
String sRET((char *)0);

byte debugON;

const byte keyRSS=80;  
