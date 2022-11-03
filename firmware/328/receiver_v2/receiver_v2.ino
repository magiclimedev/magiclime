// 
// the psuedo code...
// assign fixed-length static variables for STRING stuff to avoid heap-conflicts
// assign sensor names to sensor number as constant char arrays
// setup: spit out program version in json format... {\"source\":\"rx\",\"info\":\"ver=receiver\"}
//        initialize the RF95 module
//        get key from eeprom or make one if not there
// loop:  look at RX buffer
//        look at PC buffer
// then...
//  if RX and RSS > 100?: possible pairing situation
//    decode using key = 'thisisamagiclime'
//      yes? then you also received a temporary key.
//      use it to send this receivers' private key back.
//        (the sensor then stashes it in eeprom to use henceforth - and 'pairing' is now done.)
//      save the sensor ID in eeprom at top of a block-O-bytes (pending action byte and parameter settings)
//        (update with new parameters? or fulfill a request for parameters from the sensor, or forced update) 
//  otherwise...    
//   decode using key:
//     is it a data packet - recognizable by delimiters in expected places?
//        then 'jsonize' the data, send to PC.
//        and then... see if there is a pending action for that ID - do it.
//     is it a request for parameters? ( first characters == "PUR:" Parameter Update Request)
//        get them from sensor ID's block in eeprom - send back.
//     is it a general 'INFO' message? (info, not sure exactly what, but want to have it be possible)
//        send json string to PC \"source\":\"tx\",\"info\":\"whatever...\"}"
//
//    if PC buffer has data...
//      match possible commands...
//      p -> format p:ididid:iii:hhh:p:s , data interval, heartbeat (as measured by watchdog timeouts), power, system byte.
//  !! OR !!....
//      pu:ididid:di:xxx (Parameter Update:6chrid:Data Interval: xxx=ascii number 000-255 'ex.002'
//      pu:ididid:hb:xxx  (HeartBeat: xxx= ascii number 000-255
//      pu:ididid:pw:x  (tx PoWer setting : ascii 0-9
//      pu:ididid:sb:xx (System Bits: ascii hex value 00-FF
//      set pending action bit in pending action byte for that TX ID.
// oh - system byte... flag bits for ???. usually reset upon completion of whatever task got flagged to run.
//      d -> change the debugON setting
//      ! -> I don't remember why, but it turns the LED on for a watchdog period.

const static char VER[] = "receiver_v2";

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
#define EE_KEY_RSS        EE_SYSBYTE-1 // rss for key exchange
#define EE_KEY            EE_KEY_RSS-1  //KEY is 16 - no string null 
#define EE_PARAM_ID       EE_KEY-16     // 6 char
#define EE_PARAM_INTERVAL EE_PARAM_ID-6 //'seconds/16' 255=68 min.
#define EE_PARAM_HRTBEAT  EE_INTERVAL-1 //'seconds/64' 255=272 min.
#define EE_PARAM_POWER    EE_PARAM_HRTBEAT-1  //1-10  (2-20dB in sensor).
#define EE_PARAM_SYSBYTE  EE_PARAM_POWER-1  // + this one = 10 bytes per ID,
// PARAM ID things go all the way down to 0
 
//the sending of parameters is an 'upon request' thing.
//'request from sensor' format, 'IDIDID:param0' (where IDIDID is sensor ID).

//To set parameter values in EEPROM (so they are ready-to-go upon request),
// send this program a 'p:IDIDID:iii:hhh:p:s', where...
// 'IDIDID' is the sensor ID,
// 'iii' is data Interval count (16 sec. per, 255 max), (('0' might make it TX every 8 sec.?))
// 'hhh' is Heartbeat interval count (64 sec. per, 255 max),
// 'p' is TX Power (0-9), (in sensor -> +1=0-10, x2-> 2-20)
// 's' is the 'System byte' - various flag bits, the exact meanings of which are tbd.
//  things like 'reset max, reset min, reset avg, calibrate now, erase eeprom' etc.

