//List of Things It Requires/Expects/Does...
// Toggles LED every sec if '!' is not seen on UART for ... 10 sec.? (Is Host OK?)
// See Debug Veboseness - send 'd1' to set DebugON to '1', 'd2' for 2.
// Data decoding is total hack - lots of exceptions, but it's fast with data.
//
 

const static char VER[] = "radio_hub_220704";

#include <EEPROM.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

// Singleton instance of the radio driver
#include <RH_RF95.h>
#define RF95_RST 9 //not used
#define RF95_CS  10
#define RF95_INT  2
#define RF95_FREQ   915.0
RH_RF95 rf95(RF95_CS, RF95_INT);

#define EE_SYSBYTE  1023 //System Bits - misc flag/status/mode bits
//bit#0: erase the current ID (so it makes a new one on boot)
//bit#1: reset max's (to 0)
//bit#2: reset min's (to 1023) 
//bit#3: Stream data count MSB (streaming is to be a non-repeating one-time thing)
//bit#4: Stream data count LSB (where it returns to regular interval afterwords)
//bit#5: 
//bit#6: 
//bit#7:
#define EE_KEY            EE_SYSBYTE-1   //1 byte SEQNUM
#define EE_PARAM_ID       EE_KEY-33     //KEY up to 32 (+null)
#define EE_PARAM_INTERVAL EE_PARAM_ID-6 //'seconds/16' 255=68 min.
#define EE_PARAM_HRTBEAT  EE_INTERVAL-1 //'seconds/64' 255=272 min.
#define EE_PARAM_POWER    EE_PARAM_HRTBEAT-1  //1-10  (2-20dB in sensor).
#define EE_PARAM_SYSBYTE  EE_PARAM_POWER-1  // + this one = 10 bytes per ID,
 
//the sending of parameters is an 'upon request' thing.
//'request from sensor' format, 'IDIDID:param0' (where IDIDID is sensor ID).

//To set parameter values in EEPROM (so they are ready-to-go upon request),
// send this program a 'p:IDIDID:iii:hhh:p:s', where...
// 'IDIDID' is the sensor ID,
// 'iii' is data Interval count (16 sec. per, 255 max), (('0' might make it TX every 8 sec.?))
// 'hhh' is Heartbeat interval count (64 sec. per, 255 max),
// 'p' is TX Power ( 1-10)
// 's' is the 'System byte' - various flag bits, the exact meanings of which are tbd.

word WDTcounter=10; //start out flashing
byte LED_green=16; //'A2'
byte LED_red=17; //'A3'

boolean flgShowChar;
boolean flgShowHex;

//here's the type look-up table...
const char T0[] PROGMEM = "Beacon"; 
const char T1[] PROGMEM = "Button";  
const char T2[] PROGMEM = "Tilt"; 
const char T3[] PROGMEM = "Reed";
const char T4[] PROGMEM = "Shake";
const char T5[] PROGMEM = "Motion";
const char T6[] PROGMEM = "Knock";
const char T7[] PROGMEM = "Sensor7";
const char T8[] PROGMEM = "Sensor8"; 
const char T9[] PROGMEM = "Sensor9"; 
const char T10[] PROGMEM = "Temp"; 
const char T11[] PROGMEM = "Light";
const char T12[] PROGMEM = "T,RH";
const char T13[] PROGMEM = "Sensor13";
const char T14[] PROGMEM = "Sensor14";
const char T15[] PROGMEM = "Sensor15";
const char T16[] PROGMEM = "Sensor16";
const char T17[] PROGMEM = "Sensor17";
const char T18[] PROGMEM = "Sensor18";
const char T19[] PROGMEM = "Sensor19";
const char T20[] PROGMEM = "Sensor20";
const char T21[] PROGMEM = "Sensor21";
const char T22[] PROGMEM = "Sensor22";
const char T23[] PROGMEM = "Sensor23";
const char T24[] PROGMEM = "Sensor24";
const char T25[] PROGMEM = "Sensor25";

PGM_P const table_T[] PROGMEM ={T0,T1,T2,T3,T4,T5,
  T6,T7,T8,T9,T10,T11,T12,T13,T14,T15,T16,
  T17,T18,T19,T20,T21,T22,T23,T24,T25};

char rxBuf[4] [64]={0}; //for the rx buf
byte rxBufLen[4]={0}; //for the length
byte rxBufPtr=0;
int rxRSS[4]={0};

