/*
	This is code for MagicLime sensors.

	MagicLime invests time and resources providing this open source code,
	please support ThingBits and open-source hardware by purchasing products
	from MagicLime!

	Written by MagicLime Inc (http://www.MagicLime.com)
	author: Marlyn Anderson, maker of things, writer of code, not a 'programmer'.
	You may read that as 'many improvements are possible'. :-) 
	MIT license, all text above must be included in any redistribution
		https://github.com/RobTillaart/DHTlib
		
		
		
The flow...
power up - request a key from 'the boss' receiver
 - if rss is high enough, store it - use it
look for data - save buffer
use key to decode buffer
 - if data/format is familiar - not weird,
	send buffer right back out
which means...
make a key to send to receiver along with 'rpID'
encode a message of [key ,rpID] using hard-code key - send it out.
receiver sees big RSS, decodes using hard-code key, sees rpID a valid ID,

Crap - Asim wants to know if the sensor data packet came from the repeater or not.
So... to not violate the 'send the minimum bytes needed' rule - I need to convert a byte from 
'something' to 'something more' - or something like that.
Not only that, but in addition to decoding for 'valid ID' check, I need to take the time
to re-encode the data packet.
Here's the plan...
  The PPV byte isn't very big - goes from 1 to 7(maybe) and it's the first one, easy to find.
So...
  Add 17 to it ('0'->'A'), re-encode packet and send it out.
  A txPPV=='1' -> 'B'
So then...
  receiver code sees PPV >='A', subtracts 17 and changes the "source":"tx" to "source":"rp".
*/
#include <Arduino.h>
#include <EEPROM.h>
// Singleton instance of the radio driver
#include <RH_RF95.h>
#define RF95_RST 9 //not used
#define RF95_CS  10
#define RF95_INT  2
#define RF95_FREQ   915.0
RH_RF95 rf95(RF95_CS, RF95_INT);

#define EE_SYSBYTE  1023 //System byte, with txPWR in top 5 bits
#define EE_KEY      EE_SYSBYTE-1  //KEY is 16 - no string null 
#define rssOFFSET 140

const static char VER[] = "repeat";
const static char rpID[]="repeat"; 

static char rxBUF[80]={0}; //for the rx buf
static char rxKEY[18]={0}; //16 char + null
const char KEY0[] PROGMEM = "thisisamagiclime";
const char Krpver[] PROGMEM = "\"RP_VER\":\"";
const char chrQUOTE[] PROGMEM = "\"";
const char KsrcRP[] PROGMEM = "{\"source\":\"rp\",";

byte rxRSS;
byte rxLEN;
byte txPWR=13;
byte modeRDM; //'Raw Data Mode' what to spit out, raw data or decoded
byte sysByte; //stores modeRDM in eeprom bits 1,0
bool flgKEY_GOOD;
#define pinLED 5 //tacked on LED at D5 for visual confirmation of up-n-running

//*****************************************
void setup () {
	Serial.begin(57600); delay(10);
  json_VER(VER);
	//Serial.println("");Serial.println(VER);
  pinMode(pinLED, OUTPUT); digitalWrite(pinLED, HIGH); //off

  sysByte=EEPROM.read(EE_SYSBYTE);
  byte pwr=sysByte & 0x1F; //bottom 5 bits for power = 5-23
  if ((pwr<5) || (pwr>23)) { pwr=txPWR; //not good? set to init default
    sysByte=((sysByte & 0xE0)| txPWR); 
    EEPROM.write(EE_SYSBYTE,sysByte);
  } 
  txPWR=pwr; //assign to program use

	if (rf95.init()) {
    LED_blink(3);
		rf95.setFrequency(RF95_FREQ); delay(5);
		rf95.setTxPower(5, false);  delay(5);
		key_REQUEST(rxKEY,rpID); //returns rxKEY - if RSS was high enough at receiver
		Serial.print(F("rxKEY: "));Serial.println(rxKEY);Serial.flush();
		if (rxKEY[0]!=0) {
			if (key_VALIDATE(rxKEY)==true) { key_EE_SET(rxKEY); }
		}

    rxKEY[0]=0;
    key_EE_GET(rxKEY);
    flgKEY_GOOD=key_VALIDATE(rxKEY);
    Serial.print(F("ee-rxKEY: "));Serial.print(rxKEY);
    Serial.print(" is "); Serial.println(flgKEY_GOOD); 
    Serial.println("( but may not be the right one )"); Serial.flush();
    while (flgKEY_GOOD) {
      rxRSS=rxBUF_CHECK(); //any message will be in rxBUF with rxLEN
      if (rxRSS>0) { 
        //Serial.print(F("RSS="));Serial.print(rxRSS);Serial.print(F(", "));
        //print_HEX(rxBUF,rxLEN);
        if (msgValidate(rxBUF,rxLEN,rxKEY)) {
          //Serial.print(F("msgValidate=true "));
        rf95.send(rxBUF,rxLEN); rf95.waitPacketSent();
        LED_blink(1);}
      }
    }
	Serial.println("key problem - going to loop..."); Serial.flush();
  }
}  
//*****************************************

