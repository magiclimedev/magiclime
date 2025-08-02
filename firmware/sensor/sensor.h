
#include <Arduino.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <Wire.h>  //Two-Wire (I2C)
#include <SPI.h>
//#include <string.h>
//#include <avr/io.h>
#include <OneWire.h>
//#include <DallasTemperature.h>
#include <dht.h>
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

#define EE_SYSBYTE  1023 //General Purpose shared 'global' byte

#define EE_SEQNUM   EE_SYSBYTE-1  //TX sequence # ? - not implemented yet.
// parameter flag bits
#define EE_MAX1     EE_SEQNUM-1  //
#define EE_MIN1     EE_MAX1-2   //2-bytes to cover a2d's 1024 values, msb,lsb
#define EE_MAX2     EE_MIN1-2   //2 bytes 
#define EE_MIN2     EE_MAX2-2   //2-bytes  //not storing these at this time

#define EE_KEY      EE_MIN2-2   //// 16 byte KEY 

//these are stored for each sbn
#define EE_ID       EE_KEY-16   // 6 char ID                0-5
#define EE_INTERVAL EE_ID-6 //  'seconds*8' WDT counter     6
#define EE_HRTBEAT  EE_INTERVAL-1 //'seconds*8*16 wdmHBI    7
#define EE_PPVPWR   EE_HRTBEAT-1 //ppv,power                8
#define EE_HOLDOPT  EE_PPVPWR-1    // Holdoff/Option byte   9
#define EE_NAME     EE_HOLDOPT-1   // Name? why? setup sends it out

#define EE_BLKSIZE  20 //total config bytes per sensor

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
#define wdmPDI  1             //WatchDogMultiplier for Periodic Data Interval
#define defaultINTERVAL 75    //*8 sec *wdmPDI =... 10 min
#define wdmHBI 16             //WatchDogMultiplier for Heartbeat Interval
#define defaultHEARTBEAT 113  //*8 sec *wdmHBI =... 241 min 

const char HB[] PROGMEM = "HEARTBEAT";  
//Sensor Name
const char SNqqqq[] PROGMEM = "????"; 
const char SNM1[] PROGMEM = "BEACON";   //Minus 1    
const char SN00[] PROGMEM = "SB#00";
const char SN01[] PROGMEM = "BUTTON" ;
const char SN02[] PROGMEM = "TILT";
const char SN03[] PROGMEM = "REED";
const char SN04[] PROGMEM = "SHAKE";
const char SN05[] PROGMEM = "MOTION";
const char SN06[] PROGMEM = "KNOCK";
const char SN07[] PROGMEM = "2-BTN";
const char SN08[] PROGMEM = "SB#08";
const char SN09[] PROGMEM = "SB#09";
const char SN10[] PROGMEM = "SB#10";
const char SN11[] PROGMEM = "LIGHT%";
const char SN12[] PROGMEM = "T-H";
const char SN13[] PROGMEM = "T-K";;
const char SN14[] PROGMEM = "T-H-L";
const char SN15[] PROGMEM = "SB#15";
const char SN16[] PROGMEM = "SB#16";
const char SN17[] PROGMEM = "SB#17";
const char SN18[] PROGMEM = "SB#18";
const char SN19[] PROGMEM = "SB#19";
const char SN20[] PROGMEM = "SB#20";
const char SN21[] PROGMEM = "MOT-DOT";
const char SN22[] PROGMEM = "SB#22";

//Sensor DATA
const char FAIL1[] PROGMEM = "---";
const char FAIL2[] PROGMEM = "---|---";
const char SDM1[] PROGMEM = "BEACON";      
const char SD00[] PROGMEM = "?00??";
const char SD01[] PROGMEM = "PUSH" ;
const char SD02[] PROGMEM = "?02??";
const char SD03[] PROGMEM = "?03??";
const char SD04[] PROGMEM = "SHAKE";
const char SD05[] PROGMEM = "MOTION";
const char SD06[] PROGMEM = "KNOCK";
const char SD07[] PROGMEM = "?07??";
const char SD08[] PROGMEM = "?08??";
const char SD09[] PROGMEM = "?09??";
const char SD10[] PROGMEM = "?10??";
const char SD11[] PROGMEM = "?11??";
const char SD12[] PROGMEM = "?12??";
const char SD13[] PROGMEM = "?13??";
const char SD14[] PROGMEM = "?14??";
const char SD15[] PROGMEM = "?15??";
const char SD16[] PROGMEM = "?16??";
const char SD17[] PROGMEM = "?17??";
const char SD18[] PROGMEM = "?18??";
const char SD19[] PROGMEM = "?19??";
const char SD20[] PROGMEM = "?20??";
const char SD21[] PROGMEM = "?21??";
const char SD22[] PROGMEM = "?22??";

const char TILT00[] PROGMEM = "LOW";
const char TILT01[] PROGMEM = "HIGH";
const char TILT10[] PROGMEM = "HORIZ";
const char TILT11[] PROGMEM = "VERT";
const char TILT20[] PROGMEM = "LEFT";
const char TILT21[] PROGMEM = "RIGHT";
const char TILT30[] PROGMEM = "OPEN";
const char TILT31[] PROGMEM = "CLOSE";
const char TILT40[] PROGMEM = "UP";
const char TILT41[] PROGMEM = "DOWN";

const char MOT00[] PROGMEM = "MOTION";
const char MOT10[] PROGMEM = "MOT-LEFT";
const char MOT01[] PROGMEM = "MOT-RIGHT";
const char MOT11[] PROGMEM = "MOT-BOTH";

