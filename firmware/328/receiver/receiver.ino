/*
 *  This is code for ThingBits sensors.
 *
 *  ThingBits invests time and resources providing this open source code,
 *  please support ThingBits and open-source hardware by purchasing products
 *  from ThingBits!
 *
 *  Written by ThingBits Inc (http://www.thingbits.com)
 *
 *  MIT license, all text above must be included in any redistribution
 */
 
const static char VER[] = "RX221202";
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
#define EE_HRTBEAT  EE_INTERVAL-1 //8, 'seconds/128' 255=544 min.
#define EE_POWER    EE_HRTBEAT-1  //9, 1-10  (2-20dB in sensor).
#define EE_OPTBYTE  EE_POWER-1  //10, + this one = 10 bytes per ID,
#define EE_NAME     EE_OPTBYTE-1  //20, and then this one = 10 more bytes per ID,
#define EE_BLKSIZE  20 //plus one more for nice number
// PARAM ID things go all the way down to 0

//here's the help menu table...
const char H00[] PROGMEM = "---- commands ----"; 
const char H01[] PROGMEM = "prm:n:ididid:int:hb:p:o = PaRaMeters settings.";
const char H02[] PROGMEM = "  - n ='parameter group #, aldways '0'(for now).";
const char H03[] PROGMEM = "  - ididid = 6 char. id string.";
const char H04[] PROGMEM = "  - int = interval, periodic TX counter limit (x 8 sec.).";
const char H05[] PROGMEM = "  - hb = heartbeat counter limit (x 128 sec.).";
const char H06[] PROGMEM = "  - p = power setting, 2-20.";
const char H07[] PROGMEM = "  - o = option byte, sensor-dependent flag bits.";
const char H08[] PROGMEM = "kss:xxx       -Key Signal Strength ref. level";  
const char H09[] PROGMEM = "snr:ididid:sensorname -Sensor Name Replace."; 
const char H10[] PROGMEM = "idd:ididid    -ID Delete from eeprom";
const char H11[] PROGMEM = "idl           -ID List";
const char H12[] PROGMEM = "ide           -ID's Erase !ALL!";
const char H13[] PROGMEM = "kye           -KeY Erase ";
const char H14[] PROGMEM = "eee           -Erase Entire Eeprom";
const char H15[] PROGMEM = "";
const char H16[] PROGMEM = " -- examples --";
const char H17[] PROGMEM = "prm:0:SGPOJS:5:20:2:0";
const char H18[] PROGMEM = "kss:90"; 
const char H19[] PROGMEM = "snr:SGPOJS:MOT_STAIRS (10-char. max)"; 
const char H20[] PROGMEM = "idd:SGPOJS";
const char H21[] PROGMEM = "";

PGM_P const table_HLP[] PROGMEM ={H00,H01,H02,H03,H04,H05,H06,H07,H08,H09,
                                  H10,H11,H12,H13,H14,H15,H16,H17,H18,H19,
                                  H20,H21};
const byte hlpLIM=22;

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
  jsonVER();
  if (EEPROM.read(EE_KEY_RSS)<200) {keyRSS=EEPROM.read(EE_KEY_RSS);} 
  jsonKSS();
  
  if (rf95.init()) { rf95.setFrequency(RF95_FREQ); 
    char info[10]; strcpy(info,"rf95.init !OK!");
    jsonINFO(info); 
  }
  else {
    char info[10]; strcpy(info,"rf95.init !FAIL!");
    jsonINFO(info);
    while (1);
  }

  //Serial.println("RFM95 register settings...");
  //rf95.printRegisters();
  flgShowChar=false;
  flgShowHex=false;

  key_EE_GET(rxKEY); //Serial.print(F("* key="));Serial.println(rxKEY);
  if (key_VALIDATE(rxKEY)==false) { //Serial.println(F("* key_VALIDATE(rxKEY)==false"));Serial.flush(); //bad key? - make a new one?
    key_EE_MAKE();
    key_EE_GET(rxKEY); //Serial.print(F("* key new="));Serial.println(rxKEY);
  }
  //jsonINFO(rxKEY);
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
//looking for these things, in this order...
//  1.  a key - if rss is big.
//  2.  data
//  3.  a request for stored parameter settings. ('PUR')
//  4.  an ack of those parameters with time in minutes. ('PAK')

