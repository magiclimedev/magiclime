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

#define EE_SYSBYTE  1023
#define EE_KEY_RSS        EE_SYSBYTE-1
#define EE_KEY            EE_KEY_RSS-1

//the following consitutes a 'block' accessed by 6-byte ID matching
#define EE_ID       EE_KEY-16
#define EE_INTERVAL EE_ID-6
#define EE_HRTBEAT  EE_INTERVAL-1
#define EE_POWER    EE_HRTBEAT-1
#define EE_OPTBYTE  EE_POWER-1
#define EE_NAME     EE_OPTBYTE-1

#define EE_BLKSIZE  20      

//help menu table...
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

static char rxBUF[64];
static byte rxLEN;
static char rxKEY[18];
 
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

  flgShowChar=false;
  flgShowHex=false;

  key_EE_GET(rxKEY);
  if (key_VALIDATE(rxKEY)==false) {
    key_EE_MAKE();
    key_EE_GET(rxKEY);
  }
  //jsonINFO(rxKEY);
  flgDONE=false;
} // End Of SETUP ******************

//***************************************************************
//***************************************************************
void loop(){ byte rss=rxBUF_CHECK();
  if (rss>0) {rxBUF_PROCESS(rss);}
  pcBUF_CHECK();
}
//***************************************************************
//***************************************************************

// get RF95 buffer - return rss>0 if message  ***********
byte rxBUF_CHECK() {
  byte rss=0;
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      memcpy(rxBUF,buf,len);
      rxLEN=len;
      rss=rf95.lastRssi()+rssOFFSET;  
    }
  }
  return rss;
}

// *********************************************************
void rxBUF_PROCESS(byte rss) { flgDONE=true;
  char msg[64]; 
  if (rss>=keyRSS) {
    char pairKEY[18]; strcpy(pairKEY,"thisisamagiclime");
    pair_PROCESS(msg,rxBUF,rxLEN,pairKEY);
    if ((msg[0]!=0) && (msg[6]==':')) {
      char txid[8]; char txkey[18];
      mySubStr(txid,msg,0,6);
      mySubStr(txkey,msg,7,strlen(msg)-7);
      key_SEND(txid,txkey,rxKEY);
      word addr=addr_FIND_ID(txid);
      if (addr==0) {addr=addr_FIND_NEW();}
      if (byte(EEPROM.read(addr))==byte(0xFF)) {
        prm_EE_SET_DFLT(txid,addr);  }
      return;
    }
  } 
  
//************ not a key thing - get data, PUR or INFO *************
  rx_DECODE_0(msg,rxBUF,rxLEN,rxKEY);
  if (msg[0]!=0) {
    byte msgLEN=strlen(msg);
    if ((msg[0]>='0') && (msg[0]<='9') && (msg[1]=='|')) {
      char protocol=msg[0];
      byte pNum;
      char jp[6][24]={0};
      byte jpx;
      switch (protocol) { 
        case '1': {   pNum=5;
          strcpy(jp[0],"\"rss\":");
          char chr[5]; itoa(rss,chr,10);
          strcat(jp[0],chr);
          strcpy(jp[1],",\"uid\":\"");
          strcpy(jp[2],",\"sensor\":\"");
          strcpy(jp[3],",\"bat\":");
          strcpy(jp[4],",\"data\":\"");
 //*************************         
          byte bx=2;
          char ps[24];
          byte psx=0;
          char id[10];
          jpx=1;
          while (bx<=msgLEN) {
            if ((msg[bx]=='|')||(bx==(msgLEN))) {
              ps[psx]=0;
              switch (jpx) {
                case 1: {strcat(jp[1],ps); strcpy(id,ps); } break;
                case 2: {char sNM[12]; nameFROM_EE(sNM,id); strcat(jp[2],sNM); } break; 
                case 3: {strcat(jp[3],ps); } break; 
                case 4: {strcat(jp[4],ps); } break; 
              }
              psx=0;  jpx++;
            }
            else { ps[psx]=msg[bx]; psx++; }
            bx++;
          }

          strcat(jp[1],"\"");
          strcat(jp[2],"\"");
          strcat(jp[4],"\"}");
          
          json_PRINTdata(jp,pNum);
            
        }
      }
      return;
    }
//*************************    
    char prm[24];
    pur_LOOK(prm,msg);
    if (prm[0]!=0) {
      pur_PROCESS(prm);
      return;
    }
//*************************    
    pak_LOOK(prm,msg);
    if (prm[0]!=0) { 
      jsonINFO(prm);
    }
  }
}