//*****************************************
void loop(){
	rxBUF_CHECK();
	if (rxRSS>0) {print_HEX(rxBUF,rxLEN);}
} //  End Of LOOP 

//*****************************************
// get RF95 buffer - return rss>0 if message  ***********
byte rxBUF_CHECK() { //********* get RF Messages - return RSS=0 for no rx
  byte rss=0;
  if (rf95.available()) { // Should be a message for us now 
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      memcpy(rxBUF,buf,len);
      rxBUF[len]=0;
      rxLEN=len;
      rss=rf95.lastRssi()+rssOFFSET;  
    }
  }
  return rss;
}

//*****************************************
bool msgValidate(char *msgBUF,byte *msgLEN,char *key) { bool ret=false;
	//char msg[80]; //isn't 80 a bit much?
  char msg[12]; 
  //but for repeat purposes - maybe not the whole thing?
  //maybe just the first part we need for this? - 9 char?
  //rx_DECODE(msg,msgBUF,msgLEN, key); //decode using rx KEY
  rx_DECODE(msg,msgBUF,10, key); //decode using rx KEY
  //print_CHR(msg,msgLEN);
	if (msg[0] !=0) {
    if ((msg[1]=='|') && (msg[8]=='|')) { //p|ididid|
      char msgID[10];
      mySubStr(msgID,msg,2,6); //pluck out the ID
      //Serial.print(F("msgID= "));Serial.println(msgID);
      ret= id_VALIDATE(msgID);
      
      if (ret==true) { //add 17 (so '0'->'A') to msg[0] to signify to receiver that this comes from repeater?
        msg[0]=msg[0]+17;  //now re-encode msg[0]
        msgBUF[0]=byte((byte(msg[0])^byte(key[0])));
      }
    }
    if (ret==false) { //not a good data packet? Then maybe it's a ...
      if (msg[0]=='P' && msg[1]=='U' && msg[2]=='R' && msg[3]==':') { ret=true; }
      else if (msg[0]=='P' && msg[1]=='A' && msg[2]=='K' && msg[3]==':') { ret=true; }
      else if (msg[0]=='V' && msg[1]=='E' && msg[2]=='R' && msg[3]==':') { ret=true; }
      //do I also need to mark this as a 'repeat' in some way"
      //how? maybe change upper case 'P' to lower case 'p' ?
      //so... receiver doesn't 'double respond'?
      //or... just have receiver 'not look for a bit' to not see 2nd message?
    }
  }
  return ret;
}

//*****************************************
bool id_VALIDATE(char *id) { //check EEPROM for proper character range
  for (byte i=0;i<6;i++) { 
    if ( ((id[i]<'2')||(id[i]>'Z')) || ((id[i]>'9')&&(id[i]<'A')) ) { return false; }
  }
  return true;
}

//*****************************************
bool init_RF95(int txpwr)  { bool ret=false;
  byte timeout=0;
  while (!rf95.init() && (timeout<20)) { delay(10); timeout++; }
  if (timeout!=20) {
    rf95.setFrequency(RF95_FREQ); delay(5);
    if (txpwr<5) {txpwr=5;}
    if (txpwr>23) {txpwr=23;} 
    rf95.setTxPower(txpwr, false);  delay(5);
    ret=true;
  }
	return ret;
}

//*****************************************
char *rx_DECODE(char *mOUT,char *rxBUF, byte rxLEN, char *key) {char *ret=mOUT; 
	byte i; byte k=0;
	byte keyLEN=strlen(key); 
	for (i=0;i<rxLEN;i++) {
		if (k==keyLEN) {k=0;}
		mOUT[i]=byte((byte(rxBUF[i])^byte(key[k]))); k++;
	}
	mOUT[i]=0;
	return ret;
}

//*****************************************
char *tx_ENCODE(char *enBuf, char *msgIN, byte msgLEN, char *key) { char *ret=enBuf;
  byte k=0;//random(0,8); //0-7 (8 numbers)
  byte keyLEN=strlen(key);  
  for (byte i=0;i<msgLEN;i++) {
    if (k==keyLEN) {k=0;}
    enBuf[i]=byte((byte(msgIN[i])^byte(key[k])));
    k++;
  }
  return ret;
} 

