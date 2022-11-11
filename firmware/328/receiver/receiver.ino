/* Copyright (C) 2022 Marlyn Anderson - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the ...
 *
 * You should have received a copy of the ... with
 * this file. If not, please write to: , or visit :
 * magiclime.com
 */
 
const static char VER[] = "RX221111";

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

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

// Singleton instance of the radio driver
#include <RH_RF95.h>
#define RF95_RST 9 //not used
#define RF95_CS  10
#define RF95_INT  2
#define RF95_FREQ   915.0
RH_RF95 rf95(RF95_CS, RF95_INT);

#define EE_OPTBYTE  1023 //Sensor Option Bits - misc flag/status/mode bits
//bit#0: erase the current ID (so it makes a new one on boot)
//bit#1: reset max's (to 0)
//bit#2: reset min's (to 1023) 
//bit#3: Stream data count MSB (streaming is to be a non-repeating one-time thing)
//bit#4: Stream data count LSB (where it returns to regular interval afterwords)
//bit#5: 
//bit#6: 
//bit#7:
#define EE_KEY_RSS        EE_OPTBYTE-1 // rss for key exchange
#define EE_KEY            EE_KEY_RSS-1  //KEY is 16 - no string null 
#define EE_PARAM_ID       EE_KEY-16     // 6 char
#define EE_PARAM_INTERVAL EE_PARAM_ID-6 //7, 'seconds/16' 255=68 min.
#define EE_PARAM_HRTBEAT  EE_INTERVAL-1 //8, 'seconds/64' 255=272 min.
#define EE_PARAM_POWER    EE_PARAM_HRTBEAT-1  //9, 1-10  (2-20dB in sensor).
#define EE_PARAM_OPTBYTE  EE_PARAM_POWER-1  //10, + this one = 10 bytes per ID,
// PARAM ID things go all the way down to 0

boolean flgShowChar;
boolean flgShowHex;

//here's the type look-up table...
const char TM1[] PROGMEM = "Beacon"; 
const char T0[] PROGMEM = "MY_SBN0"; 
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
const char T21[] PROGMEM = "MTN_DOT";
const char T22[] PROGMEM = "MY_SBN22";


PGM_P const table_T[] PROGMEM ={TM1,T0,T1,T2,
  T3,T4,T5,T6,T7,T8,T9,T10,T11,T12,T13,T14,T15,
  T16,T17,T18,T19,T20,T21,T22};

static char rxBUF[64]; //for the rx buf
static byte rxLEN; //for the length
static char rxKEY[18]; //16 char + null
 
const byte rssOFFSET=130;
byte keyRSS=80;
bool flgDONE;
//**********************************************************************
void setup() { 
  while (!Serial);
  Serial.begin(57600);
  showVER();
  if (EEPROM.read(EE_KEY_RSS)<200) {keyRSS=EEPROM.read(EE_KEY_RSS);} 
  showKSS();
  //key_EE_ERASE();
  //param_EE_ERASE();
  
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

  key_EE_GET(rxKEY); //Serial.print(F("key="));Serial.println(rxKEY);
  if (key_VALIDATE(rxKEY)==false) { //Serial.println(F("key_VALIDATE(rxKEY)==false"));Serial.flush(); //bad key? - make a new one?
    key_EE_MAKE();
    key_EE_GET(rxKEY); //Serial.print(F("key new="));Serial.println(rxKEY);
  }
  showINFO(rxKEY);
  flgDONE=false;
} // End Of SETUP ******************

//***************************************************************
//***************************************************************
void loop(){ byte rss=rxBUF_CHECK();
  if (rss>0) {rxBUF_PROCESS(rss);}
  pcBUF_CHECK();
} //  End Of LOOP 
//***************************************************************
//***************************************************************