String keyPUBLIC((char *)0);
String sID((char *)0);
String sKEY((char *)0);
String sMSG((char *)0);
String sRET((char *)0);
String sINFO((char *)0); 

byte debugON;
const byte keyRSS=80;
 
//**********************************************************************
void setup() {  debugON=1;//1;
  pinMode(LED_red,OUTPUT);
  digitalWrite(LED_red,LOW); //on
  pinMode(LED_green,OUTPUT);
  digitalWrite(LED_green,HIGH); //off
  
  keyPUBLIC.reserve(34);
  sID.reserve(8);
  sKEY.reserve(34);
  sMSG.reserve(64);
  sRET.reserve(64);
  sINFO.reserve(64);
  
  while (!Serial);
  Serial.begin(57600);
  Serial.print(F("INFO: radio_hub, "));Serial.flush();
  
  if (rf95.init()) {
    rf95.setFrequency(RF95_FREQ); 
    Serial.println(F("rf95.init OK")); Serial.flush(); }
  else { Serial.println(F("rf95.init FAIL"));Serial.flush(); while (1); }

  //Serial.println("RFM95 register settings...");
  //rf95.printRegisters();
  flgShowChar=false;
  flgShowHex=false;
  
  //key_EE_ERASE();
  //param_EE_ERASE();
  keyPUBLIC=key_EE_GET();
  if (key_VALIDATE(keyPUBLIC)==false) { //bad key? - make a new one?
    key_EE_MAKE(16); keyPUBLIC=key_EE_GET(); }
  Serial.print(F("INFO:KEY,key:"));Serial.println(keyPUBLIC); 
   
  rxBufLen[0]=0;
  
  //watchdog timer - 1 sec
  cli(); wdt_reset();
  WDTCSR |= B00011000; //bits 3,4 set up for clocking in timer
  //WDTCSR = B01100001; //9=8sec
  WDTCSR = B01000110; //6=1sec
  sei(); //watchdog timer - 1 sec
  digitalWrite(LED_red,HIGH); //off
  
} // *************************************End Of SETUP *****
// *********************************************************

// *********************************************************
void loop(){ 
  CheckPCbuf();
  if (CheckRXbuf()==true) { //caches up to 4 messages
    ProcessRXbuf(); 
  }
} // ************************************* End Of LOOP *****
// *********************************************************