//**********************************************************************
void pcBUF_CHECK() { // Look for Commands from the Host PC
  if (Serial.available())  {
    char cbyte;   byte c_pos = 0;  char PCbuf[50];
    cbyte = Serial.read();
    while ((cbyte != '\n') && (cbyte != '\r'))  {
      PCbuf[c_pos] = cbyte; c_pos++; //build string
      if (c_pos >= sizeof(PCbuf)) {c_pos = 0; }
      cbyte = Serial.peek(); 
      byte trynum=0;
      while ((int(cbyte)< 0)&&(trynum<255)) {
        cbyte = Serial.peek();
      }
      if (trynum==255) {cbyte='\n';}
      else {cbyte = Serial.read(); }
    }
    
    PCbuf[c_pos] = 0;
    byte bufLEN=strlen(PCbuf);
    
    if ( PCbuf[0] == '?') { Serial.println("");Serial.println(VER); Serial.flush();
      if (PCbuf[1]=='?') { showHELP(hlpLIM); }
      return; 
    }
    char pfx[6]; mySubStr(pfx,PCbuf,0,3);
    if (strcmp(pfx,"kss")==0) {key_SETREF(PCbuf,bufLEN);}
    if (strcmp(pfx,"snr")==0) {name_REPLACE(PCbuf,bufLEN);}
    if (strcmp(pfx,"idd")==0) {id_DELETE(PCbuf);}
    if (strcmp(pfx,"idl")==0) {id_LIST();}  
    if (strcmp(pfx,"kye")==0) {eeprom_ERASE_KEY();}
    if (strcmp(pfx,"ide")==0) {eeprom_ERASE_ID();} //!!! this needs work !!!
    if (strcmp(pfx,"eee")==0) {eeprom_ERASE_ALL();}
    if (strcmp(pfx,"prm")==0) {prm_UPDATE(PCbuf,bufLEN);}
  }
}

//**********************************************************************
void key_SETREF(char *buf,byte len) {
  if (buf[3]==':') { 
    char kss[4];  mySubStr(kss,buf,4,len-4);
    keyRSS=byte(atoi(kss));
    if ((keyRSS<80) and (keyRSS>120)) {keyRSS=90;}
    EEPROM.write(EE_KEY_RSS,keyRSS);
    jsonKSS();
  }
  else {jsonKSS();}
}

//**********************************************************************
void name_REPLACE(char *buf, byte len){
  char id[8]; char nm[12];
  if ((buf[3]==':') && (buf[10]==':')) {
    mySubStr(id,buf,4,6);
    mySubStr(nm,buf,11,len-11);
    word addr=addr_FIND_ID(id);
    if (addr>0) {nameTO_EE(addr, nm);}
  }
}

//**********************************************************************
void prm_UPDATE(char *PCbuf,byte bufLEN){
  byte cp[10]; byte len[10]; byte cpp=0;
  for (byte i=0;i<bufLEN;i++) {
    if (PCbuf[i]==':') {cp[cpp]=i; cpp++;}
  }
  len[1]=(cp[1]-cp[0])-1;
  len[2]=(cp[2]-cp[1])-1;
  len[3]=(cp[3]-cp[2])-1;
  len[4]=(cp[4]-cp[3])-1;
  len[5]=(cp[5]-cp[4])-1; 
  len[6]=(bufLEN-cp[5])-1;
  char pnum = PCbuf[4];
  switch (pnum) {
    case '0': {
      if (len[2]==6) {
        char p_id[8]; memcpy(p_id,&PCbuf[cp[1]+1],len[2]); p_id[len[2]]=0;
        char p_int[5]; memcpy(p_int,&PCbuf[cp[2]+1],len[3]); p_int[len[3]]=0;
        char p_hb[5]; memcpy(p_hb,&PCbuf[cp[3]+1],len[4]); p_hb[len[4]]=0;
        char p_pwr[3]; memcpy(p_pwr,&PCbuf[cp[4]+1],len[5]);  p_pwr[len[5]]=0;
        char p_opt[3]; memcpy(p_opt,&PCbuf[cp[5]+1],len[6]);  p_opt[len[6]]=0;
        byte bINT=byte(atoi(p_int));
        byte bHB=byte(atoi(p_hb));
        byte bPWR=byte(atoi(p_pwr));
        byte bOPT=byte(strtol(p_opt,NULL,16));

        prm_2EEPROM(p_id,bINT,bHB,bPWR,bOPT);
        char pkt[32];
        prm_pkt(pkt,0,p_id,bINT,bHB,bPWR,bOPT);
        jsonINFO(pkt);      
      }
    } break;
  } 
}
             
