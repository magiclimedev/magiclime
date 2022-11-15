/* Copyright (C) 2022 Marlyn Anderson - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the ...
 *
 * You should have received a copy of the ... with
 * this file. If not, please write to: , or visit :
 * magiclime.com
 */
 
const static char VER[] = "receiver2";

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

#define EE_SYSBYTE  1023 //System byte, misc flag/status/mode bits for ???? nothing yet.
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
#define EE_ID       EE_KEY-16     // 6 char
#define EE_INTERVAL EE_ID-6 //7, 'seconds/16' 255=68 min.
#define EE_HRTBEAT  EE_INTERVAL-1 //8, 'seconds/64' 255=272 min.
#define EE_POWER    EE_HRTBEAT-1  //9, 1-10  (2-20dB in sensor).
#define EE_OPTBYTE  EE_POWER-1  //10, + this one = 10 bytes per ID,
#define EE_NAME     EE_OPTBYTE-1  //20, and then this one = 10 more bytes per ID,
#define EE_BLKSIZE  20 //plus one more for nice number
// PARAM ID things go all the way down to 0

boolean flgShowChar;
boolean flgShowHex;

static char rxBUF[64]; //for the rx buf
static byte rxLEN; //for the length
static char rxKEY[18]; //16 char + null
 
const byte rssOFFSET=140;
byte keyRSS=90;
bool flgDONE;
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

  key_EE_GET(rxKEY); //Serial.print(F("key="));Serial.println(rxKEY);
  if (key_VALIDATE(rxKEY)==false) { //Serial.println(F("key_VALIDATE(rxKEY)==false"));Serial.flush(); //bad key? - make a new one?
    key_EE_MAKE();
    key_EE_GET(rxKEY); //Serial.print(F("key new="));Serial.println(rxKEY);
  }
  //showINFO(rxKEY);
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
    //Serial.print(F("pair msg: "));Serial.println(msg);
    if ((msg[0]!=0) && (msg[6]==':')) {
      char txid[8]; char txkey[18];
      mySubStr(txid,msg,0,6);//
      mySubStr(txkey,msg,7,strlen(msg)-7);
      key_SEND(txid,txkey,rxKEY);
      word newaddr=addr_FIND(txid);
      if (EEPROM.read(newaddr)==255) { //new addition?
        prm_EE_SET_DFLT(txid,newaddr);  } //sensor name="unassigned";
      return;
    }
  } 
  
