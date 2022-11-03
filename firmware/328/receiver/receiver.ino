/* Copyright (C) 2022 Marlyn Anderson - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the ...
 *
 * You should have received a copy of the ... with
 * this file. If not, please write to: , or visit :
 * magiclime.com
 */
 
const static char VER[] = "receiver";

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

byte keyRSS=80;
byte RSSnow;
 
//**********************************************************************
void setup() { 
  while (!Serial);
  Serial.begin(57600);
  showVER();
  if (EEPROM.read(EE_KEY_RSS)<200) {keyRSS=EEPROM.read(EE_KEY_RSS);} 
  showKSS();
  
  if (rf95.init()) { rf95.setFrequency(RF95_FREQ); 
    char info[10]; strcpy(info,"rf95.init !OK!");
    showINFO(info); 
  }
  else {
    char info[10]; strcpy(info,"rf95.init !FAIL!");
    showINFO(info);
    while (1);
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
  //showINFO(rxKEY);
  
} // End Of SETUP ******************

void loop(){ 
  RSSnow=rxBUF_CHECK();
  if (RSSnow>0) { rxBUF_PROCESS(RSSnow); }
  pcBUF_CHECK();
} //  End Of LOOP 


// get RF95 buffer - return rss>0 if message  ***********
byte rxBUF_CHECK() { //********* get RF Messages - return RSS=0 for no rx
  byte rss=0;
  if (rf95.available()) { // Should be a message for us now 
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      memcpy(rxBUF,buf,len);
      rxLEN=len;
      rss=rf95.lastRssi()+150; // Yeah, a little high, but I saw -1 at 140 once. 
    }
  }
  return rss;
}