//*****************************************
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
char *pur_LOOK(char *purOUT, char *purIN) {char *ret=purOUT;
  purOUT[0]=0;
  char pfx[6];
  mySubStr(pfx,purIN,0,4);
  if (strcmp(pfx,"PUR:")==0) { mySubStr(purOUT,purIN,4,strlen(purIN)); }
  return ret;
}

//*****************************************
char *pak_LOOK(char *pakOUT, char *pakIN) {char *ret=pakOUT; 
  pakOUT[0]=0; //default failflag
  char pfx[6];  mySubStr(pfx,pakIN,0,3);
  if (strcmp(pfx,"PAK")==0) { strcpy(pakOUT,pakIN); }
  return ret;
}

//*****************************************
void pur_PROCESS(char *pktIDNM) {
  byte pktLEN=strlen(pktIDNM);
  if (pktIDNM[0]='0') {
    char txid[10];
    mySubStr(txid,pktIDNM,2,6);
    word addr= addr_FIND_ID(txid);
    if (addr==0) {addr=addr_FIND_NEW(); }
    char snm[12];
    byte nmLEN=pktLEN-9; 
    mySubStr(snm,pktIDNM,9,nmLEN);
    if (byte(EEPROM.read(addr))==byte(0xFF)) {
      prm_EE_SET_DFLT(txid,addr);  }
    else { nameTO_EE(addr,snm); }
    prm_SEND(0,addr);
  }
}

//*****************************************
void prm_SEND(byte pnum, word eeAddr) { 
  byte prm[24]; word eeID;
  switch (pnum) {
    case 0: {
      memcpy(prm,"PRM:0:",6);
      for (byte i=0;i<6;i++) { prm[i+6]=EEPROM.read(eeAddr-i); }
      prm[12]=':'; prm[13]=EEPROM.read(eeAddr-6);
      prm[14]=':'; prm[15]=EEPROM.read(eeAddr-7);
      prm[16]=':'; prm[17]=EEPROM.read(eeAddr-8);
      prm[18]=':'; prm[19]=EEPROM.read(eeAddr-9);
    } break;
  } 
  msg_SEND_HEX(prm,20,rxKEY,2);
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
  byte ptr; byte eeByte;
  for (addr=EE_ID;addr>EE_BLKSIZE;addr-=EE_BLKSIZE) {
    ptr=0; eeByte=byte(EEPROM.read(addr-ptr));
    while ( eeByte==byte(id[ptr]) ) {
      ptr++;
      if (ptr==6) { ret=addr; break; }
      eeByte=byte(EEPROM.read(addr-ptr));
    }
    if (ret>0) {break;}
  }
  return ret;
}

//*****************************************
void prm_EE_SET_DFLT(char *id, word addr) {
  char sn[]="unassigned";
  for(byte b=0;b<6;b++) { EEPROM.write((addr-b),id[b]); }
  EEPROM.write(addr-6,4);
  EEPROM.write(addr-7,16);
  EEPROM.write(addr-8,2);
  EEPROM.write(addr-9,0);
  addr=addr-10;
  for(byte b=0;b<10;b++) { EEPROM.write((addr-b),sn[b]); }
  delay(10);
}

//*****************************************
void nameTO_EE(word addr, char *snm) {
  byte snLEN=strlen(snm);
  addr=addr-10;
  for (byte b=0;b<10;b++) { char t=snm[b];
    if (b>=snLEN) {t=0;}
    EEPROM.write(addr-b, t); }
}

//*****************************************
char* nameFROM_EE(char *snm, char *id) { char *ret=snm;
  word addr=addr_FIND_ID(id);
  if (addr>0) {
    for (byte bp=0;bp<10;bp++) {
      snm[bp]=EEPROM.read(addr-10-bp);
      if (snm[bp]>126) {snm[bp]=0;}
    }
    snm[10]=0;
  }
  return ret;
}
  
//*****************************************
void prm_2EEPROM(char *id, byte intvl, byte hb, byte pwr, byte opt) {
  word addr=addr_FIND_ID(id);
  if (addr>0) {
    for (byte b=0;b<6;b++) { EEPROM.write(addr-b, id[b]); }
    EEPROM.write(addr-6,intvl); 
    EEPROM.write(addr-7,hb);        
    EEPROM.write(addr-8,pwr);
    EEPROM.write(addr-9,opt);
  }
}