//************ not key stuff ... *************
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
          char id[10];
          jpx=1; //json Pair # indeX, '0' is rss - already done.  
          while (bx<=msgLEN) {
            if ((msg[bx]=='|')||(bx==(msgLEN))) { // hit next delim? - got a pair
              ps[psx]=0; //null term
              switch (jpx) {
                case 1: {strcat(jp[1],ps); strcpy(id,ps); } break; //id
                case 2: {char sNM[12]; nameFROM_EE(sNM,id); strcat(jp[2],sNM); } break; 
                //  strcat_P(jp[2], (char*)pgm_read_word(&(table_T[atoi(ps)+1]))); } break; 
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
    purFIND(prm,msg); //strips off the 'PUR:', is "" if not 'PUR:'. // RX expects PUR:0:IDxxxx:NAME...
    if (prm[0]!=0) { //is this a request for parameters?
      pur_PROCESS(prm); //sends paramters if OK
     // Serial.println(F("pur_PROCESS-done"));  Serial.flush();
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
    if (( PCbuf[0] == 's')&&( PCbuf[1] == 'n')&&( PCbuf[2] == 'm')) {name_UPDATE(PCbuf,bufLEN);}
    if (( PCbuf[0] == 'k')&&( PCbuf[1] == 'e')&&( PCbuf[2] == 'e')) {key_EE_ERASE();}
    if (( PCbuf[0] == 'p')&&( PCbuf[1] == 'e')&&( PCbuf[2] == 'e')) {prm_EE_ERASE();} 
    if (( PCbuf[0] == 'p')&&( PCbuf[1] == 'r')&&( PCbuf[2] == 'm')) { //Parameter stuff to follow
      prm_UPDATE(PCbuf,bufLEN); 
    } // if PCbuf[0] == 'p')      
  } //if Serial.aval
} //End Of CheckPCbuf

//**********************************************************************
void name_UPDATE(char *buf, byte len){ // snm:ididid:snsnsnsnsn
  char id[8]; char nm[12];
  mySubStr(id,buf,4,6);
  mySubStr(nm,buf,11,len-11);
  word addr=addr_FIND(id);
  nameTO_EE(addr, nm);
}

//**********************************************************************
void prm_UPDATE(char *PCbuf,byte bufLEN){
  byte cp[10]; byte len[10]; byte cpp=0; //Colon Positions/Pointer
  for (byte i=0;i<bufLEN;i++) {//format prm:n:ididid:iii:hhh:p:o 
    if (PCbuf[i]==':') {cp[cpp]=i; cpp++;} // log postion of all ':'s
  } //len[x] is length of string preceding colon# x 
  len[1]=(cp[1]-cp[0])-1;  //parameters number
  len[2]=(cp[2]-cp[1])-1;  //ID, 6 char
  len[3]=(cp[3]-cp[2])-1;  //data interval, 1 byte
  len[4]=(cp[4]-cp[3])-1;  //heartbeat, 1 byte
  len[5]=(cp[5]-cp[4])-1;  //power level, 2-20, 1 byte   
  len[6]=(bufLEN-cp[5])-1;     //system byte flag bits, ascii hex text 00-FF
  //Serial.print(F("len[1]="));Serial.println(len[1]);Serial.flush();
  char pnum = PCbuf[4];
  switch (pnum) {
    case '0': {
      if (len[2]==6) { //validation of id in prm:ididid:iii:hhh:p:o 
        char p_id[8]; memcpy(p_id,&PCbuf[cp[1]+1],len[2]); p_id[len[2]]=0; //ID, 6 char
        char p_int[5]; memcpy(p_int,&PCbuf[cp[2]+1],len[3]); p_int[len[3]]=0; //data interval, 1 byte
        char p_hb[5]; memcpy(p_hb,&PCbuf[cp[3]+1],len[4]); p_hb[len[4]]=0; //heartbeat, 1 byte        
        char p_pwr[3]; memcpy(p_pwr,&PCbuf[cp[4]+1],len[5]);  p_pwr[len[5]]=0;   //power level, 2-20, 1 byte
        char p_opt[3]; memcpy(p_opt,&PCbuf[cp[5]+1],len[6]);  p_opt[len[6]]=0;   //system byte flag bits, ascii hex text 00-FF
        byte bINT=byte(atoi(p_int));
        byte bHB=byte(atoi(p_hb));
        byte bPWR=byte(atoi(p_pwr)); 
        byte bOPT=byte(strtol(p_opt,NULL,16)); //2 char hex string -> long
      
        //put it eeprom...
        prm_2EEPROM(p_id,bINT,bHB,bPWR,bOPT);
        char pkt[32];
        prm_pkt(pkt,p_id,bINT,bHB,bPWR,bOPT);
        showINFO(pkt);      
      }
    } break;
  } 
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
char *purFIND(char *purOUT, char *purIN) {char *ret=purOUT; // RX expects PUR:0:IDxxxx:SENSORNAME
  Serial.print(F("...purFIND..."));Serial.println(purIN);Serial.flush();
  purOUT[0]=0; //default failflag
  char pfx[6];
  mySubStr(pfx,purIN,0,4);
  if (strcmp(pfx,"PUR:")==0) { mySubStr(purOUT,purIN,4,strlen(purIN)); }//strip off the 'PUR:'
  Serial.println(purOUT);Serial.flush();
  return ret;
}

//***************************************** Paramter AcK - spit it back to rcvr as info
//expecting... PAK0:IdIdId:10:30:2:0 - like
char *pakFIND(char *pakOUT, char *pakIN) {char *ret=pakOUT; 
  Serial.println(F("...pakFIND..."));Serial.flush();
  pakOUT[0]=0; //default failflag
  char pfx[6];
  mySubStr(pfx,pakIN,0,3);
  if (strcmp(pfx,"PAK")==0) {strcpy(pakOUT,pakIN); } //send it back as is
  return ret;
}

//*****************************************
void pur_PROCESS(char *pktIDNM) { //looking for 0:txidxx:SENSORNAME
  //Serial.print(F("pur_PROCESS: ")); Serial.print(pktIDNM); 
  byte pktLEN=strlen(pktIDNM);
  if (pktIDNM[0]='0') { //parameter set #0?
    char txid[10];
    mySubStr(txid,pktIDNM,2,6); //2-7, 
    //Serial.print(F("txid= ")); Serial.println(txid); Serial.flush();
    word eeAddr= addr_FIND(txid); //overflow writes over eeADR=10
    //Serial.print(F("eeAddr= ")); Serial.println(eeAddr); Serial.flush();
    char snm[12];
    byte nmLEN=pktLEN-9; 
    mySubStr(snm,pktIDNM,9,nmLEN); //0:ididid: 9-end
    //Serial.print(F("snm= ")); Serial.println(snm); Serial.flush();
    if (EEPROM.read(eeAddr)==255) { //new addition
      prm_EE_SET_DFLT(txid,eeAddr);  } //sensor name="unassigned";
    else { nameTO_EE(eeAddr,snm); }
    prm_SEND(0,eeAddr); //parameter set '0' to sensor at eeAddr
  }
}

//*****************************************
void prm_SEND(byte pnum, word eeAddr) { 
  //Serial.print(F("prm0_SEND addr= ")); Serial.println(eeAddr); Serial.flush();
  byte prm[24]; word eeID;  //Param Pointer 
  switch (pnum) {
    case 0: { //PRM:0:ididid:i:h:p:o
      memcpy(prm,"PRM:0:",6); //0-5
      for (byte i=0;i<6;i++) { prm[i+6]=EEPROM.read(eeAddr-i); } //6-11, ID to prm
      prm[12]=':'; prm[13]=EEPROM.read(eeAddr-6);  //interval in wdt timeouts * multiplier in sensor (wdmTXI)
      prm[14]=':'; prm[15]=EEPROM.read(eeAddr-7);//heartbeat, in wdt timeouts * multiplier in sensor (wdmHBP)
      prm[16]=':'; prm[17]=EEPROM.read(eeAddr-8); //power
      prm[18]=':'; prm[19]=EEPROM.read(eeAddr-9); //20th byte
    } break;//13th, System byte? might be zero
  } 
  //print_HEX(prm,24);
  msg_SEND_HEX(prm,20,rxKEY,2); //
 // Serial.println(F(" done"));Serial.flush();
}

//*****************************************
word addr_FIND(char *pID) { word eeAddr=EE_ID;
//Serial.print(F("addr_FIND= ")); Serial.println(pID); Serial.flush();
 //print_HEX(pID,strlen(pID));
 //for(word x=EE_ID;x>980;x--) {Serial.print(x);Serial.print(":");Serial.println(EEPROM.read(x),HEX);} 
  byte eeByte=EEPROM.read(eeAddr); byte ptr;
  while ((eeByte!=255)&&(eeAddr>20)) {
    //Serial.print(F("......"));Serial.println(eeAddr);
    ptr=0;
    while ( eeByte==pID[ptr] ) {
      //Serial.print(ptr);Serial.print(F(":"));Serial.println(eeByte,HEX);
      ptr++;
      if (ptr==6) { return eeAddr; }
      eeByte=EEPROM.read(eeAddr-ptr);
    }
    eeAddr=eeAddr-EE_BLKSIZE; //try next block
    eeByte=EEPROM.read(eeAddr);
  }
  delay(10);
  return eeAddr;
}

//*****************************************
void prm_EE_SET_DFLT(char *id, word addr) {//ID,Interval,Power
  //Serial.print(F("prm_EE_SET_DFLT, id= ")); Serial.print(id);
  //Serial.print(F(" at ")); Serial.println(addr); Serial.flush();
  char sn[]="unassigned";
  for(byte b=0;b<6;b++) { EEPROM.write((addr-b),id[b]); } //0-5
  EEPROM.write(addr-6,4); //interval sec/16       //6
  EEPROM.write(addr-7,16); //heartbeat sec./64    //7
  EEPROM.write(addr-8,2); //2-20 default power    //8
  EEPROM.write(addr-9,0); //Sysbyte bits all 0    //9
  addr=addr-10; //where the name goes - after 6 char ID and four paramters
  for(byte b=0;b<10;b++) { EEPROM.write((addr-b),sn[b]); }//10-19    
    delay(10);
}

//*****************************************
void nameTO_EE(word addr, char *snm) { 
  //Serial.print(F("nameTO_EE at "));
  //Serial.print(addr); Serial.print(F(" is "));Serial.println(snm); Serial.flush();
  byte snLEN=strlen(snm);
  addr=addr-10; //where the name goes - after 6 char ID and four paramters
  for (byte b=0;b<10;b++) { char t=snm[b];
    if (b>=snLEN) {t=0;}
    EEPROM.write(addr-b, t); } //0-5
}

//*****************************************
char* nameFROM_EE(char *snm, char *id) { char *ret=snm;
  //Serial.print(F("nameFROM_EE: "));Serial.print(id);
  word addr=addr_FIND(id);
  //Serial.print(F(" at "));Serial.print(addr);Serial.flush();
  for (byte bp=0;bp<10;bp++) { //10 char max
    snm[bp]=EEPROM.read(addr-10-bp); //starting at '6 char id + 4 paramters = 10
    if (snm[bp]>126) {snm[bp]=0;} //10-19
  }
  snm[10]=0;
  //delay(10);Serial.print(F(" is "));Serial.println(snm);Serial.flush();
  return ret;
}
  
//*****************************************
void prm_2EEPROM(char *id, byte intvl, byte hb, byte pwr, byte opt) { 
  //Serial.print(F("prm_2EEPROM..."));Serial.println(id);Serial.flush();
  word addr=addr_FIND(id);
  //Serial.print(F("addr="));Serial.println(addr);Serial.flush();
  for (byte b=0;b<6;b++) { EEPROM.write(addr-b, id[b]); } //0-5
  EEPROM.write(addr-6,intvl); 
  EEPROM.write(addr-7,hb);        
  EEPROM.write(addr-8,pwr);
  EEPROM.write(addr-9,opt);
  //Serial.print(F("bINT "));Serial.println(intrvl);Serial.flush();
  //Serial.print(F("bHB  "));Serial.println(hb);Serial.flush();
  //Serial.print(F("bPWR "));Serial.println(pwr);Serial.flush();
  //Serial.print(F("bOPT "));Serial.println(opt);Serial.flush();
}

//*****************************************
char *prm_pkt(char *pktOUT, char *id, byte intvl, byte hb, byte pwr, byte opt) { char *ret=pktOUT;
  char n2a[10]; // for Number TO Ascii things
  strcpy(pktOUT,"PRM:"); strcat(pktOUT,id); 
  strcat(pktOUT,":"); itoa((intvl),n2a,10); strcat(pktOUT,n2a); n2a[0]=0;
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
  //Serial.print(F("ssout=")); Serial.println(out);Serial.flush();
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
  Serial.print(F("key_EE_ERASE..."));Serial.flush();
  for (byte i=0;i<16;i++) { EEPROM.write(EE_KEY-i,255); } //the rest get FF's
  Serial.println(F(" Done"));Serial.flush();
}

//*****************************************
void prm_EE_ERASE() { //danger danger will robinson! this is ALL sensor parameters
  Serial.print(F("prm_EE_ERASE...."));Serial.flush();
  for (word i=EE_ID;i>0;i--) { EEPROM.write(i,255); } //the rest get FF's
  Serial.println(F(" Done"));Serial.flush();
}

//*****************************************
void EE_ERASE_all() {
  Serial.print(F("EE_ERASE_all...#"));Serial.flush();
  for (word i=0;i<1024;i++) { EEPROM.write(i,255); } //the rest get FF's
  Serial.println(F(" Done")); Serial.flush();
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