word WDTcounter=10; //start out flashing
//byte LED_green=16; //'A2' ?? what is this??
//byte LED_red=17; //'A3' ?? and this??

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
const char T7[] PROGMEM = "2Button";
const char T8[] PROGMEM = "SBN8"; 
const char T9[] PROGMEM = "SBN9"; 
const char T10[] PROGMEM = "Temp"; 
const char T11[] PROGMEM = "Light";
const char T12[] PROGMEM = "T,RH";
const char T13[] PROGMEM = "SBN13";
const char T14[] PROGMEM = "SBN14";
const char T15[] PROGMEM = "SBN15";
const char T16[] PROGMEM = "SBN16";
const char T17[] PROGMEM = "SBN17";
const char T18[] PROGMEM = "SBN18";
const char T19[] PROGMEM = "SBN19";
const char T20[] PROGMEM = "SBN20";
const char T21[] PROGMEM = "SBN21";
const char T22[] PROGMEM = "SBN22";
const char T23[] PROGMEM = "SBN23";
const char T24[] PROGMEM = "SBN24";
const char T25[] PROGMEM = "SBN25";

PGM_P const table_T[] PROGMEM ={T0,T1,T2,T3,T4,T5,
  T6,T7,T8,T9,T10,T11,T12,T13,T14,T15,T16,
  T17,T18,T19,T20,T21,T22,T23,T24,T25};

char rxBUF[64]; //for the rx buf
byte rxLEN; //for the length
char rxKEY[18]; //16 char + null 

byte debugON;
byte keyRSS=80;
byte RSSnow;
 
//**********************************************************************
void setup() {  debugON=0;//3;//0;//1;
  //pinMode(LED_red,OUTPUT);
 // digitalWrite(LED_red,LOW); //on
 // pinMode(LED_green,OUTPUT);
 // digitalWrite(LED_green,HIGH); //off
  //EE_ERASE_all();
  
  
  while (!Serial);
  Serial.begin(57600);
  showVER();
  showKSS();
  if (rf95.init()) { rf95.setFrequency(RF95_FREQ); 
    char info[10]; strcpy(info,"rf95.init OK");
    showINFO(info); 
  }
  else {Serial.println(F("INFO:rf95.init FAIL"));Serial.flush(); while (1);
  }

  //Serial.println("RFM95 register settings...");
  //rf95.printRegisters();
  flgShowChar=false;
  flgShowHex=false;
  
  //key_EE_ERASE();  param_EE_ERASE();
  key_EE_GET(rxKEY); //Serial.print(F("key="));Serial.println(rxKEY);
  if (key_VALIDATE(rxKEY)==false) { //Serial.println(F("key_VALIDATE(rxKEY)==false"));Serial.flush(); //bad key? - make a new one?
    key_EE_MAKE();
    key_EE_GET(rxKEY); //Serial.print(F("key new="));Serial.println(rxKEY);
  }
  if (EEPROM.read(EE_KEY_RSS)<200) {keyRSS=EEPROM.read(EE_KEY_RSS);} 
  //Serial.print(F("keyRSS="));Serial.println(keyRSS); Serial.flush();
  
  //watchdog timer - 1 sec
  cli(); wdt_reset();
  WDTCSR |= B00011000; //bits 3,4 set up for clocking in timer
  //WDTCSR = B01100001; //9=8sec
  WDTCSR = B01000110; //6=1sec
  sei(); //watchdog timer - 1 sec
  //digitalWrite(LED_red,HIGH); //off
  
} // *************************************End Of SETUP *****
// *********************************************************

// *********************************************************
void loop(){ 
  RSSnow=rxBUF_CHECK();
  if (RSSnow>0) { rxBUF_PROCESS(RSSnow); }
  pcBUF_CHECK();
} // ************************************* End Of LOOP *****
// *********************************************************

//*********  get RF95 buffer - return rss>0 if message  ****************************
byte rxBUF_CHECK() { //********* get RF Messages - return RSS=0 for no rx
   //max of 4 in a row, then bail out and process them
  //otherwise, bail out when NOT rf95.available
  byte rss=0;
  if (rf95.available()) { // Should be a message for us now 
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      memcpy(rxBUF,buf,len);
      rxLEN=len;
      rss=rf95.lastRssi()+150;
    }
  }
  return rss;
}