void rxBUF_PROCESS(byte rss) { flgDONE=true;
  //Serial.println(F("* rxBUF_PROCESS..."));Serial.flush();
  char msg[64]; 
  if (rss>=keyRSS) { //first requirement for pairing
   // Serial.print(F("* rss>keyRSS:"));Serial.println(rss);Serial.flush();
    //special decode key - not the regular rxKEY
    char pairKEY[18]; strcpy(pairKEY,"thisisamagiclime");
    pair_PROCESS(msg,rxBUF,rxLEN,pairKEY); //!ididid!txkey.. --> ididid:keykeykey
    //Serial.print(F("* pair msg: "));Serial.println(msg);
    if ((msg[0]!=0) && (msg[6]==':')) {
      char txid[8]; char txkey[18];
      mySubStr(txid,msg,0,6);//
      mySubStr(txkey,msg,7,strlen(msg)-7);
      key_SEND(txid,txkey,rxKEY);
      word addr=addr_FIND_ID(txid);
      if (addr==0) {addr=addr_FIND_NEW();}
      if (byte(EEPROM.read(addr))==byte(0xFF)) { //new addition?
        prm_EE_SET_DFLT(txid,addr);  } //sensor name="unassigned";
      return;
    }
  } 
  
//************ not key stuff ... *************
//Serial.print(F("* key: ")); Serial.println(rxKEY);Serial.flush();
  rx_DECODE_0(msg,rxBUF,rxLEN,rxKEY); //decode using rx KEY
  if (msg[0]!=0) { //is it 'PUR'? Or DATA? or INFO?
    byte msgLEN=strlen(msg);
    //Serial.print(F("* msg: ")); Serial.print(msgLEN);Serial.print(F(" : "));Serial.println(msg);Serial.flush();
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
    pur_LOOK(prm,msg); //strips off the 'PUR:', is "" if not 'PUR:'. // RX expects PUR:0:IDxxxx:NAME...
    if (prm[0]!=0) { //is this a request for parameters?
      pur_PROCESS(prm); //sends paramters if OK
     // Serial.println(F("pur_PROCESS-done"));  Serial.flush();
      return;
    }
//*************************    
    pak_LOOK(prm,msg); //PAK:0:IDxxxx:10:30:2,7 -ish , 
    if (prm[0]!=0) { 
      //json_PRINTinfo(prm, strlen(prm)); 
      jsonINFO(prm);
     
    
  } //end of rxDECODED OK msg[0]!=0
} //end of rxBUF_PROCESS() 

//**********************************************************************
void pcBUF_CHECK() { // Look for Commands from the Host PC
  if (Serial.available())  {
    //Serial.print(F("* pcbufchk("));
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
    //Serial.print(F("* pcbuf("));Serial.print(bufLEN);Serial.print(F(") "));
    //Serial.println(PCbuf);Serial.flush();
    
    if ( PCbuf[0] == '?') { Serial.println("");Serial.println(VER); Serial.flush();
      if (PCbuf[1]=='?') { showHELP(hlpLIM); }
      return; 
    }
    char pfx[6]; mySubStr(pfx,PCbuf,0,3);
    if (strcmp(pfx,"kss")==0) {key_SETREF(PCbuf,bufLEN);} //Key Signal Strength
    if (strcmp(pfx,"snr")==0) {name_REPLACE(PCbuf,bufLEN);}
    if (strcmp(pfx,"idd")==0) {id_DELETE(PCbuf);}
    if (strcmp(pfx,"idl")==0) {id_LIST();}  
    if (strcmp(pfx,"kye")==0) {key_EE_ERASE();}
    if (strcmp(pfx,"ide")==0) {id_EE_ERASE();}
    if (strcmp(pfx,"eee")==0) {all_EE_ERASE();}
    if (strcmp(pfx,"prm")==0) {prm_UPDATE(PCbuf,bufLEN);}//Parameter stuff to follow
  } //if Serial.aval
} //End Of CheckPCbuf

//**********************************************************************
void key_SETREF(char *buf,byte len) {
  if (buf[3]==':') { //one more validate
    char kss[4];  mySubStr(kss,buf,4,len-4);  //get kss value at PCbuf[3,4 and maybe 5]
    keyRSS=byte(atoi(kss));
    if ((keyRSS<80) and (keyRSS>120)) {keyRSS=90;}
    EEPROM.write(EE_KEY_RSS,keyRSS);
    jsonKSS();
  }
  else {jsonKSS();}
}

//**********************************************************************
void name_REPLACE(char *buf, byte len){ // snr:ididid:snsnsnsnsn
  char id[8]; char nm[12];
  if ((buf[3]==':') && (buf[10]==':')) {//one more validate
    mySubStr(id,buf,4,6);
    mySubStr(nm,buf,11,len-11);
    word addr=addr_FIND_ID(id);
    if (addr>0) {nameTO_EE(addr, nm);}
  }
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
  //Serial.print(F("* len[1]="));Serial.println(len[1]);Serial.flush();
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
        prm_pkt(pkt,0,p_id,bINT,bHB,bPWR,bOPT);
        jsonINFO(pkt);      
      }
    } break;
  } 
}
             