// *********************************************************
void ProcessRXbuf() { //rxBuf[0-3]
  byte ptr=rxBufPtr; //should be on last used 
  byte bufCntr=0;     //but don't trust it - look for length>0
  while ((rxBufLen[ptr]==0) && (bufCntr<4)) {
    ptr++; if (ptr>3) {ptr=0;}
    bufCntr++;  //rxBufPtr is at last-processed number
  }
  //Serial.print(F("ptr="));Serial.println(ptr); Serial.flush();
  if (bufCntr==4) {return;} //SO... nothing to do?
  
  if (debugON>0) {Serial.println(""); Serial.print(F("RSS=")); Serial.print(rxRSS[ptr]); Serial.print(F(", "));}
  //if (debugON>0) {Serial.print(F("LEN=")); Serial.print(rxBufLen[ptr]); Serial.print(F(", "));}
  if (debugON>0) {print_CHR(rxBuf[ptr],rxBufLen[ptr]);}
  if (debugON>1) {print_HEX(rxBuf[ptr],rxBufLen[ptr]);}
   
  bool flgIsData=true;
  if (rxRSS[ptr]>=keyRSS) { //first requirement
    sMSG=pair_VALIDATE(rxBuf[ptr],rxBufLen[ptr]); //!TXID!KEY -> TXID:KEY
      if (debugON>1) {Serial.print(F("IdKey: "));Serial.println(sMSG);}
    if (sMSG!="") {
      sID=parse_TXID(sMSG);
        if (debugON>1) {Serial.print(F("INFO:ID,ID:"));Serial.print(sID);}
      sKEY=parse_TXKEY(sMSG);
        if (debugON>1) {Serial.print(F(",KEY:"));Serial.println(sKEY);}
      key_SEND(sID,sKEY,keyPUBLIC);
      flgIsData=false;
    }
  } //******************
 
  if (flgIsData==true) { //was not a KEY thing, but what about 'INFO' ?
    sMSG=msg_GET(rxBuf[ptr],rxBufLen[ptr],keyPUBLIC);
      if (debugON>1) {Serial.print(F("sMSG: "));Serial.println(sMSG); Serial.flush(); }   
    if (sMSG!=""){
      sINFO=isINFO(sMSG); //strips off the 'INFO: ', is "" if not INFO.
        if (debugON>1) {Serial.print(F("sINFO: ")); Serial.println(sINFO); Serial.flush();}
      
      if (sINFO!="") { //is this 'INFO' a request for parameters?
        if (param_ReqChk(sINFO)==false) { //not a param request? Just pass on the INFO
          Serial.print(F("INFO:"));Serial.println(sINFO); Serial.flush();} //INFO: ':param0' in 'ididid:param0'  ?
        }
      else { //not INFO, must be DATA
          if (debugON>1) {Serial.print(sMSG); Serial.print(F(" is DATA. "));} //was not a KEY OR INFO thing, should be data
        byte msgLen=sMSG.length();
        char dBuf[65]; sMSG.toCharArray(dBuf,msgLen+1); //Decoded BUFfer
        if (debugON>1) {print_CHR(dBuf,msgLen);}
        //'data validation' filter uses first two characters...
        if ((dBuf[0]>='0') && (dBuf[0]<='9') && (dBuf[1]=='|')) { //protocol # validate
          char protocol=dBuf[0];
          byte pNum; //json Number of Pairs 
          char jp[6][20]; //Json Pairs, 6 'cause it should be mostly enough.
          byte jpx; //json Pair # indeX pointer
          switch (protocol) { 
            case '1': {   pNum=5; // 5 pairs in this protocol
              //compose rss pair and first-half of data pairs, 
              strcpy(jp[0],"{\"rss\":"); char chr[10]; itoa(rxRSS[ptr],chr,10);
              strcat(jp[0],chr); strcat(jp[0],","); //integer
              strcpy(jp[1],"\"id\":\"");     //data is string (6) , 20-5=15 left for data object
              strcpy(jp[2],"\"type\":\"");   //data is string, 20-7, the verbose one 
              strcpy(jp[3],"\"bat\":");      //data is number (3), 20-6=14
              strcpy(jp[4],"\"data\":\"");   //data is string, 20-7
              
              byte bx=2; //Buf indeX start: [0] is protocol, [1] is first delimiter 
              char ps[20];  //Pair String accumulator
              byte psx=0; //ps's indeX
              jpx=1; //json Pair # indeX, '0' is rss - already done.                
              while (bx<rxBufLen[ptr]) {
                if ((dBuf[bx]=='|')||(bx==(rxBufLen[ptr]-1))) { // hit next delim?, got a pair
                  ps[psx]=0; //null term
                  switch (jpx) {
                    case 1: {strcat(jp[1],ps); } break; 
                    case 2: {strcat_P(jp[2], (char*)pgm_read_word(&(table_T[atoi(ps)]))); } break; 
                    case 3: {strcat(jp[3],ps); } break; 
                    case 4: {strcat(jp[4],ps); } break; 
                  }
                  psx=0;  jpx++; //done with this 'jpx', prep for next pair
                }
                else { ps[psx]=dBuf[bx]; psx++; } //build up pair # jpx's data
                bx++; //next char in dBuf
              }
              strcat(jp[1],"\","); //data is string
              strcat(jp[2],"\","); //data is string
              strcat(jp[3],",");   //data is number
              strcat(jp[4],"\"}"); //data is string 
              json_PRINT(jp,pNum); //move this down if data validation works well
            } //End of case:'1'
          } //EndOfSwtchProtocol
        }//validate protocol format
      } //else 'not INFO'
    } //end of rxDECODED OK
  }//end of flgIsData
  rxBufLen[ptr]=0; //reset for re-use
  rxBufPtr=ptr; //and this is now the last-processed number
} //end of ProcessRXbuf()