// *********************************************************
void rxBUF_PROCESS(byte rss) { bool flgDONE=false;
  //Serial.print(F("\n...rxBUF_PROCESS: rss="));  Serial.println(rss); Serial.flush();
  char msg[64]; 
  if (rss>=keyRSS) { //first requirement for pairing
    char pairKEY[18]; strcpy(pairKEY,"thisisamagiclime");
    pair_VALIDATE(msg,rxBUF,rxLEN,pairKEY); //!ididid!txkey.. --> ididid:keykeykey
    //Serial.print(F("msg: ")); Serial.println(msg); Serial.flush();
    if ((msg[0]!=0) && (msg[6]==':')) {
      //Serial.println(F("pairing with txKEY"));  Serial.flush(); 
      char txid[8]; char txkey[18];
      mySubStr(txid,msg,0,6);
      //Serial.print(F("txid="));Serial.println(txid);Serial.flush();
      mySubStr(txkey,msg,7,strlen(msg)-7);
      //Serial.print(F("txkey="));Serial.println(txkey);Serial.flush();
      key_SEND(txid,txkey,rxKEY);
      flgDONE=true; //exit (0); //****** no need to go further if it was a pairing
    }
  } 
    //**************************************************************
  if (flgDONE==false) { //for pur, data, info
    //Serial.println(F("doing msg with rxKEY")); Serial.flush();  
    msg_GET(msg,rxBUF,rxLEN,rxKEY); //decode using rx KEY
    if (msg[0]!=0) { //is it 'PUR'? Or DATA? or INFO?
      //Serial.print(F("msgGET: ")); Serial.println(msg);Serial.flush();
      char pur[32];
      purFIND(pur,msg); //strips off the 'PUR:', is "" if not 'PUR'. // RX expects PUR:IDxxxx:PRM0
      if (pur[0]!=0) { //is this a request for parameters?
        //Serial.print(F("purFOUND: ")); Serial.println(pur);Serial.flush(); 
        flgDONE=pur_REQ_CHECK(pur); //true is 'params passed - all done'
      }
      
      if (flgDONE==false) { //data, info
    
        if ((msg[0]>='0') && (msg[0]<='9') && (msg[1]=='|')) { //protocol # validate
          char protocol=msg[0];
          byte pNum; //json Number of Pairs 
          char jp[6][24]={0}; //Json Pairs, 6 'cause it should be mostly enough.
          
          byte jpx; //json Pair # indeX pointer
          switch (protocol) { 
            case '1': {   pNum=5; // 5 pairs in this protocol
              //compose rss pair and first-half of data pairs, 
              strcpy(jp[0],"\"rss\":"); //no first comma,no quotes for numbers
                char chr[5]; itoa(rss,chr,10);
              strcat(jp[0],chr); //integer
              strcpy(jp[1],",\"uid\":\"");     //data is string (6) , 20-5=15 left for data object
              strcpy(jp[2],",\"sensor\":\"");   //data is string, 20-7, the verbose one 
              strcpy(jp[3],",\"bat\":");      //data is number (3), 20-6=14 no pre-quote
              strcpy(jp[4],",\"data\":\"");   //data is string, 20-7
              
              byte bx=2; //Buf indeX start: [0] is protocol, [1] is first delimiter 
              char ps[24];  //Pair String accumulator
              byte psx=0; //ps's indeX
              jpx=1; //json Pair # indeX, '0' is rss - already done.  
              while (bx<=rxLEN) {
                if ((msg[bx]=='|')||(bx==(rxLEN))) { // hit next delim?, got a pair
                  ps[psx]=0; //null term
                  switch (jpx) {
                    case 1: {strcat(jp[1],ps); } break; 
                    case 2: {strcat_P(jp[2], (char*)pgm_read_word(&(table_T[atoi(ps)]))); } break; 
                    case 3: {strcat(jp[3],ps); } break; 
                    case 4: {strcat(jp[4],ps); } break; 
                  }
                  psx=0;  jpx++; //done with this 'jpx', prep for next pair
                }
                else { ps[psx]=msg[bx]; psx++; } //build up pair # jpx's data
                bx++; //next char in msg
              }
              //and now tack on closing quote (or not,because it was a number)
              strcat(jp[1],"\""); //value was string, close quote
              strcat(jp[2],"\""); //value was string, close quote
              //strcat(jp[3],"");   //value was number - no close quote
              strcat(jp[4],"\"}"); //value was string - last one get the closing brace
              
              json_PRINTdata(jp,pNum); //move this down if data validation works well
                
            } //End of case:'1'
          } //EndOfSwtchProtocol
          flgDONE=true;
        }//validate protocol format
      } //flgDONE=false (data,info)
      
      if (flgDONE==false) { Serial.println(F("looking for INFO")); 
        //byte lenInfo = sPUR.length;
        //json_PRINTinfo(sInfo,lenInfo); 
      } //flgDONE=false (INFO)
      
    } //end of rxDECODED OK msg[0]!=0
  }//flgDONE=false (pur,data,info)
} //end of rxBUF_PROCESS() 