const char PB00[] PROGMEM = "PUSH";
const char PB01[] PROGMEM = "PUSH-1";
const char PB10[] PROGMEM = "PUSH-2";
const char PB11[] PROGMEM = "PUSH-3";

const char REED0[] PROGMEM = "OPEN";
const char REED1[] PROGMEM = "CLOSED";
const char DHT22a[] PROGMEM = "32.0|"; //DHT22 reads back 0,1,2,3 etc. 
const char DHT22b[] PROGMEM = "33.8|"; //dpending on warmth
const char DHT22c[] PROGMEM = "35.6|"; //which becomes *1.8 +32
const char DHT22d[] PROGMEM = "37.4|"; // so four reasonable possibilities

const char PUR[] PROGMEM = "PUR:";
const char KEY0[] PROGMEM = "thisisamagiclime";
const char idChar[] PROGMEM = "1A2B3C4D5E6F7G8H9JLKMNRSTUVWXYZ";

word MAX1,MIN1,MAX2,MIN2; //for adaptive sensor reference
word CAL_VAL; //trimpot set value

int SBN; //the Sensor Board Number, -1 is 'no board', 0 is grounded, 22 is tied high

byte txPWR; //2-20
byte txPPV; //1-7 (0 held in reserve)
byte txHOLDOFF; //4 high bits of EE_HOLDOPT for resetting HoldOffCtr
byte txSNSROPT; //4 low bits of EE_HOLDOPT for sensor options
long txCOUNT=0;

//ISR things to keep alive
volatile byte HoldOffCtr; //WatchDog - about 8 sec per.
volatile byte wakeWHY;
volatile int wd_COUNTER; //counter of 8-sec. Heart Beats
volatile int wd_TIMER; //either interval or heartbeat depending on sensor
volatile bool flgKEY_GOOD=false;
volatile bool flgHoldOff=false; //for holding off another event trigger
volatile bool HrtBtON;

int wd_INTERVAL; //wdt counts - 8 sec. per
int wd_HEARTBEAT;
//pre-assigning as many as possible...
//char chr8[8]={0}; //general purpose use char buffer
//char chr12[12]={0}; //general purpose use char buffer
//char chr18[18]={0}; //general purpose use char buffer
//char chr20[20]={0}; //general purpose use char buffer
//char chr30[30]={0}; //general purpose use char buffer

//char n2a[8]={0}; //for Number To Ascii use
char txID[8]={0}; //6 char + null 
char txLABEL[20]={0}; //sensor label 10-char max. but suffix can add 10
//char sbnNAME[20]={0}; // snsrnm|suffix 
char sbnDATA[46]={0}; //dlm+10char * 4 
char rxKEY[18]={0}; //16 char + null

//char ssfx[10]={0}; //and this for that suffix by itself
char dataOLD[12]={0}; //for discriminating against redundant TX's
//char msg[64]={0}; //the all-purpose in/out Message string
//char txENbuf[64]={0}; //for encoding/decoding char buffer


bool RF95_UP=false;

const float mV_bit= float(3000.0 / 1024.0); //assuming 3000 mV aRef;
float txBV; //Battery Voltage  

enum TYPE {BEACON=0, EVENT_LOW, EVENT_CHNG, EVENT_RISE, EVENT_FALL, ANALOG, DIGITAL_SPI, DIGITAL_I2C} DATA_TYPE;

bool flgLED_KEY=false;
bool flgEE_ERASED=false;

byte sysBYTE; //from the EE_SYSBYTE

byte SPI_CS; //for soft spi sensor CS pin
byte SPI_SO; //for soft spi sensor SI pin
byte SPI_SI; //for soft spi sensor SO pin
byte SPI_CLK; //for soft spi sensor SCK pin

/*What the heck is the 'flow' here???
 This thing is all about the sensor sitting on it, so... 
  	What is the sensor on that little board? Get a SBN (Sensor Board Number).
  	It needs a unique ID to go with that - the receiver will want to know what
	temperature (or whatever) it's talking about, so make one... maybe.
 		For the 'plug-n-play' board swapping feature to work nicely, the ID 
	for that SBN is added to EEPROM memory as it's made.
	There may be one already there.	In which case, use that. 
    Next... use that SBN to initalize/assign 'sensor specific' things.
	For instance, start 'wire' for I2C devices and set the default label name,
	like 'TILT' for a tilt sensor.
		Side note... that 'TILT' will be sent to the receiver with the ID soon
		to be stored in EEPROM so it can go out in the JSON packet with each data
		message. No	need for the sensor data message to be burdened with a possible
		10 bytes of	redundant characters. The ID will index to it.
  Back on track...
	Get that simple 16 char. encryption key the receiver uses to validate messages
	from YOUR sensors. (There's a little back-n-forth of random keys involved as
	well as the requirement of close proximity to receiver.)
	Store it in EEPROM.
	Now... using that 'RX-KEY', ask the receiver for an update of sensor related
	parameters (because the receiver is where they are stored).
	The first time they will be default values. (Well, actually, if you do not
	change them, they always will be, but anyway... )
	Also, this 'PUR' Parameter Update Request is when that 'TILT' label is sent
	along with the ID.
	
	Use those parameters to set the Periodic Data Interval, Heartbeat Interval,
	TX Power level, Packet Protocol Version, Event Holdoff Counter and the misc.
	options for this sensor.
	
	Enter the Loop. Send some data. Go to sleep. Wait for something to happen.
  
*/