//*****************************************
boolean CheckRXbuf() {//********* get RF Messages
  boolean ret=false; //max of 4 in a row, then bail out and process them
  //otherwise, bail out when NOT rf95.available
  byte ptr=rxBufPtr; byte bufCntr=0;

  while (rf95.available()) { // Should be a message for us now 
    while ((rxBufLen[ptr]!=0)&&(bufCntr<4)) {
      ptr++; if (ptr>3) {ptr=0;}
      bufCntr++;} //avail location 
    //Serial.print(F("ptr="));Serial.println(ptr); Serial.flush();
    if (bufCntr==4) { return true; } //queue is full? go process them!
  //found an empty rxBuf (rxBufLen=0)
   
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      memcpy(rxBuf[ptr],buf,len);
      rxBufLen[ptr]=len;
      rxRSS[ptr]=rf95.lastRssi()+140;
      ret=true; 
    }
  }
  return ret;
}

//*****************************************
void json_PRINT(char jsn[][20], byte pNum) {
  Serial.print(F("DATA: "));
  for (byte pn=0;pn<pNum;pn++) { //Pair Number
    Serial.print( jsn[pn] ); }
  Serial.println(""); Serial.flush();  
}


//*****************************************
String pair_VALIDATE(char *buf,byte len) { //returns message with !ID!KEY as ID:KEY
  if (debugON>1) {Serial.print(F("\npair_VALIDATE... "));}
  if (debugON>1) {print_HEX(buf,len);  } 
  sKEY="thisisathingbits";
  sMSG= msg_GET(buf,len,sKEY); // CSB check,  not "" if good
  sRET="";
    if (debugON>1) {Serial.print(F("sMSG=")); Serial.println(sMSG); Serial.flush();}
  if((sMSG[0]=='!') && (sMSG[7]=='!')) {
    sID=sMSG.substring(1,7);
      if (debugON>1) {Serial.print(F("sID=")); Serial.println(sID); Serial.flush();}
    sKEY=sMSG.substring(8);
      if (debugON>1) {Serial.print(F("sKEY=")); Serial.println(sKEY); Serial.flush();}
    if (key_VALIDATE(sKEY)==true) {
      sRET=sID; sRET+=":"; sRET+=sKEY; //encode using TXKEY and send back
        if (debugON>1) {Serial.print(F("(sRET)=")); Serial.println(sRET); Serial.flush();}
    }
  }
  return sRET;
}

//*****************************************
String isINFO(String &sInfo) {
    if (debugON>1) {Serial.print(F("isINFO: ")); Serial.print(sInfo);}
  sRET="";
  if (sInfo.substring(0,5)=="INFO:") { sRET=sInfo.substring(5); }
    if (debugON>1) {Serial.print(F(", is now: "));Serial.println(sRET); Serial.flush(); }
  return sRET;
}

//*****************************************
String parse_TXID(String &idkey){
  sRET="";
  byte eos=idkey.length(); 
  byte i=0;
  while ((i<eos)&&(idkey[i]!=':')) {i++;}
  if (idkey[i]==':') { sRET=idkey.substring(0,i); }
  return sRET;  
}

//*****************************************
String parse_TXKEY(String &idkey){
  sRET="";
  byte eos=idkey.length();
  byte i=0;
  while ((i<eos)&&(idkey[i]!=':')) {i++;}
  if (idkey[i]==':') { sRET=idkey.substring(i+1); }  
  return sRET;
}

//*****************************************
//begins param handoff
//verify param-number
//look for id in eeprom list, return address of id
//check loc for 'new' status
// yes-make default params.
// no-send params using eeprom address provided
boolean param_ReqChk(String info) {//look for IDxxxx:param0
  bool ret=false;
    if (debugON>1) {Serial.print(F("param_ReqChk on: "));Serial.println(info);Serial.flush();}
  if (info.substring(6)==":param0") {
    //Serial.println(F("is :param0")); //find it (make it), send
    sID=info.substring(0,6);
    char pID[8];  sID.toCharArray(pID,7);
    word eePrmID= param_FIND_ID(pID); //overflow writes over eeADR=10
    if (EEPROM.read(eePrmID)==255) { param_SET_DEFAULT(sID,eePrmID);}
    param0_SEND(eePrmID);
    ret=true;
  }
  return ret;
}
//char parse out ID, look for match in eeprom,