//*****************************************
void json_PRINTdata(char jsn[][24], byte pNum) {
  Serial.print(F("{\"source\":\"tx\","));
  for (byte pn=0;pn<pNum;pn++) { //Pair Number
    Serial.print( jsn[pn] ); }
  Serial.println(""); Serial.flush();  
}

//*****************************************
void json_PRINTinfo(char info[32], byte iNum) {
  Serial.print(F("{\"source\":\"tx\",\"info\":\""));
  for (byte i=0;i<iNum;i++) {Serial.print( info[i] ); }
  Serial.println(""); Serial.flush();  
}

//*****************************************pair_VALIDATE(msg,rxBUF,rxLEN,pairKEY);
char *pair_VALIDATE(char *idkey, char *rxbuf, byte rxlen, char *rxkey) { char *ret=idkey; 
  //Serial.println(F("...pair_VALIDATE..."));Serial.flush();
  idkey[0]=0; //fail flag
  char msg[64]; char txid[8]; char txkey[18];
  decode(msg,rxbuf,rxlen,rxkey);
  byte msgLEN=strlen(msg);
  if ((msg[0]=='!') && (msg[7]=='!')) {
    mySubStr(txid,msg,1,6);
    //Serial.print(F("txid="));Serial.println(txid);Serial.flush();
    mySubStr(txkey,msg,8,msgLEN-8);
    //Serial.print(F("txkey="));Serial.println(txkey);Serial.flush();
    strcpy(idkey,txid); strcat(idkey,":"); strcat(idkey,txkey);
  }
  return ret;
}
  
//*****************************************
char *purFIND(char *purOUT, char *purIN) {char *ret=purOUT; // RX expects PUR:IDxxxx:PRM0
  //Serial.println(F("...purFIND..."));Serial.flush();
  purOUT[0]=0; //default failflag
  char pfx[32];
  mySubStr(pfx,purIN,0,4);
  if (strcmp(pfx,"PUR:")==0) { mySubStr(purOUT,purIN,4,strlen(purIN)); }//strip off the 'PUR:'
  return ret;
}

//*****************************************
//begins param handoff
//verify param-number
//look for id in eeprom list, return address of id
//check loc for 'new' status
// yes-make default params.
// no-send params using eeprom address provided

boolean pur_REQ_CHECK(char *purTXID) { bool ret=false; //looking for txidxx:PRM0
  //Serial.println(F("...pur_REQ_CHECK..."));Serial.flush();
  char pur[8];
  mySubStr(pur,purTXID,6,strlen(purTXID));
 
  if (strcmp(pur,":PRM0")==0) {
    char txid[8];
    mySubStr(txid,purTXID,0,6);
    word eePrmID= param_FIND_ID(txid); //overflow writes over eeADR=10
    if (EEPROM.read(eePrmID)==255) { param_SET_DEFAULT(txid,eePrmID);}
    param0_SEND(eePrmID);
    ret=true;
  }
  return ret;
}
//char parse out ID, look for match in eeprom,

//*****************************************
void param0_SEND(word eePrmID) { //get TXpower and interval from EEPROM per this ID
  //Serial.print(F("param0_SEND from: "));Serial.println(eePrmID);Serial.flush(); 
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
  if (debugON>1) {print_HEX((char*)param,14);}
  msg_SEND(param,14,rxKEY,2);   //
}

//*****************************************
word param_FIND_ID(char *pID) { word eeLoc=EE_PARAM_ID;
  //Serial.print(F("...param_FIND_ID: ")); Serial.println(pID);Serial.flush();
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
  return eeLoc;
}