//*****************************************
void print_HEX(char *buf,byte len) { byte i;
	Serial.print(len);Serial.print(F(": "));
	for (i=0;i<(len-1);i++) {Serial.print(byte(buf[i]),HEX);Serial.print(F(" "));}
	Serial.println(byte(buf[len-1]),HEX);Serial.flush();
}
//*****************************************
void print_CHR(char *buf,byte len) { byte i;
	Serial.print(len);Serial.print(F(": "));
	for (i=0;i<(len-1);i++) {Serial.print( buf[i]);}
	Serial.println(buf[len-1]);Serial.flush();
}

//*****************************************
char *key_REQUEST(char *keyREQ, char* TxId) { char *ret=keyREQ;
	char newK[20]; key_NEW(newK);
	key_TXID_SEND(TxId,newK);
	char MSG[70];
	rx_LOOK(MSG,newK,30);
	if (MSG[0] !=0) {
	if (MSG[6]==':') { //just a little more validate
		char myID[10];
		mySubStr(myID,MSG,0,6); //pluck out the ID
		if (strcmp(myID,TxId)==0) { //just a little validate
		mySubStr(keyREQ,MSG,7,16);} //16 char key returned
	}
	}
	return ret;
}

//*****************************************
char *rx_LOOK(char *mOUT, char *rxkey, byte ctr) { char *ret=mOUT;
  byte timeout=0; mOUT[0]=0;
  while (!rf95.available() && timeout<ctr) { delay(10); timeout++; }
  if (timeout<=ctr) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {rx_DECODE(mOUT,buf,len,rxkey); }
  }
  return ret;
}

//*****************************************
void key_TXID_SEND(char *txid, char* keyTEMP) {
  char keyML[20]; char idKeyML[30];
  strlcpy_P(keyML,KEY0,20); //"thisisamagiclime" from PROGMEM
  strcpy(idKeyML,"!"); strcat(idKeyML,txid); strcat(idKeyML,"!"); strcat(idKeyML,keyTEMP);
  msg_SEND(idKeyML,keyML,5);
}

//*****************************************
void msg_SEND(char *msg, char *key, int pwr) { 
  byte txLEN=strlen(msg);
  char txENbuf[70]; tx_ENCODE(txENbuf,msg,txLEN,key);
  rf95.send(txENbuf,txLEN); rf95.waitPacketSent();
}

//*****************************************
bool key_VALIDATE(char *key2VAL) {
	byte lenKEY=strlen(key2VAL);
	boolean ret=true;
	if (lenKEY<16) {ret=false;}
	for (byte i=0;i<lenKEY;i++) {
	if ((key2VAL[i]<36) || (key2VAL[i]>126)|| (lenKEY<4)) { ret=false; break; }
	}
	return ret;
}

//*****************************************
char *key_EE_GET(char *keyOUT) { char *ret=keyOUT;
	byte i=0;
	for (i=0;i<16;i++){ keyOUT[i]=EEPROM.read(EE_KEY-i); }
	keyOUT[16]=0; //...and terminate
	return ret;
}

//*****************************************
void key_EE_SET(char *key) {
	byte lenKEY=strlen(key);
	for (byte i=0;i<lenKEY;i++) {EEPROM.write(EE_KEY-i,key[i]);}
}

//*****************************************
char *key_NEW(char *key) { char *ret=key;
	randomSeed(analogRead(1));
	for (byte i=0;i<16;i++) { key[i]=random(34,126); }
	key[16]=0;
	return ret;
}


//*****************************************
char *mySubStr(char *SSout, char* SSin,byte from,byte len) { char *ret=SSout;
	byte p=0;
	for (byte i=from;i<(from+len);i++) {SSout[p]=SSin[i]; p++;}
	SSout[p]=0;
	return ret;
}

//*****************************************
int myChrPos(char *cstr, char* cfind) {
	byte pos;
	for (pos=0;pos<strlen(cstr);pos++) {
	if (byte (cstr[pos])== byte (cfind)) {return pos;}
	 }
	return -1;
}

//*****************************************
void LED_blink(byte n) {
  for(byte b=n;b>0;b--){
    digitalWrite(pinLED, LOW); //on
    delay(200);
	  digitalWrite(pinLED, HIGH); //off
    delay(200);
  }
}

//**********************************************************************
void json_VER(char *ver) { char jsnVER[24];
  strlcpy_P(jsnVER,Krpver,12); //"\"RP_VER\":\""
  strcat(jsnVER,ver); strcat_P(jsnVER,chrQUOTE);
  json_RP(jsnVER);
}


//**********************************************************************
void json_RP(char *info) {  char src[16]; 
  strlcpy_P(src,KsrcRP,18); Serial.print(src); //Serial.print(F("{\"source\":\"rp\","));
  Serial.print(info);
  Serial.println(F("}")); Serial.flush();
}