//*****************************************
void param0_SEND(word eePrmID) { //get TXpower and interval from EEPROM per this ID
  //goto EEPROM , send values in EE_PARAM_INTERVAL and EE_PARAM_POWER
    if (debugON>1) {Serial.print(F("param_SEND from: "));Serial.println(eePrmID);Serial.flush(); }
  char param[15]; byte pp=0;  //Param Pointer  //got 6 matches
  for (word eeID=eePrmID;eeID>(eePrmID-6);eeID--) { 
    param[eePrmID-eeID]=EEPROM.read(eeID); 
  } //ID to param
  param[6]=':'; 
  param[7]=EEPROM.read(eePrmID-6);  //interval , sec./16
  param[8]=':';  
  param[9]=EEPROM.read(eePrmID-7);  //heartbeat, sec./64
  param[10]=':';
  param[11]=EEPROM.read(eePrmID-8); //power 1-10
  param[12]=':';
  param[13]=EEPROM.read(eePrmID-9); //13th, System byte? might be zero
  //ididid:i:h:p:s
  if (debugON>1) {print_HEX(param,14);}
  msg_SEND(param,14,keyPUBLIC,10);   //go high power
}

//*****************************************
word param_FIND_ID(char *pID) { word eeLoc=EE_PARAM_ID;
  if (debugON>1) {Serial.print(F("param_FIND_ID: ")); print_CHR(pID,6);}
byte eeByte=EEPROM.read(eeLoc); byte eep;

  while ((eeByte!=255)&&(eeLoc>20)) {
    eep=0;
    while ( eeByte==pID[eep] ) {
      eep++;
      if (eep==6) {return eeLoc;}
      else { eeByte=EEPROM.read(eeLoc-eep); }
    }
    eeLoc=eeLoc-10;
    eeByte=EEPROM.read(eeLoc);
  }
  //Serial.print(F(" at "));Serial.println(eeLoc); Serial.flush();
  return eeLoc;
}

//*****************************************
void param_SET_DEFAULT(String &id,word eeLoc) {//ID,Interval,Power
  //Serial.println(F("param_SET_DEFAULT... "));Serial.flush(); 
  byte bp;  
  for(bp=0;bp<6;bp++) { EEPROM.write(eeLoc-bp,id[bp]); } //0-5
    EEPROM.write(eeLoc-bp,4); bp++; //interval sec/16         //6
    EEPROM.write(eeLoc-bp,64); bp++; //heartbeat sec./64      //7
    EEPROM.write(eeLoc-bp,1); bp++;//1-10 default power        //8
    EEPROM.write(eeLoc-bp,0); //Sysbyte bits all 0            //9
}

//*****************************************
//uses key in idkey to encode response containing public key
void key_SEND(String &idTx,String &keyTx,String &keyPublic) {
  //randomize the send time in hopes of avoiding a simultaneous response
  //conflict from another receiver(s) that might be in rss range.
  randomSeed(analogRead(5));
  delay(byte(random()));
  sMSG=idTx; sMSG+=":"; sMSG+=keyPublic; //compose the public key with ID
  byte idkeyLEN=sMSG.length(); 
  char msg[64]; sMSG.toCharArray(msg,idkeyLEN+1);
  msg_SEND(msg,idkeyLEN,keyTx,10); //the TX should decode this, it made the key
  Serial.print(F("INFO:KEY_SEND,TO:"));Serial.println(idTx);Serial.flush();
}

//*****************************************
void msg_SEND(char* msgIN, byte msgLEN, String key, byte txPWR) { //txPWR is 1-10
    if (debugON>1) {Serial.print(F("msg_SENDin: "));print_HEX(msgIN,msgLEN);}
  byte CSB=csb_MAKE(msgIN,msgLEN);
  msgIN[msgLEN]=CSB;  
  msgLEN++;
    if (debugON>1) {Serial.print(F("encoding with: ")); Serial.println(key);Serial.flush();}
  const uint8_t* msg=encode(msgIN,msgLEN,key);
  rf95.setTxPower((txPWR*2), false); // from 1-10 to 2-20dB
  rf95.send(msg,msgLEN);//+1); //rf95 need msgLen to be one more ???
  rf95.waitPacketSent();
    if (debugON>1) {Serial.print(F("msg_SENDout: "));print_HEX((char*)msg,msgLEN);}
}

