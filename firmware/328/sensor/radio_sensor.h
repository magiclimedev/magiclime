
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

#define EE_OPTBYTE  1023 //Sensor Option flag/status/mode bits
//example use - photocell as % of previously observed max/min
//bit#0: reset max's (ex. photocell as % of previously observed max/min
//bit#1: reset min's
//bit#2: erase this ID (make new one) 
//bit#3: erase all eeprom
//bit#4: Stream data mode/count
//bit#5: 
//bit#6: 
//bit#7: 
#define EE_SEQNUM   EE_OPTBYTE-1  //TX sequence # for detecting missing data - not implemented yet.
// parameter flag bits
#define EE_MAX1     EE_SEQNUM-1  //an idea in waiting - some other packet format  
#define EE_MIN1     EE_MAX1-2   //2-bytes to cover a2d's 1024 values, msb,lsb
#define EE_MAX2     EE_MIN1-2   //2 bytes 
#define EE_MIN2     EE_MAX2-2   //2-bytes  //not storing these at this time
#define EE_KEY      EE_MIN2-2   //2-bytes 
//these are stored for each sbn, 22*22 = 484 bytes - room for more!
#define EE_ID       EE_KEY-16   // 16 byte KEY
#define EE_INTERVAL EE_ID-6 //  'seconds/16' WDT counter    1
#define EE_HRTBEAT  EE_INTERVAL-1 //'seconds/64             1
#define EE_POWER    EE_HRTBEAT-1 //1 byte for tx power 0-9  1
#define EE_NAME     EE_POWER-1     // 6+1+1+1+10 = 19 per sensor
#define EE_BLKSIZE  20 //plus one more for nice number

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

byte optBYTE; 
//sending of sensor parameters is an 'on request' thing.
//request format 'IDxxxx:param0' 0 to make six char and room for other param categories.
//(length of each half is equal)

word MAX1,MIN1,MAX2,MIN2; //for adaptive sensor reference
word CAL_VAL; //trimpot set value

volatile int SBN; //the Sensor Board Number, -1 is 'no board', 0 is grounded, 22 is tied high
char SNM[20]; //sensor name
 
int txPWR; //1-10 default - updateable by gateway?
int txINTERVAL; //wdt counts - 8 sec. per
int txHRTBEAT;
volatile int txCOUNTER; //counter of 8-sec. Heart Beats
volatile int txTIMER; //either interval or heartbeat depending on sensor

char txID[8]; //6 char + null 
char rxKEY[18]; //16 char + null 
char txDATA[20]; //should be plenty

bool RF95_UP=false;

const float mV_bit= float(3000.0 / 1024.0); //assuming 3000 mV aRef;
float txBV; //Battery Voltage  

enum TYPE {BEACON=0, EVENT_LOW, EVENT_CHNG, EVENT_RISE, EVENT_FALL, ANALOG, DIGITAL_SPI, DIGITAL_I2C} DATA_TYPE;
bool HrtBtON;
volatile byte wakeWHY;

char dataOLD[12]; //for discriminating against redundent TX's

const byte rssOFFSET=130;
const byte keyRSS=80;  
