
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

#define EE_SEQNUM   EE_OPTBYTE-1  //TX sequence # for detecting missing data - not implemented yet.
// parameter flag bits
#define EE_MAX1     EE_SEQNUM-1  //an idea in waiting - some other packet format  
#define EE_MIN1     EE_MAX1-2   //2-bytes to cover a2d's 1024 values, msb,lsb
#define EE_MAX2     EE_MIN1-2   //2 bytes 
#define EE_MIN2     EE_MAX2-2   //2-bytes  //not storing these at this time

#define EE_KEY      EE_MIN2-2   //2-bytes 

//these are stored for each sbn
#define EE_ID       EE_KEY-16   // 16 byte KEY
#define EE_INTERVAL EE_ID-6 //  'seconds/16' WDT counter    1
#define EE_HRTBEAT  EE_INTERVAL-1 //'seconds/64             1
#define EE_POWER    EE_HRTBEAT-1 //1 byte for tx power 0-9  1
#define EE_NAME     EE_POWER-1     // 6+1+1+1+10 = 19 per sensor

#define EE_BLKSIZE  20 //plus one more, nice round number.

#define pinRF95_INT  2
#define pinEVENT  3
#define pinSWITCH 4
#define pinBOOT_SW  7
#define pinLED_BOOT 6
#define pinLED_TX   8
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

#define rssOFFSET 140
#define keyRSS 80

//timer parameter values limited to 1 byte (255 max).
//Multiplier needed if longer times than ( 8 sec.* 255) wanted
#define wdmTXI  1             //WatchDogMultiplier for Data Interval
#define defaultINTERVAL 75    //*8 sec *wdmTXI =... 10 min
#define wdmHBI 16             //WatchDogMultiplier for Heartbeat Interval
#define defaultHEARTBEAT 113  //*8 sec *wdmHBI =... 241 min 

byte optBYTE; 

word MAX1,MIN1,MAX2,MIN2; //for adaptive sensor reference
word CAL_VAL; //trimpot set value

volatile int SBN; //the Sensor Board Number, -1 is 'no board', 0 is grounded, 22 is tied high
char SNM[12]; //sensor name 10-char max.
 
int txPWR; //2-20
int wd_INTERVAL; //wdt counts - 8 sec. per
int wd_HEARTBEAT;
volatile int wd_COUNTER; //counter of 8-sec. Heart Beats
volatile int wd_TIMER; //either interval or heartbeat depending on sensor

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
bool flgLED_KEY=false;
bool flgEE_ERASED=false;
volatile bool flgKEY_GOOD=false;
 