//***********************
//decoded and checksum-tested 
String msg_GET(char *buf, byte len,String &key) { 
  sRET="";
    if (debugON>1) {Serial.print(F("msg GET: "));print_HEX(buf,len); }
    if (debugON>1) {Serial.print(F("decoding with: ")); Serial.println(key);Serial.flush();}
  char *msg=decode(buf,len,key); msg[len]=0;
    if (debugON>1) {Serial.print(F("msg dec: ")); Serial.println(String(msg));Serial.flush(); }
  byte CSB=csb_MAKE(msg,len-1);
  byte csb=msg[len-1];
    if (debugON>1) {Serial.print(F("CSB: ")); Serial.print(CSB,HEX); }
    if (debugON>1) {Serial.print(F(", csb: ")); Serial.println(csb,HEX);Serial.flush(); }
    if (debugON>1) {print_HEX(msg,len-1);print_CHR(msg,len-1);}
  if (csb == CSB) {   msg[len-1]=0; sRET=String(msg); }
    if (debugON>1) {Serial.print(F("sRET: ")); Serial.println(sRET);Serial.flush(); }
  return sRET;
}

//*****************************************
byte csb_MAKE(char *pkt, byte len) {  byte CSB = 0;
  for (byte i = 0; i < len; i++) { CSB = CSB ^ pkt[i]; }
  return CSB;
}

//*****************************************
//encode is just before sending, so is char array for rf95.send
const uint8_t* encode(char* msg, byte len, String &key) {
  static byte cOUT[64];  byte i=0; byte k=0;
  if (key=="") {key[0]=0; } //XOR of zero is 'no change'
  int lenKEY=key.length();  
  while (i<len) {if (k>(lenKEY-1)) {k=0;}
    cOUT[i]=byte((byte(msg[i])^byte(key[k])));
    i++; k++;} 
  return cOUT;
} 

//************************* agh-gh-gh!**********
char* decode(char *msg, byte lenMSG, String &key) {
  static char cOUT[64]; cOUT[0]=0;  //prep for fail
  if (key=="") {key[0]=0; } //XOR of zero is 'no change'
  int lenKEY=key.length(); 
  byte sp;  //Starting Point  
  for (sp=0;sp<8;sp++) { //8 possible starting points - find it
    // validate first 9 bytes, '1|ididid|'
    byte tst=byte((byte(msg[1])^byte(key[sp+1]))); 				// looking for first delim '|' at 1
    if (tst=='|') { 																	// (fast first test)
      tst=byte((byte(msg[8])^byte(key[sp+8]))); 			// looking for last delim '|'
      if (tst=='|') {																	// (another fast test)		
        tst=byte((byte(msg[0])^byte(key[sp+0])));			// protocol byte within range?
        if ( (tst>='0') || tst<='9') { 								// a little longer test
          byte ix;																		// 3 passings - maybe skip the next?
          for (ix=2;ix<8;ix++) { 											// id chars 2,3,4,5,6,7 in range?
            tst=byte((byte(msg[ix])^byte(key[sp+ix])));
            if ( !((tst>='2') && (tst<='9')) && !((tst>='A') && (tst<='Z')) ) { break;}
          }
          if (ix==8) {																// all good?
            byte mx=0; byte kx=sp;	//this 'sp' is it!
            while (mx<lenMSG) { if (kx>(lenKEY-1)) {kx=0;}
              cOUT[mx]=byte((byte(msg[mx])^byte(key[kx]))); 
              mx++; kx++; 
            }
          break; //out of sp - all done
          }
    } }	}
  } //for sp
  
  if (sp==8) { //not data format? Maybe it's INFO?
    for (sp=0;sp<8;sp++) { //8 possible starting points - find it
      byte tst=byte((byte(msg[0])^byte(key[sp])));
      if (tst=='I') {
        tst=byte((byte(msg[1])^byte(key[sp+1])));
        if (tst=='N') {
          tst=byte((byte(msg[2])^byte(key[sp+2])));
          if (tst=='F') {
            tst=byte((byte(msg[3])^byte(key[sp+3])));
            if (tst=='O') {
              tst=byte((byte(msg[4])^byte(key[sp+4])));
              if (tst==':') { 
                byte mx=0; byte kx=sp;	//this 'sp' is it!
                while (mx<lenMSG) { if (kx>(lenKEY-1)) {kx=0;}
                  cOUT[mx]=byte((byte(msg[mx])^byte(key[kx]))); 
                  mx++; kx++; 
                }
      }}}}}
    }
  }
  
    if (sp==8) { //not data? not INFO? is this the key exchange?
      for (sp=0;sp<8;sp++) { //8 possible starting points - find it
        byte tst1=byte((byte(msg[0])^byte(key[sp+0])));
        byte tst2=byte((byte(msg[7])^byte(key[sp+7])));
        if((tst1=='!') && (tst2=='!')) {
          byte mx=0; byte kx=sp;	//this 'sp' is it!
          while (mx<lenMSG) { if (kx>(lenKEY-1)) {kx=0;}
            cOUT[mx]=byte((byte(msg[mx])^byte(key[kx]))); 
            mx++; kx++; 
          }
        }
      }
    }
 
  if (debugON>1) {Serial.print(F("Key Starting Index: "));Serial.println(sp);}
  if (debugON>1) {print_CHR(cOUT,lenMSG);print_HEX(cOUT,lenMSG);}  
  return cOUT;
}