// *********************************************************
void rxBUF_PROCESS(byte rss) { bool flgDONE=false;
  char msg[64]; 
  if (rss>=keyRSS) { //first requirement for pairing
    char pairKEY[18]; strcpy(pairKEY,"thisisamagiclime");
    pair_VALIDATE(msg,rxBUF,rxLEN,pairKEY); //!ididid!txkey.. --> ididid:keykeykey
    if ((msg[0]!=0) && (msg[6]==':')) {
      char txid[8]; char txkey[18];
      mySubStr(txid,msg,0,6);
      mySubStr(txkey,msg,7,strlen(msg)-7);
      key_SEND(txid,txkey,rxKEY);
      flgDONE=true;
    }
  } 
    //**************************************************************
  if (flgDONE==false) { //for pur, data, info
    rx_DECODE_0(msg,rxBUF,rxLEN,rxKEY); //decode using rx KEY
    if (msg[0]!=0) { //is it 'PUR'? Or DATA? or INFO?
      byte msgLEN=strlen(msg);
      char pur[32];
      purFIND(pur,msg); //strips off the 'PUR:', is "" if not 'PUR'. // RX expects PUR:IDxxxx:PRM0
      if (pur[0]!=0) { //is this a request for parameters?
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
              //compose rss and first-half of data pairs, 
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
              while (bx<=msgLEN) {
                if ((msg[bx]=='|')||(bx==(msgLEN))) { // hit next delim? - got a pair
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
      
      if (flgDONE==false) { //Serial.println(F("looking for INFO")); 
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
char *pair_VALIDATE(char *idkey, char *rxbuf, byte rxlen, char *pkey) { char *ret=idkey; 
  idkey[0]=0; //fail flag
  char msg[64]; char txid[8]; char txkey[18];
  rx_DECODE_0(msg,rxbuf,rxlen,pkey);
  byte msgLEN=strlen(msg);
  if ((msg[0]=='!') && (msg[7]=='!')) {
    mySubStr(txid,msg,1,6);
    mySubStr(txkey,msg,8,msgLEN-8);
    strcpy(idkey,txid); strcat(idkey,":"); strcat(idkey,txkey);
  }
  return ret;
}
  
//*****************************************
char *purFIND(char *purOUT, char *purIN) {char *ret=purOUT; // RX expects PUR:IDxxxx:PRM0
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
void param0_SEND(word eePrmID) { 
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
  msg_SEND(param,14,rxKEY,2);   //
}

//*****************************************
word param_FIND_ID(char *pID) { word eeLoc=EE_PARAM_ID;
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
  char txBUF[48]; 
  strcpy(txBUF,txid); strcat(txBUF,":"); strcat(txBUF,rxkey); //ididid:rxkeyrxkeyrxkeyr
  byte txLEN=strlen(txBUF);
  msg_SEND(txBUF,txLEN,txkey,1); //the TX will decode this, it made the key
  Serial.print(F("{\"source\":\"rx\",\"info\":\"key2tx\",\"id\":\""));
  Serial.print(txid); Serial.println(F("\"}")); Serial.flush();
}

//*****************************************
void msg_SEND(char* msgIN, byte msgLEN, char *msgKEY, byte txPWR) { //txPWR is 1-10
  char txBUF[64];
  tx_ENCODE_0(txBUF,msgIN,msgLEN,msgKEY);
  rf95.setTxPower((txPWR*2), false); // from 1-10 to 2-20dB
  rf95.send(txBUF,msgLEN);//+1); //rf95 need msgLen to be one more ???
  rf95.waitPacketSent();
}

//*****************************************
//encode is just before sending, so is non-string char array for rf95.send
char *tx_ENCODE_0(char *msgOUT, char *msgIN, byte lenMSG, char *key) { char *ret=msgOUT;
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

//*****************************************
char* rx_DECODE_0(char *msgOUT, char *rxBUF, byte rxLEN, char *key) { char *ret=msgOUT;
  byte i; byte k=0;
  byte keyLEN=strlen(key); 
  for (i=0;i<rxLEN;i++) {
    if (k==keyLEN) {k=0;}
    msgOUT[i]=byte((byte(rxBUF[i])^byte(key[k])));
    k++;
  }
  msgOUT[i]=0;
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
    if ( PCbuf[0] == '?') { showVER(); }
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
      byte bp=0; byte cp[4]; byte len[4]; byte cpp=0; //Colon Positions
      while (PCbuf[bp]!=0) {//format p:ididid:iii:hhh:p:s c[0],c[1],c[2],c[3],c[4]
        if (PCbuf[bp]==':') {cp[cpp]=bp; cpp++;} // log postion of all ':'s
        bp++; //and leave with cpp=length?
      } //cp[0,1,2,3] = postions of delimiters between the four things, bp=length of PCbuf
      len[1]=(cp[1]-cp[0])-1;  //ID, 6 char
      len[2]=(cp[2]-cp[1])-1;  //data interval, 1 byte
      len[3]=(cp[3]-cp[2])-1;  //heartbeat, 1 byte
      len[4]=(cp[4]-cp[3])-1;  //power level, 0-9, 1 byte   
      len[5]=(bp-cp[4])-1;     //system byte flag bits, ascii hex text 00-FF
      char p_id[8];memcpy(p_id,&PCbuf[cp[0]+1],len[1]); p_id[len[1]]=0; //ID, 6 char
      char p_int[5];memcpy(p_int,&PCbuf[cp[1]+1],len[2]); p_int[len[2]]=0; //data interval, 1 byte
      char p_hb[5];memcpy(p_hb,&PCbuf[cp[2]+1],len[3]); p_hb[len[3]]=0; //heartbeat, 1 byte        
      char p_pwr[3];memcpy(p_pwr,&PCbuf[cp[3]+1],len[4]);  p_pwr[len[4]]=0;   //power level, 0-9, 1 byte
      char p_sys[3];memcpy(p_sys,&PCbuf[cp[4]+1],len[5]);  p_sys[len[5]]=0;   //system byte flag bits, ascii hex text 00-FF
      byte bINT=byte(atoi(p_int));
      byte bHB=byte(atoi(p_hb));
      byte bPWR=byte(atoi(p_pwr)); 
      byte bSYS=byte(strtol(p_sys,NULL,16)); //2 char hex string -> long
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

}

//*****************************************
void EE_ERASE_all() {
  Serial.print(F("EE_ERASE_all...#"));Serial.flush();
  for (word i=0;i<1024;i++) { EEPROM.write(i,255); } //the rest get FF's
  Serial.println(F(" ...Done#"));Serial.flush();
}
  