//*****************************************
void param_SET_DEFAULT(char *id,word eeLoc) {//ID,Interval,Power
  //Serial.println(F("param_SET_DEFAULT... "));Serial.flush(); 
  byte bp;  
  for(bp=0;bp<6;bp++) { EEPROM.write(eeLoc-bp,id[bp]); } //0-5
    EEPROM.write(eeLoc-bp,4); bp++; //interval sec/16         //6
    EEPROM.write(eeLoc-bp,64); bp++; //heartbeat sec./64      //7
    EEPROM.write(eeLoc-bp,1); bp++;//1-10 default power        //8
    EEPROM.write(eeLoc-bp,0); //Sysbyte bits all 0            //9
}

//*****************************************
//uses key in idkey to encode response containing rxKEY
void key_SEND(char *txid, char *txkey, char *rxkey) {
  //Serial.println(F("...key_SEND... "));Serial.flush(); 
  char txBUF[48]; 
  strcpy(txBUF,txid); strcat(txBUF,":"); strcat(txBUF,rxkey); //ididid:rxkeyrxkeyrxkeyr
  byte txLEN=strlen(txBUF);
  msg_SEND(txBUF,txLEN,txkey,1); //the TX will decode this, it made the key
  Serial.print(F("{\"source\":\"rx\",\"info\":\"key2tx\",\"id\":\""));
  Serial.print(txid); //Serial.println(F("\"}")); Serial.flush();
}

//*****************************************
void msg_SEND(char* msgIN, byte msgLEN, char *msgKEY, byte txPWR) { //txPWR is 1-10
  //Serial.print(F("...msg_SENDing... "));Serial.println(msgIN);Serial.flush();
  //Serial.print(F("with key: "));Serial.println(msgKEY);Serial.flush();
  char txBUF[64];
  encode(txBUF,msgIN,msgLEN,msgKEY);
  rf95.setTxPower((txPWR*2), false); // from 1-10 to 2-20dB
  rf95.send(txBUF,msgLEN);//+1); //rf95 need msgLen to be one more ???
  rf95.waitPacketSent();
  //Serial.print(F("msg_SENT: ")); print_HEX(txBUF,msgLEN);
}

//***********************
//decoded and checksum-tested 
char *msg_GET(char *msgOUT,char *bufIN, byte bufLEN,char *key) { char *ret=msgOUT;
  //Serial.println(F("...msg_GET... "));Serial.flush();
  decode(msgOUT,bufIN,bufLEN,key);
  return ret; //ret;
}

//*****************************************
//encode is just before sending, so is non-string char array for rf95.send
char *encode(char *msgOUT, char *msgIN, byte lenMSG, char *key) { char *ret=msgOUT;
  //Serial.print(F("...encode with key: "));Serial.println(key); Serial.flush();
  byte i; byte k=0;
  byte keyLEN=strlen(key);
  for (i=0;i<lenMSG;i++) {
    if (k==keyLEN) {k=0;}
    msgOUT[i]=byte((byte(msgIN[i])^byte(key[k])));
    k++;}
  msgOUT[i]=0; //null term?
  return ret;
} 