//*****************************************
void key_EE_ERASE() {
  for (byte i=0;i<33;i++) { EEPROM.write(EE_KEY-i,255); } //the rest get FF's
}

//*****************************************
void param_EE_ERASE() {
  for (word i=EE_PARAM_ID;i>0;i--) { EEPROM.write(EE_PARAM_ID,255); } //the rest get FF's
}

//*****************************************
void key_EE_MAKE(byte kSize) { 
    if (debugON>1) {Serial.print(F("key_EE_MAKE... "));Serial.flush();}
  byte key;
  for(byte i=0;i<kSize;i++) { key= EEPROM.read(EE_KEY-i);
    if ((key<34) || (key>126)) {
      long rs=analogRead(2);  rs=rs+analogRead(3); rs=rs+analogRead(4);
      rs=rs+analogRead(5); 
      randomSeed(rs); byte i=0;
      while ((i<32) && (i<kSize)) { EEPROM.write(EE_KEY-i,random(34,126)); i++; }
      EEPROM.write(EE_KEY-i,0); //string null term.
      break;
    }
  }
}

//*****************************************
String key_EE_GET() { 
    if (debugON>1) {Serial.print(F("key_EE_GET... "));Serial.flush();}
  char cKey[33];  byte i=0;  
  i=0;  cKey[i]=EEPROM.read(EE_KEY); // this way includes the null at end
  while ((cKey[i]!=0) && (i<32)) {
    i++; //assign first, check later
    cKey[i]=EEPROM.read(EE_KEY-i); 
  }
  cKey[i]=0; //term-truncate it again?
  return String(cKey); 
}

//*****************************************
bool key_VALIDATE(String &key) { //check EEPROM for proper character range
  //Serial.print(F("key_VAL key=")); Serial.println(key);
  byte len=key.length(); 
  for (byte i=0;i<len;i++) {
    if ((key[i]<34) || (key[i]>126) || (len<4)) { return false; }
  }
  return true;
}

//*****************************************

String T2S(byte idx) { char cStr[20]; //Type to String
  strcpy_P(cStr, (char*)pgm_read_word(&(table_T[idx])));
  return String(cStr);
}
  