//*****************************************pair_PROCESS(msg,rxBUF,rxLEN,pairKEY);
char *pair_PROCESS(char *idkey, char *rxbuf, byte rxlen, char *pkey) { char *ret=idkey; 
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
char *pur_LOOK(char *purOUT, char *purIN) {char *ret=purOUT; // RX expects PUR:0:IDxxxx:SENSORNAME
  //Serial.print(F("* ...pur_LOOK..."));Serial.println(purIN);Serial.flush();
  purOUT[0]=0; //default failflag
  char pfx[6];
  mySubStr(pfx,purIN,0,4);
  if (strcmp(pfx,"PUR:")==0) { mySubStr(purOUT,purIN,4,strlen(purIN)); }//strip off the 'PUR:'
  //Serial.println(purOUT);Serial.flush();
  return ret;
}

//***************************************** Paramter AcK - spit back to rcvr as info
//expecting... PAK:0:IdIdId:10:30:2:0 - like
char *pak_LOOK(char *pakOUT, char *pakIN) {char *ret=pakOUT; 
  //Serial.println(F("* ...pak_LOOK..."));Serial.flush();
  pakOUT[0]=0; //default failflag
  char pfx[6];  mySubStr(pfx,pakIN,0,3);
  if (strcmp(pfx,"PAK")==0) { strcpy(pakOUT,pakIN); }
  return ret;
}

//*****************************************
void pur_PROCESS(char *pktIDNM) { //looking for 0:txidxx:SENSORNAME
  //Serial.print(F("* pur_PROCESS: ")); Serial.print(pktIDNM); 
  byte pktLEN=strlen(pktIDNM);
  if (pktIDNM[0]='0') { //parameter set #0?
    char txid[10];
    mySubStr(txid,pktIDNM,2,6); //2-7, 
    //Serial.print(F("* txid= ")); Serial.println(txid); Serial.flush();
    word addr= addr_FIND_ID(txid); //overflow writes over addr=10
    if (addr==0) {addr=addr_FIND_NEW(); }
    //Serial.print(F("* addr= ")); Serial.println(addr); Serial.flush();
    char snm[12];
    byte nmLEN=pktLEN-9; 
    mySubStr(snm,pktIDNM,9,nmLEN); //0:ididid: 9-end
    //Serial.print(F("* snm= ")); Serial.println(snm); Serial.flush();
    if (byte(EEPROM.read(addr))==byte(0xFF)) { //new addition
      prm_EE_SET_DFLT(txid,addr);  } //sensor name="unassigned";
    else { nameTO_EE(addr,snm); }
    prm_SEND(0,addr); //parameter set '0' to sensor at eeAddr
  }
}

//*****************************************
void prm_SEND(byte pnum, word eeAddr) { 
  //Serial.print(F("* prm0_SEND addr= ")); Serial.println(eeAddr); Serial.flush();
  byte prm[24]; word eeID;  //Param Pointer 
  switch (pnum) {
    case 0: { //PRM:0:ididid:i:h:p:o
      memcpy(prm,"PRM:0:",6); //0-5
      for (byte i=0;i<6;i++) { prm[i+6]=EEPROM.read(eeAddr-i); } //6-11, ID to prm
      prm[12]=':'; prm[13]=EEPROM.read(eeAddr-6);//interval in wdt timeouts * multiplier in sensor (wdmTXI)
      prm[14]=':'; prm[15]=EEPROM.read(eeAddr-7);//heartbeat, in wdt timeouts * multiplier in sensor (wdmHBP)
      prm[16]=':'; prm[17]=EEPROM.read(eeAddr-8); //power
      prm[18]=':'; prm[19]=EEPROM.read(eeAddr-9); //option byte, 20th byte
    } break;
  } 
  //print_HEX(prm,24);
  msg_SEND_HEX(prm,20,rxKEY,2); //
 // Serial.println(F(" done"));Serial.flush();
}

//*****************************************
word addr_FIND_NEW() { word ret=0;
  for (word addr=EE_ID;addr>EE_BLKSIZE;addr-=EE_BLKSIZE) {
    if (byte(EEPROM.read(addr))==byte(0xFF)) {ret=addr; break;}
  }
  return ret;  
}

//*****************************************
word addr_FIND_ID(char *id) { word ret=0; word addr;
//Serial.print(F("* addr_FINDing ")); Serial.print(id); Serial.flush();
 //print_HEX(pID,strlen(id));
 //for(word x=EE_ID;x>980;x--) {Serial.print(x);Serial.print(":");Serial.println(EEPROM.read(x),HEX);} 
  byte ptr; byte eeByte;
  for (addr=EE_ID;addr>EE_BLKSIZE;addr-=EE_BLKSIZE) {
    ptr=0; eeByte=byte(EEPROM.read(addr-ptr));
    while ( eeByte==byte(id[ptr]) ) {
      //Serial.print(ptr);Serial.print(F(":"));Serial.println(eeByte,HEX);
      ptr++;
      if (ptr==6) { ret=addr; break; }
      eeByte=byte(EEPROM.read(addr-ptr));
    }
    if (ret>0) {break;}
  }
  return ret;
}

//*****************************************
void prm_EE_SET_DFLT(char *id, word addr) {//ID,Interval,Power
  //Serial.print(F("* prm_EE_SET_DFLT, id= ")); Serial.print(id);
  //Serial.print(F("*  at ")); Serial.println(addr); Serial.flush();
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
  //Serial.print(F("* nameTO_EE at "));
  //Serial.print(addr); Serial.print(F(" is "));Serial.println(snm); Serial.flush();
  byte snLEN=strlen(snm);
  addr=addr-10; //where the name goes - after 6 char ID and four paramters
  for (byte b=0;b<10;b++) { char t=snm[b];
    if (b>=snLEN) {t=0;}
    EEPROM.write(addr-b, t); } //0-5
}

//*****************************************
char* nameFROM_EE(char *snm, char *id) { char *ret=snm;
  //Serial.print(F("* nameFROM_EE: "));Serial.print(id);
  word addr=addr_FIND_ID(id);
  //Serial.print(F(" at "));Serial.print(addr);Serial.flush();
  if (addr>0) {
    for (byte bp=0;bp<10;bp++) { //10 char max
      snm[bp]=EEPROM.read(addr-10-bp); //starting at '6 char id + 4 paramters = 10
      if (snm[bp]>126) {snm[bp]=0;} //10-19
    }
    snm[10]=0;
  }  
    //delay(10);Serial.print(F(" is "));Serial.println(snm);Serial.flush();
  return ret;
}
  
//*****************************************
void prm_2EEPROM(char *id, byte intvl, byte hb, byte pwr, byte opt) { 
  //Serial.print(F("* prm_2EEPROM..."));Serial.println(id);Serial.flush();
  word addr=addr_FIND_ID(id);
  if (addr>0) {
    //Serial.print(F("* addr="));Serial.println(addr);Serial.flush();
    for (byte b=0;b<6;b++) { EEPROM.write(addr-b, id[b]); } //0-5
    EEPROM.write(addr-6,intvl); 
    EEPROM.write(addr-7,hb);        
    EEPROM.write(addr-8,pwr);
    EEPROM.write(addr-9,opt);
    //Serial.print(F("* bINT "));Serial.println(intrvl);Serial.flush();
    //Serial.print(F("* bHB  "));Serial.println(hb);Serial.flush();
    //Serial.print(F("* bPWR "));Serial.println(pwr);Serial.flush();
    //Serial.print(F("* bOPT "));Serial.println(opt);Serial.flush();
  }
}

//*****************************************
char *prm_pkt(char *pktOUT, byte pktnum, char *id, byte intvl, byte hb, byte pwr, byte opt) {
  char *ret=pktOUT;  char n2a[10]; // for Number TO Ascii things
  strcpy(pktOUT,"PRM"); //instead of 'itoa' just add 48?
  strcat(pktOUT,":"); itoa(n2a,pktnum,10); strcat(pktOUT,n2a); 
  strcat(pktOUT,":"); strcat(pktOUT,id); 
  strcat(pktOUT,":"); itoa(intvl,n2a,10); strcat(pktOUT,n2a); n2a[0]=0;
  strcat(pktOUT,":"); itoa(hb,n2a,10);  strcat(pktOUT,n2a); n2a[0]=0;
  strcat(pktOUT,":"); itoa(pwr,n2a,10); strcat(pktOUT,n2a);
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
  //Serial.print(F("* msg_SEND: ")); print_CHR(msgIN,strlen(msgIN));
  char txBUF[64]; byte txLEN=strlen(msgIN);
  tx_ENCODE_0(txBUF,msgIN,txLEN,key);
  rf95.setTxPower(pwr, false); // 2-20
  rf95.send(txBUF,txLEN); rf95.waitPacketSent();
}

//*****************************************
void msg_SEND_HEX(char *hexIN, byte len, char *key, int pwr) { 
  //Serial.print(F("* msg_SEND_HEX: ")); print_HEX(hexIN,len);
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
bool id_VALIDATE(char *id) { //check EEPROM for proper character range
  for (byte i=0;i<6;i++) { 
    if ( ((id[i]<'2')||(id[i]>'Z')) || ((id[i]>'9')&&(id[i]<'A')) ) { return false; }
  }
  return true;
}

//*****************************************
void json_PRINTdata(char jsn[][24], byte pNum) {
  Serial.print(F("{\"source\":\"tx\","));
  for (byte pn=0;pn<pNum;pn++) { //Pair Number
    Serial.print( jsn[pn] ); }
  Serial.println(F("\"}")); Serial.flush();  
}

//*****************************************
void json_PRINTinfo(char info[32], byte iNum) {
  Serial.print(F("{\"source\":\"tx\",\"info\":\""));
  for (byte i=0;i<iNum;i++) {Serial.print( info[i] ); }
  Serial.println(F("\"}")); Serial.flush();  
}

//**********************************************************************
void jsonINFO(char *info) { 
  Serial.print(F("{\"source\":\"rx\",\"info\":\""));
  Serial.print(info);
  Serial.println(F("\"}")); Serial.flush();
}

//**********************************************************************
void jsonKSS() { 
  Serial.print(F("{\"source\":\"rx\",\"info\":\"kss="));
  Serial.print(keyRSS);
  Serial.println(F("\"}")); Serial.flush();
}

//**********************************************************************
void jsonVER() { 
  Serial.print(F("{\"source\":\"rx\",\"info\":\"ver="));
  Serial.print(VER);
  Serial.println(F("\"}")); Serial.flush();
}

//*****************************************
void id_DELETE(char *buf) {
  char id[8]; mySubStr(id,buf,4,6);
  if (buf[3]==':') { word addr=addr_FIND_ID(id);
    if (byte(EEPROM.read(addr))!=byte(0xFF)) { //not an empty place
      for (byte i=0;i<EE_BLKSIZE;i++) { EEPROM.write(addr-i,0xFF); }
      Serial.print("removed "); Serial.println(id); Serial.flush(); 
    }
  }
}

//*****************************************
void id_LIST() { char id[8]; char nm[12]; byte blknum=0;
  for (word addr=EE_ID;addr>EE_BLKSIZE;addr-=EE_BLKSIZE) {
    for (byte i=0;i<6;i++) { id[i]=EEPROM.read(addr-i); }
    id[6]=0; //null term stringify
    blknum++;
    if (byte(id[0])!=byte(0xFF)) { byte n;
      for (n=0;n<10;n++) {
        nm[n]= EEPROM.read(addr-10-n);
        if ((byte(nm[n])==0) || (byte(nm[n])==byte(0xFF))) {break;}
      }
      nm[n]=0;
      Serial.print(blknum); Serial.print(":");
      Serial.print(id);  Serial.print(":");
      Serial.println(nm);Serial.flush(); 
    }
  }
}

//*****************************************
void key_EE_ERASE() {
  Serial.print(F("* key_EE_ERASE..."));Serial.flush();
  for (byte i=0;i<16;i++) { EEPROM.write(EE_KEY-i,0xFF); } //the rest get FF's
  Serial.println(F(" Done"));Serial.flush();
}

//*****************************************
void id_EE_ERASE() { //danger danger will robinson! this is ALL sensor parameters
  Serial.print(F("id_EE_ERASE "));
  for (word i=EE_ID;i>0;i--) { EEPROM.write(i,0xFF);
    if ((i % 64)==0){Serial.print(F(". "));} } //the rest get FF's
  Serial.println(F(" Done"));Serial.flush();
}

//*****************************************
void all_EE_ERASE() {
  Serial.print(F("* EE_ERASE_all "));
  for (word i=0;i<1024;i++) { EEPROM.write(i,0xFF); 
    if ((i % 64)==0){Serial.print(F(". "));} } //the rest get FF's
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

void showHELP(byte lim) { char sHLP[80]; 
  for (byte i=0;i<lim-1;i++) { *sHLP=0;
    strcat_P(sHLP, (char*)pgm_read_word(&(table_HLP[i]))); Serial.println(sHLP); }
  Serial.println("");Serial.flush();
}

//*****************************************
char *mySubStr(char *out, char* in,byte from,byte len) { char *ret=out;
  byte p=0; 
  for (byte i=from;i<(from+len);i++) {out[p]=in[i]; p++;}
  out[p]=0;
  //Serial.print(F("* ssout=")); Serial.println(out);Serial.flush();
  return ret;
}

//*****************************************
void freeMemory() {  char top;  int fm;
#ifdef __arm__
  fm= &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  fm= &top - __brkval;
#else  // __arm__
  fm= __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
Serial.print(F("* Free Mem: "));Serial.println(fm);Serial.flush();
}


//*****************************************
ISR(WDT_vect) { //in avr library
}