//*** agh-gh-gh! so many lines! **********
//** but bailing out fast is important - not sure how compiler does - if ("xxxx" != "yyyy" ) 
char* decode(char *msgOUT, char *rxBUF, byte rxLEN, char *key) { char *ret=msgOUT;
  //Serial.print(F("...decode with key: "));Serial.println(key); Serial.flush();
  msgOUT[0]=0; //fail flag
  static char cOUT[64]; cOUT[0]=0;  //prep for fail
  byte keyLEN=strlen(key); 
  byte sp;  //Starting Point  
  //is it a data packet?
  for (sp=0;sp<8;sp++) { //8 possible starting points - find it
    // validate first 9 bytes, '1|ididid|'
    //yes - could do'&' of all elements but I don't know that the compiler doesn't take the
    //time to do them all before returning 'false' - this way does bail asap.
    byte tst=byte((byte(rxBUF[1])^byte(key[sp+1])));         // looking for first delim '|' at 1
    if (tst=='|') {                                   // (fast first test)
      tst=byte((byte(rxBUF[8])^byte(key[sp+8])));       // looking for last delim '|'
      if (tst=='|') {                                 // (another fast test)    
        tst=byte((byte(rxBUF[0])^byte(key[sp+0])));     // protocol byte within range?
        if ( (tst>='0') || tst<='9') {                // a little longer test
          byte ix;                                    // 3 passings - maybe skip the next?
          for (ix=2;ix<8;ix++) {                      // id chars 2,3,4,5,6,7 in range?
            tst=byte((byte(rxBUF[ix])^byte(key[sp+ix])));
            if ( !((tst>='2') && (tst<='9')) && !((tst>='A') && (tst<='Z')) ) { break;} }
            
          if (ix==8) {                                // all good?
            byte kx=sp;  //this 'sp' is it!
            byte i;
            for (i=0;i<rxLEN;i++) {
              if (kx==keyLEN) {kx=0;}
              msgOUT[i]=byte((byte(rxBUF[i])^byte(key[kx]))); 
              kx++; }
            msgOUT[i]=0; //null term
            return ret;
          }
    } } }
  } //for sp
  
  //not Data? Is it PUR - Parameter Update Request?...
  for (sp=0;sp<8;sp++) { //8 possible starting points - find it
    byte tst=byte((byte(rxBUF[0])^byte(key[sp])));
    if (tst=='P') {
      tst=byte((byte(rxBUF[1])^byte(key[sp+1])));
      if (tst=='U') {
        tst=byte((byte(rxBUF[2])^byte(key[sp+2])));
        if (tst=='R') {
          tst=byte((byte(rxBUF[3])^byte(key[sp+3])));
            if (tst==':') {
              byte kx=sp;  //this 'sp' is it!
              byte i;
              for (i=0;i<rxLEN;i++) {
                if (kx==keyLEN) {kx=0;}
                msgOUT[i]=byte((byte(rxBUF[i])^byte(key[kx]))); 
                kx++; }
              msgOUT[i]=0; //null term
              return ret;
            }
    } } } 
  } 
  
//not data and not PRM? Maybe it's INFO?
  for (sp=0;sp<8;sp++) { //8 possible starting points - find it
    byte tst=byte((byte(rxBUF[0])^byte(key[sp])));
    if (tst=='I') {
      tst=byte((byte(rxBUF[1])^byte(key[sp+1])));
      if (tst=='N') {
        tst=byte((byte(rxBUF[2])^byte(key[sp+2])));
        if (tst=='F') {
          tst=byte((byte(rxBUF[3])^byte(key[sp+3])));
          if (tst=='O') {
            tst=byte((byte(rxBUF[4])^byte(key[sp+4])));
            if (tst==':') { 
              byte kx=sp;  //this 'sp' is it!
              byte i;
              for (i=0;i<rxLEN;i++) {
                if (kx==keyLEN) {kx=0;}
                msgOUT[i]=byte((byte(rxBUF[i])^byte(key[kx]))); 
                kx++; }
              msgOUT[i]=0; //null term
              return ret;
   }}}}}
  }
    
//not data? not PRM? not INFO? is this the key exchange?
  for (sp=0;sp<8;sp++) { //8 possible starting points - find it
    byte tst1=byte((byte(rxBUF[0])^byte(key[sp+0])));
    byte tst2=byte((byte(rxBUF[7])^byte(key[sp+7])));
    if((tst1=='!') && (tst2=='!')) {
      byte kx=sp;  //this 'sp' is it!
      byte i;
      for (i=0;i<rxLEN;i++) {
        if (kx==keyLEN) {kx=0;}
        msgOUT[i]=byte((byte(rxBUF[i])^byte(key[kx]))); 
        kx++; }
      msgOUT[i]=0; //null term
      return ret;
    }
  }
  return ret;
}

//*****************************************
void key_EE_ERASE() {
  for (byte i=0;i<16;i++) { EEPROM.write(EE_KEY-i,255); } //the rest get FF's
}

//*****************************************
void param_EE_ERASE() { //danger danger will robinson! this is ALL sensor parameters
  for (word i=EE_PARAM_ID;i>0;i--) { EEPROM.write(i,255); } //the rest get FF's
}