//*****************************************
char *prm_pkt(char *pktOUT, byte pktnum, char *id, byte intvl, byte hb, byte pwr, byte opt) {
  char *ret=pktOUT;  char n2a[10];
  strcpy(pktOUT,"PRM");
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
void key_SEND(char *txid, char *txkey, char *rxkey) {
  char txBUF[48]; 
  strcpy(txBUF,txid); strcat(txBUF,":"); strcat(txBUF,rxkey);
  msg_SEND(txBUF,txkey,1);
  Serial.print(F("{\"source\":\"rx\",\"info\":\"key2tx\",\"id\":\""));
  Serial.print(txid); Serial.println(F("\"}")); Serial.flush();
}

//*****************************************
void msg_SEND(char *msgIN, char *key, int pwr) {
  char txBUF[64]; byte txLEN=strlen(msgIN);
  tx_ENCODE_0(txBUF,msgIN,txLEN,key);
  rf95.setTxPower(pwr, false);
  rf95.send(txBUF,txLEN); rf95.waitPacketSent();
}

//*****************************************
void msg_SEND_HEX(char *hexIN, byte len, char *key, int pwr) {
  char hexBUF[64];
  tx_ENCODE_0(hexBUF,hexIN,len,key);
  rf95.setTxPower(pwr, false);
  rf95.send(hexBUF,len); rf95.waitPacketSent();
}

//*****************************************
char *tx_ENCODE_0(char *txBUF, char *msgIN, byte msgLEN, char *key) { char *ret=txBUF;
  byte k=0;
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
  key[i]=0;
  return ret; 
}

//*****************************************
bool key_VALIDATE(char *key) {
  byte len=strlen(key); 
  if (len<16) { return false; }
  for (byte i=0;i<len;i++) { if ((key[i]<34) || (key[i]>126)) { return false; } }
  return true;
}

//*****************************************
bool id_VALIDATE(char *id) {
  for (byte i=0;i<6;i++) { 
    if ( ((id[i]<'2')||(id[i]>'Z')) || ((id[i]>'9')&&(id[i]<'A')) ) { return false; }
  }
  return true;
}

//*****************************************
void json_PRINTdata(char jsn[][24], byte pNum) {
  Serial.print(F("{\"source\":\"tx\","));
  for (byte pn=0;pn<pNum;pn++) {
    Serial.print( jsn[pn] ); }
  Serial.println(F("\"}")); Serial.flush();  
}

//*****************************************
void json_PRINTinfo(char info[32], byte iNum) {
  Serial.print(F("{\"source\":\"tx\",\"info\":\""));
  for (byte i=0;i<iNum;i++) {Serial.print( info[i] ); }
  Serial.println(F("\"}")); Serial.flush();  
}

//*****************************************
void jsonINFO(char *info) { 
  Serial.print(F("{\"source\":\"rx\",\"info\":\""));
  Serial.print(info);
  Serial.println(F("\"}")); Serial.flush();
}

//*****************************************
void jsonKSS() { 
  Serial.print(F("{\"source\":\"rx\",\"info\":\"kss="));
  Serial.print(keyRSS);
  Serial.println(F("\"}")); Serial.flush();
}

//*****************************************
void jsonVER() { 
  Serial.print(F("{\"source\":\"rx\",\"info\":\"ver="));
  Serial.print(VER);
  Serial.println(F("\"}")); Serial.flush();
}

//*****************************************
void id_DELETE(char *buf) {
  char id[8]; mySubStr(id,buf,4,6);
  if (buf[3]==':') { word addr=addr_FIND_ID(id);
    if (byte(EEPROM.read(addr))!=byte(0xFF)) {
      for (byte i=0;i<EE_BLKSIZE;i++) { EEPROM.write(addr-i,0xFF); }
      Serial.print("removed "); Serial.println(id); Serial.flush(); 
    }
  }
}

//*****************************************
void id_LIST() { char id[8]; char nm[12]; byte blknum=0;
  for (word addr=EE_ID;addr>EE_BLKSIZE;addr-=EE_BLKSIZE) {
    for (byte i=0;i<6;i++) { id[i]=EEPROM.read(addr-i); }
    id[6]=0;
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
void eeprom_ERASE_KEY() {
  Serial.print(F("* eeprom_ERASE_KEY..."));Serial.flush();
  for (byte i=0;i<16;i++) { EEPROM.write(EE_KEY-i,0xFF); }
  Serial.println(F(" Done"));Serial.flush();
}

//*****************************************
void eeprom_ERASE_ID() {
  Serial.print(F("* eeprom_ERASE_ID "));
  for (word i=EE_ID;i>0;i--) { EEPROM.write(i,0xFF); }
  Serial.println(F(" Done"));Serial.flush();
}

//*****************************************
void eeprom_ERASE_ALL() {
  Serial.print(F("* eeprom_ERASE_ALL "));
  for (word i=0;i<1024;i++) { EEPROM.write(i,0xFF); 
    if ((i % 64)==0){Serial.print(F(". "));} }
  Serial.println(F(" Done")); Serial.flush();
}

//*****************************************
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
  return ret;
}

//*****************************************
ISR(WDT_vect) {}