// get RF95 buffer - return rss>0 if message  ***********
byte rxBUF_CHECK() { //********* get RF Messages - return RSS=0 for no rx
  byte rss=0;
  if (rf95.available()) { // Should be a message for us now 
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      memcpy(rxBUF,buf,len);
      rxLEN=len;
      rss=rf95.lastRssi()+rssOFFSET;  
    }
  //print_HEX(rxBUF,rxLEN);
  }
  return rss;
}

// *********************************************************
void rxBUF_PROCESS(byte rss) { flgDONE=true;
  //Serial.println(F("rxBUF_PROCESS..."));Serial.flush();
  char msg[64]; 
  if (rss>=keyRSS) { //first requirement for pairing
   // Serial.print(F("rss>keyRSS:"));Serial.println(rss);Serial.flush();
    //special decode key - not the regular rxKEY
    char pairKEY[18]; strcpy(pairKEY,"thisisamagiclime");
    pair_VALIDATE(msg,rxBUF,rxLEN,pairKEY); //!ididid!txkey.. --> ididid:keykeykey
    if ((msg[0]!=0) && (msg[6]==':')) {
      char txid[8]; char txkey[18];
      mySubStr(txid,msg,0,6);
      mySubStr(txkey,msg,7,strlen(msg)-7);
      key_SEND(txid,txkey,rxKEY);
      return;
    }
  } 
//************************************
//Serial.print(F("key: ")); Serial.println(rxKEY);Serial.flush();
  rx_DECODE_0(msg,rxBUF,rxLEN,rxKEY); //decode using rx KEY
  if (msg[0]!=0) { //is it 'PUR'? Or DATA? or INFO?
    byte msgLEN=strlen(msg);
    //Serial.print(F("msg: ")); Serial.print(msgLEN);Serial.print(F(" : "));Serial.println(msg);Serial.flush();
//Quick - get the DATA! (then look for other stuff)
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
 //*************************         
          byte bx=2; //Buf indeX start: [0] is protocol, [1] is first delimiter 
          char ps[24];  //Pair String accumulator
          byte psx=0; //ps's indeX
          jpx=1; //json Pair # indeX, '0' is rss - already done.  
          while (bx<=msgLEN) {
            if ((msg[bx]=='|')||(bx==(msgLEN))) { // hit next delim? - got a pair
              ps[psx]=0; //null term
              switch (jpx) {
                case 1: {strcat(jp[1],ps); } break; 
                case 2: { //Serial.print(F("sbn=")); Serial.println(atoi(ps+1)); Serial.flush();
                  strcat_P(jp[2], (char*)pgm_read_word(&(table_T[atoi(ps)+1]))); } break; 
                // about that (ps)+1... sensor -1 is beacon, but lookup table starts with 0.
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
      return;
    }//validate protocol format
//*************************    
    char prm[24];
    purFIND(prm,msg); //strips off the 'PUR:', is "" if not 'PUR'. // RX expects PUR:IDxxxx:PRM0
    if (prm[0]!=0) { //is this a request for parameters?
      pur_REQ_CHECK(prm); //sends paramters if OK
     // Serial.println(F("pur_REQ_CHECK-done"));  Serial.flush();
      return;
    }
//*************************    
    pakFIND(prm,msg); //PAK:IDxxxx:10:30:2,7 -ish , strips off the 'PAK:', is "" if not 'PAK'.
    if (prm[0]!=0) { 
      json_PRINTinfo(prm, strlen(prm)); 
      return;
    }
    
  } //end of rxDECODED OK msg[0]!=0
} //end of rxBUF_PROCESS() 

//**********************************************************************
void pcBUF_CHECK() { // Look for Commands from the Host PC
  if (Serial.available())  {
    //Serial.print(F("pcbufchk("));
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
    byte bufLEN=strlen(PCbuf);
    //Serial.print(F("pcbuf("));Serial.print(bufLEN);Serial.print(F(") "));
    //Serial.println(PCbuf);Serial.flush();

    if ( PCbuf[0] == '?') { showVER(); return;}
    if (( PCbuf[0]=='k')&&(PCbuf[1]=='s')&&(PCbuf[2]=='s')) { //Key Signal Strength
      byte i=3; char kss[4];  //get kss value at PCbuf[3,4 and maybe 5]
      while (i<6) {kss[i-3]=PCbuf[i]; i++;}
      if ((i>3)&&(i<7)) { //not bad data? term ascii value
        kss[i-3]=0;  keyRSS=byte(atoi(kss));
        EEPROM.write(EE_KEY_RSS,keyRSS);
      }
      showKSS();
      return;
    }
    
    if (( PCbuf[0] == 'p')&&( PCbuf[1] == 'r')&&( PCbuf[2] == 'm')&&(PCbuf[3] == ':')) { //Parameter stuff to follow
      byte cp[6]; byte len[6]; byte cpp=0; //Colon Positions/Pointer
      for (byte i=0;i<bufLEN;i++) {//format prm:ididid:iii:hhh:p:o c[0],c[1],c[2],c[3],c[4]
        if (PCbuf[i]==':') {cp[cpp]=i; cpp++;} // log postion of all ':'s
      } 
      len[1]=(cp[1]-cp[0])-1;  //ID, 6 char
      len[2]=(cp[2]-cp[1])-1;  //data interval, 1 byte
      len[3]=(cp[3]-cp[2])-1;  //heartbeat, 1 byte
      len[4]=(cp[4]-cp[3])-1;  //power level, 2-20, 1 byte   
      len[5]=(bufLEN-cp[4])-1;     //system byte flag bits, ascii hex text 00-FF
      //Serial.print(F("len[1]="));Serial.println(len[1]);Serial.flush();
      if (len[1]==6) { //validation of format prm:ididid:iii:hhh:p:o 
        len[1]=(cp[1]-cp[0])-1;  //ID, 6 char      
        char p_id[8]; memcpy(p_id,&PCbuf[cp[0]+1],len[1]); p_id[len[1]]=0; //ID, 6 char
        char p_int[5];memcpy(p_int,&PCbuf[cp[1]+1],len[2]); p_int[len[2]]=0; //data interval, 1 byte
        char p_hb[5];memcpy(p_hb,&PCbuf[cp[2]+1],len[3]); p_hb[len[3]]=0; //heartbeat, 1 byte        
        char p_pwr[3];memcpy(p_pwr,&PCbuf[cp[3]+1],len[4]);  p_pwr[len[4]]=0;   //power level, 2-20, 1 byte
        char p_opt[3];memcpy(p_opt,&PCbuf[cp[4]+1],len[5]);  p_opt[len[5]]=0;   //system byte flag bits, ascii hex text 00-FF
        byte bINT=byte(atoi(p_int));
        byte bHB=byte(atoi(p_hb));
        byte bPWR=byte(atoi(p_pwr)); 
        byte bOPT=byte(strtol(p_opt,NULL,16)); //2 char hex string -> long

        //put it eeprom...
        param_2EEPROM(p_id,bINT,bHB,bPWR,bOPT);
        char pkt[32];
        prm_pkt(pkt,p_id,bINT,bHB,bPWR,bOPT);
        showINFO(pkt);
      } //(len[1]==6)
    } // if PCbuf[0] == 'p')      
  } //if Serial.aval
} //End Of CheckPCbuf

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
  //Serial.print(F("...purFIND..."));Serial.flush();
  purOUT[0]=0; //default failflag
  char pfx[6];
  mySubStr(pfx,purIN,0,4);
  if (strcmp(pfx,"PUR:")==0) { mySubStr(purOUT,purIN,4,strlen(purIN)); }//strip off the 'PUR:'
  //Serial.println(purOUT);Serial.flush();
  return ret;
}

//***************************************** Paramter AcK - spit it back to rcvr as info
char *pakFIND(char *pakOUT, char *pakIN) {char *ret=pakOUT; // RX expects PAK:IDxxxx:10,30,2,7 stuff
  //Serial.println(F("...pakFIND..."));Serial.flush();
  pakOUT[0]=0; //default failflag
  char pfx[6];
  mySubStr(pfx,pakIN,0,4);
  if (strcmp(pfx,"PAK:")==0) {strcpy(pakOUT,pakIN); } //send it back as is
  return ret;
}

//*****************************************
//begins param handoff
//verify param-number
//look for id in eeprom list, return address of id
//check loc for 'new' status
// yes-make default params.
// no-send params using eeprom address provided

void pur_REQ_CHECK(char *purTXID) { //looking for txidxx:PRM0
  char pur[8];
  mySubStr(pur,purTXID,6,strlen(purTXID));
 
  if (strcmp(pur,":PRM0")==0) {
    char txid[8];
    mySubStr(txid,purTXID,0,6);
    //Serial.print(F("reqCheck...")); Serial.println(txid); Serial.flush();
    word eePrmID= param_FIND_ID(txid); //overflow writes over eeADR=10
    //Serial.print(F("eePrmID...")); Serial.println(eePrmID); Serial.flush();
    if (EEPROM.read(eePrmID)==255) { param_SET_DEFAULT(txid,eePrmID);}
    param0_SEND(eePrmID);
  }
}

//*****************************************
void param0_SEND(word eePrmID) { 
  //Serial.print(F("param0_SEND...")); Serial.println(eePrmID); Serial.flush();
  byte param[16]; word eeID;  //Param Pointer  //got 6 matches
  for (byte i=0;i<6;i++) { param[i]=EEPROM.read(eePrmID-i); } //ID to param
  param[6]=':';  //interval in wdt timeouts * multiplier in sensor (wdmTXI)
  param[7]=EEPROM.read(eePrmID-6);
  param[8]=':'; //heartbeat, in wdt timeouts * multiplier in sensor (wdmHBP)
  param[9]=EEPROM.read(eePrmID-7); 
  param[10]=':';//power
  param[11]=EEPROM.read(eePrmID-8); 
  param[12]=':'; //13th, System byte? might be zero
  param[13]=EEPROM.read(eePrmID-9);
  //ididid:i:h:p:o
  //print_HEX(param,14);
  msg_SEND_HEX(param,14,rxKEY,2); //
 // Serial.println(F(" done"));Serial.flush();
}

//*****************************************
word param_FIND_ID(char *pID) { word eeStart=EE_PARAM_ID;
  //for(word i=EE_PARAM_ID;i>990;i--) {
  //  Serial.print(i);Serial.print(F(":")); Serial.println(EEPROM.read(i));
  //}
  //Serial.print(F("param_FIND_ID...")); Serial.println(pID); Serial.flush();
  byte eeByte=EEPROM.read(eeStart); byte ptr;
  //Serial.print(F("eeStart0=")); Serial.print(eeStart); 
  //Serial.print(F(", eeByte=")); Serial.println(eeByte); Serial.flush();
  while ((eeByte!=255)&&(eeStart>20)) {
    //Serial.print(F("eeStart=")); Serial.print(eeStart); 
    //Serial.print(F(", eeByte=")); Serial.println(eeByte); Serial.flush();
    ptr=0;
    while ( eeByte==pID[ptr] ) {
      ptr++;
      if (ptr==6) {
        //Serial.print(F("eeStart6=")); Serial.print(eeStart); 
        //Serial.print(F(", eeByte=")); Serial.println(EEPROM.read(eeStart)); Serial.flush();
        return eeStart;
      }
      eeByte=EEPROM.read(eeStart-ptr); 
    }
    eeStart=eeStart-10; //try next block
    eeByte=EEPROM.read(eeStart);
  }
  //Serial.print(F("eeStart255=")); Serial.print(eeStart); Serial.flush();
  //Serial.print(F(", eeByte=")); Serial.println(eeByte); Serial.flush();
  return eeStart;
}

//*****************************************
void param_SET_DEFAULT(char *id,word eeLoc) {//ID,Interval,Power
  //Serial.print(F("prmDflt:")); Serial.print(id);Serial.print(",");Serial.println(eeLoc); Serial.flush();
  byte bp;  
  for(bp=0;bp<6;bp++) { EEPROM.write(eeLoc-bp,id[bp]); } //0-5
    EEPROM.write(eeLoc-bp,4); bp++; //interval sec/16         //6
    EEPROM.write(eeLoc-bp,16); bp++; //heartbeat sec./64      //7
    EEPROM.write(eeLoc-bp,2); bp++;//2-20 default power        //8
    EEPROM.write(eeLoc-bp,0); //Sysbyte bits all 0            //9
}

//*****************************************
void param_2EEPROM(char *id, byte intrvl, byte hb, byte pwr, byte opt) { 
  //Serial.print(F("param_2EEPROM..."));Serial.println(id);Serial.flush();
  word addr=param_FIND_ID(id);
  //Serial.print(F("addr="));Serial.println(addr);Serial.flush();
  byte eePtr;
  for (eePtr=0;eePtr<6;eePtr++) { EEPROM.write(addr-eePtr, id[eePtr]); }
  EEPROM.write(addr-eePtr,intrvl); eePtr++;
  EEPROM.write(addr-eePtr,hb); eePtr++;        
  EEPROM.write(addr-eePtr,pwr); eePtr++;
  EEPROM.write(addr-eePtr,opt);
  //Serial.print(F("bINT "));Serial.println(intrvl);Serial.flush();
  //Serial.print(F("bHB  "));Serial.println(hb);Serial.flush();
  //Serial.print(F("bPWR "));Serial.println(pwr);Serial.flush();
  //Serial.print(F("bOPT "));Serial.println(opt);Serial.flush();
}

//*****************************************
char *prm_pkt(char *pktOUT, char *id, byte intrvl, byte hb, byte pwr, byte opt) { char *ret=pktOUT;
  char n2a[10]; // for Number TO Ascii things
  strcpy(pktOUT,"PRM:");
  strcat(pktOUT,id); 
  strcat(pktOUT,":"); itoa((intrvl),n2a,10); strcat(pktOUT,n2a); n2a[0]=0;
  strcat(pktOUT,":"); itoa((hb),n2a,10);  strcat(pktOUT,n2a); n2a[0]=0;
  strcat(pktOUT,":"); itoa((pwr),n2a,10); strcat(pktOUT,n2a);
  static const char hex[] = "0123456789ABCDEF";
  byte msnb = byte((opt>>4)& 0x0F); byte lsnb=byte(opt & 0x0F);
  char msn[2]; msn[0]=hex[msnb]; msn[1]=0;
  char lsn[2]; lsn[0]=hex[lsnb]; lsn[1]=0;
  strcat(pktOUT,":"); strcat(pktOUT,msn); strcat(pktOUT,lsn);
  return ret;
}
  
//*****************************************
//uses key in idkey to encode response containing rxKEY
void key_SEND(char *txid, char *txkey, char *rxkey) {
  char txBUF[48]; 
  strcpy(txBUF,txid); strcat(txBUF,":"); strcat(txBUF,rxkey); //ididid:rxkeyrxkeyrxkeyr
  msg_SEND(txBUF,txkey,1); //the TX will decode this, it made the key
  Serial.print(F("{\"source\":\"rx\",\"info\":\"key2tx\",\"id\":\""));
  Serial.print(txid); Serial.println(F("\"}")); Serial.flush();
}

//*****************************************
void msg_SEND(char *msgIN, char *key, int pwr) { 
  //Serial.print(F("msg_SEND: ")); print_CHR(msgIN,strlen(msgIN));
  char txBUF[64]; byte txLEN=strlen(msgIN);
  tx_ENCODE_0(txBUF,msgIN,txLEN,key);
  rf95.setTxPower(pwr, false); // 2-20
  rf95.send(txBUF,txLEN); rf95.waitPacketSent();
}

//*****************************************
void msg_SEND_HEX(char *hexIN, byte len, char *key, int pwr) { 
  //Serial.print(F("msg_SEND_HEX: ")); print_HEX(hexIN,len);
  char hexBUF[64]; // memcpy(hexBUF,&hexIN,len);
  tx_ENCODE_0(hexBUF,hexIN,len,key);
  rf95.setTxPower(pwr, false); //from 1-10 to 2-20dB
  rf95.send(hexBUF,len); rf95.waitPacketSent();
}

//*****************************************
//encode is just before sending, so is non-string char array for rf95.send
//*****************************************
char *tx_ENCODE_0(char *txBUF, char *msgIN, byte msgLEN, char *key) { char *ret=txBUF;
  byte k=0;//random(0,8); //0-7 (8 numbers)
  byte keyLEN=strlen(key);  
  for (byte i=0;i<msgLEN;i++) {
    if (k==keyLEN) {k=0;}
    txBUF[i]=byte((byte(msgIN[i])^byte(key[k])));
    k++;
  }
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
void key_EE_ERASE() {
  for (byte i=0;i<16;i++) { EEPROM.write(EE_KEY-i,255); } //the rest get FF's
}

//*****************************************
void param_EE_ERASE() { //danger danger will robinson! this is ALL sensor parameters
  for (word i=EE_PARAM_ID;i>0;i--) { EEPROM.write(i,255); } //the rest get FF's
}

//*****************************************
void EE_ERASE_all() {
  Serial.print(F("EE_ERASE_all...#"));Serial.flush();
  for (word i=0;i<1024;i++) { EEPROM.write(i,255); } //the rest get FF's
  Serial.println(F(" ...Done#"));Serial.flush();
}

//*****************************************
void print_HEX(char *buf,byte len) { byte i;
  Serial.print(len);Serial.print(F(": "));
  for (i=0;i<(len-1);i++) {Serial.print(byte(buf[i]),HEX);Serial.print(F(" "));}
  Serial.println(byte(buf[len-1]),HEX); Serial.flush();
}

//*****************************************
void print_CHR(char *buf,byte len) { byte i;
  Serial.print(len);Serial.print(F(": "));
  for (i=0;i<(len-1);i++) {Serial.print( buf[i]);}
  Serial.println(buf[len-1]);Serial.flush();
}

//*****************************************
char* dtoa(double dN, char *cMJA, int iP) {
  //arguments... 
  // float-double value, char array to fill, precision (4 is .xxxx)
  //and... it rounds last digit up/down!
  char *ret = cMJA; long lP=1; byte bW=iP;
  while (bW>0) { lP=lP*10;  bW--;  }
  long lL = long(dN); double dD=(dN-double(lL))* double(lP); 
  if (dN>=0) { dD=(dD + 0.5);  } else { dD=(dD-0.5); }
  long lR=abs(long(dD));  lL=abs(lL);  
  if (lR==lP) { lL=lL+1;  lR=0;  }
  if ((dN<0) & ((lR+lL)>0)) { *cMJA++ = '-';  } 
  ltoa(lL, cMJA, 10);
  if (iP>0) { while (*cMJA != '\0') { cMJA++; } *cMJA++ = '.'; lP=10; 
  while (iP>1) { if (lR< lP) { *cMJA='0'; cMJA++; } lP=lP*10;  iP--; }
  ltoa(lR, cMJA, 10); }  return ret; }
  
//*****************************************
void freeMemory() {  char top;  int fm;
#ifdef __arm__
  fm= &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  fm= &top - __brkval;
#else  // __arm__
  fm= __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
Serial.print(F("Free Mem: "));Serial.println(fm);Serial.flush();
}