//*****************************************
void key_EE_MAKE() {
  long rs=analogRead(2);  rs=rs+analogRead(3); rs=rs+analogRead(4);
  rs=rs+analogRead(5); randomSeed(rs);
  for (byte i=0;i<16;i++) { EEPROM.write(EE_KEY-i,random(34,126)); }
}

//*****************************************
char *key_EE_GET(char *key) { char *ret=key;
  byte i;
  for (i=0;i<16;i++) { key[i]=EEPROM.read(EE_KEY-i); }
  key[i]=0; //null term
  return ret; 
}

//*****************************************
bool key_VALIDATE(char *key) { //check EEPROM for proper character range
  byte len=strlen(key); 
  if (len<16) { return false; }
  for (byte i=0;i<len;i++) { if ((key[i]<34) || (key[i]>126)) { return false; } }
  return true;
}

//*****************************************
char *T2S(char *type, byte idx) { char *ret=type;
  strcpy_P(type, (char*)pgm_read_word(&(table_T[idx])));
  return ret;
}
  
//**********************************************************************
void pcBUF_CHECK() { // Look for Commands from the Host PC
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
    //if ( PCbuf[0] == '!') { WDTcounter=0; digitalWrite(LED_green,LOW); } //? what's this for again?
    if ( PCbuf[0] == '?') { showVER(); }
    if ( PCbuf[0] == 'd') { debugON=PCbuf[1]-'0';
                            Serial.print(F("debugON=")); Serial.println(debugON); }
    if (( PCbuf[0]=='k')&&(PCbuf[1]=='s')&&(PCbuf[2]=='s')) { //Key Signal Strength
      byte i=3; char kss[4];  //get kss value at PCbuf[3,4 and maybe 5]
      while ((PCbuf[i]!=0) && (i<6)) {kss[i-3]=PCbuf[i]; i++;}
      if ((i>3)&&(i<7)) { //not bad data? term ascii value
        kss[i-3]=0;  keyRSS=byte(atoi(kss));
        EEPROM.write(EE_KEY_RSS,keyRSS);
      }
      showKSS();
    }
    
    if (( PCbuf[0] == 'p')&&(PCbuf[1] == ':')) { //Parameter stuff to follow
      //Serial.print(F("p:")); print_CHR(PCbuf,c_pos);}
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
char *mySubStr(char *out, char* in,byte from,byte len) { char *ret=out;
  byte p=0; 
  for (byte i=from;i<(from+len);i++) {out[p]=in[i]; p++;}
  out[p]=0;
  return ret;
}

//**********************************************************************
void showINFO(char *info) { 
  Serial.print(F("{\"source\":\"rx\",\"info\":\""));
  Serial.print(info);
  Serial.println(F("\"}")); Serial.flush();
}

//**********************************************************************
void showKSS() { 
  Serial.print(F("{\"source\":\"rx\",\"info\":\"kss="));
  Serial.print(keyRSS);
  Serial.println(F("\"}")); Serial.flush();
}

//**********************************************************************
void showVER() { 
  Serial.print(F("{\"source\":\"rx\",\"info\":\"ver="));
  Serial.print(VER);
  Serial.println(F("\"}")); Serial.flush();
}

//*****************************************
ISR(WDT_vect) { //in avr library
  WDTcounter++; 
  //if (WDTcounter>10) {
  //  if (digitalRead(LED_green)==1) {digitalWrite(LED_green,LOW);} //ON
  //  else {digitalWrite(LED_green,HIGH);} //OFF
  //}
}

//*****************************************
void EE_ERASE_all() {
  Serial.print(F("EE_ERASE_all...#"));Serial.flush();
  for (word i=0;i<1024;i++) { EEPROM.write(i,255); } //the rest get FF's
  Serial.println(F(" ...Done#"));Serial.flush();
}
  
//*****************************************
void print_HEX(char *buf,byte len) { byte i;
  Serial.print(len);Serial.print(F(", "));
  for (i=0;i<(len-1);i++) {Serial.print(byte(buf[i]),HEX);Serial.print(F(" "));}
  Serial.println(byte(buf[i]),HEX); Serial.flush();
}

//*****************************************
void print_CHR(char *buf,byte len) { byte i;
  Serial.print(len);Serial.print(F(": "));
  for (i=0;i<(len-1);i++) {Serial.print( buf[i]);}
  Serial.println(buf[i]); Serial.flush();
}