//**********************************************************************
void CheckPCbuf() { // Look for Commands from the Host PC
  if (Serial.available())  {
    char cbyte;   byte c_pos = 0;  char PCbuf[50];
    cbyte = Serial.read();
    while ((cbyte != '\n') && (cbyte != '\r'))  {
      PCbuf[c_pos] = cbyte; c_pos++; //build string
      if (c_pos >= sizeof(PCbuf)) {c_pos = 0; } //PCbuf over-run error handling
     
      cbyte = Serial.peek(); 
      
      byte trynum=0;
      while ((int(cbyte)< 0)&&(trynum<255)) {
        cbyte = Serial.peek();  //wait for valid character to appear
      }
      if (trynum==255) {cbyte='\n';}
      else {cbyte = Serial.read(); }
    }  //get another
    
    PCbuf[c_pos] = 0; // mark end-of-string ...
    //String PCmsg=String(PCbuf);
    if ( PCbuf[0] == '!') { WDTcounter=0; digitalWrite(LED_green,LOW); }
    if ( PCbuf[0] == '?') { showVER(); }
    if ( PCbuf[0] == 'd') { debugON=PCbuf[1]-'0';
                            Serial.print(F("debugON=")); Serial.println(debugON); }
    
    if (( PCbuf[0] == 'p')&&(PCbuf[1] == ':')) { //Parameter stuff to follow
      if (debugON>1) {Serial.print(F("p:"));print_CHR(PCbuf,c_pos);}
      byte bp=0; byte cp[4]; byte cpp=0; //next colon positions for substr
      while (PCbuf[bp]!=0) {//format p:ididid:iii:hhh:p:s c[0],c[1],c[2],c[3],c[4]
        if (PCbuf[bp]==':') {cp[cpp]=bp; cpp++;} // log postion of all ':'s
        bp++; //and leave with length
      } //cp[0,1,2,3] = postions of delimiters between the four things, bp=length of PCbuf
      byte len1=(cp[1]-cp[0])-1;  //ID, 6 char
      byte len2=(cp[2]-cp[1])-1;  //data interval, 1 byte
      byte len3=(cp[3]-cp[2])-1;  //heartbeat, 1 byte
      byte len4=(cp[4]-cp[3])-1;  //power level, 0-9, 1 byte   
      byte len5=(bp-cp[4])-1;     //system byte flag bits, ascii hex text 00-FF
      if (cp[1]==8) {//validation of format p:ididid:iii:hhh:p:ss       //strncpy(char* dest, char* src, int n)
//char p_pwr[3]; p_pwr=PCbuf.substr(9,(bpp+1));
        char p_id[8];memcpy(p_id,&PCbuf[cp[0]+1],len1); p_id[len1]=0; //ID, 6 char
        char p_int[5];memcpy(p_int,&PCbuf[cp[1]+1],len2); p_int[len2]=0; //data interval, 1 byte
        char p_hb[5];memcpy(p_hb,&PCbuf[cp[2]+1],len3); p_hb[len3]=0; //heartbeat, 1 byte        
        char p_pwr[3];memcpy(p_pwr,&PCbuf[cp[3]+1],len4);  p_pwr[len4]=0;   //power level, 0-9, 1 byte
        char p_sys[3];memcpy(p_sys,&PCbuf[cp[4]+1],len5);  p_sys[len5]=0;   //system byte flag bits, ascii hex text 00-FF
        byte bINT=byte(atoi(p_int));
        byte bHB=byte(atoi(p_hb));
        byte bPWR=byte(atoi(p_pwr)); 
        byte bSYS=byte(strtol(p_sys,NULL,16)); //2 char hex string to long
        //Serial.print(F("p_id:"));Serial.print(p_id); //0-5
        //Serial.print(F(", bINT:"));Serial.print(bINT); //6,
        //Serial.print(F(", bHB:"));Serial.print(bHB); //7,
        //Serial.print(F(", bPWR:"));Serial.print(bPWR); //8
        //Serial.print(F(", bSYS:"));Serial.println(bSYS); //9
        //put it eeprom...
        word eeADR=param_FIND_ID(p_id); //find existing or next avail location
        byte eePtr;
        for (eePtr=0;eePtr<6;eePtr++) {EEPROM.write(eeADR-eePtr,p_id[eePtr]);}
        EEPROM.write(eeADR-eePtr,bINT); eePtr++;
        EEPROM.write(eeADR-eePtr,bHB); eePtr++;        
        EEPROM.write(eeADR-eePtr,bPWR); eePtr++;
        EEPROM.write(eeADR-eePtr,bSYS);
      }
    } //if 'p'      
  } //if Serial.aval
} //End Of CheckPCbuf

//*****************************************
String mySubChar(char* in,byte from,byte len) {
  byte p=0; char out[255];
  for (byte i=from;i<(from+len);i++) {out[p]=in[i]; p++;}
  out[p]=0;
  return String(out);
}

//**********************************************************************
void showVER() { Serial.println( String (VER)); Serial.flush();
}

//*****************************************
ISR(WDT_vect) { //in avr library
  WDTcounter++; 
  if (WDTcounter>10) {
    if (digitalRead(LED_green)==1) {digitalWrite(LED_green,LOW);} //ON
    else {digitalWrite(LED_green,HIGH);} //OFF
  }
}

//*****************************************
void print_HEX(byte *buf,byte len) { byte i;
  Serial.print(len);Serial.print(F(", "));
  for (i=0;i<(len-1);i++) {Serial.print(byte(buf[i]),HEX);Serial.print(F(" "));}
  Serial.println(byte(buf[len-1]),HEX); Serial.flush();
}
//*****************************************
void print_CHR(char *buf,byte len) { byte i;
  Serial.print(len);Serial.print(F(": "));
  for (i=0;i<(len-1);i++) {Serial.print( buf[i]);}
  Serial.println(buf[len-1]);Serial.flush();
}
