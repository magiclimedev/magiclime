/*
    This is code for MagicLime sensors.

    MagicLime invests time and resources providing this open source code,
    please support MagicLime and open-source hardware by purchasing products
    from MagicLime!

    Written by MagicLime Inc (http://www.MagicLime.com)
    author: Marlyn Anderson, maker of things, writer of code, not a 'programmer'.
    You may read that as 'many improvements are possible'. :-) 
    MIT license, all text above must be included in any redistribution
    * 
    * 250712...
    * PPV gets 17 added to it when it comes from a repeater. '0'='A'
    * Normal range is 0-7 so, this works. 
*/

const static char VER[] = "RX250714";
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

#define EE_SYSBYTE  1023 //System byte, txPWR and RDM.
//starting at top out of habit of being considerate to YOU (fellow coder)
// who would like to use EEPROM starting at zero. 
//HOWEVER... this program uses it all - except maybe 0-4.
#define EE_KEY_RSS  EE_SYSBYTE-1  // rss level for key exchange
#define EE_KEY      EE_KEY_RSS-1  //KEY is 16 - no string null 
#define EE_ID       EE_KEY-16     // 0-5, 6 char                     
#define EE_INTERVAL EE_ID-6       // 6, 'seconds/8.8
#define EE_HRTBEAT  EE_INTERVAL-1 // 7, 'seconds/(8.8*16)' 255=544 min.
#define EE_PPVPWR   EE_HRTBEAT-1  // 8, PPV 1-7, POWER 2-20.
#define EE_HOLDOPT  EE_PPVPWR-1   // 9, 7-4 holdoff( x8 sec)
#define EE_NAME     EE_HOLDOPT-1  // 10-19, 10 char max.? why not say, 16?
#define EE_BLKSIZE  20            // 20 total BLocK SIZE
//that OPTBYTE... bits 7-4 are HoldOff, bits 3,0 unused

//here's the help menu table...
const char H00[] PROGMEM = "---- commands ----";
const char H01[] PROGMEM = "PaRaMeters ('ididid' is sensor's 6-char. ID.)";
const char H02[] PROGMEM = "prm:ididid:0:seconds > 8-1800 Interval-Periodic";
const char H03[] PROGMEM = "prm:ididid:1:minutes > 2-540  Interval-Heartbeat";
const char H04[] PROGMEM = "prm:ididid:2:TX power > 5-23.";
const char H05[] PROGMEM = "prm:ididid:3:Packet Protocol Version, PPV > 0-7";
const char H06[] PROGMEM = "prm:ididid:4:Hold-Off re-trigger > 0-15 (*8 sec.)";
const char H07[] PROGMEM = "prm:ididid:5:Sensor Board Options > 0-15 ";
const char H08[] PROGMEM = "kss:xxx       -Key exchange Signal Strength level";  
const char H09[] PROGMEM = "slr:ididid:nnnnn -Sensor Label Replace.(10 char.max.)";
const char H10[] PROGMEM = "ida:ididid    -ID Add to list"; 
const char H11[] PROGMEM = "idd:ididid    -ID Delete from list";
const char H12[] PROGMEM = "idl           -ID List";
const char H13[] PROGMEM = "ide           -ID's Erase !ALL!";
const char H14[] PROGMEM = "key           -KEY show ";
const char H15[] PROGMEM = "kye           -KeY Erase ";
const char H16[] PROGMEM = "kys:(16char.) -KeY Set ";
const char H17[] PROGMEM = "eee           -Erase Entire EEprom";
const char H18[] PROGMEM = "rdm:0/1/2     -Raw Data Mode: off/only/both";
const char H19[] PROGMEM = "txp:xx        -TX Power (5-23)";
const char H20[] PROGMEM = "ver           -show this program Version";
const char H21[] PROGMEM = "ppv           -tell me more about that 'ppv'.";
const char H22[] PROGMEM = "";
const char H23[] PROGMEM = " -- examples --";
const char H24[] PROGMEM = "prm:MYSNSR:0:120, prm:MYSNSR:1:60, prm:MYSNSR:2:5";
const char H25[] PROGMEM = "kss:90"; 
const char H26[] PROGMEM = "kys:?6Rh0@'](MUTtN*R";
const char H27[] PROGMEM = "slr:MYSNSR:GarageDoor"; 
const char H28[] PROGMEM = "idd:MYSNSR";
const char H29[] PROGMEM = "";

PGM_P const table_HLP[] PROGMEM ={
  H00,H01,H02,H03,H04,H05,H06,H07,H08,H09,
  H10,H11,H12,H13,H14,H15,H16,H17,H18,H19,
  H20,H21,H22,H23,H24,H25,H26,H27,H28,H29};
const byte HLPnum=30;

//here's the help menu table for 'ppv'...
const char P00[] PROGMEM = "---- ppv explained ----";
const char P01[] PROGMEM = "Packet Protocol Version";
const char P02[] PROGMEM = " The packet of data from the sensor transmitter is";
const char P03[] PROGMEM = "configurable via this 'ppv' parameter.";
const char P04[] PROGMEM = " A 'ppv' value of '0' will have the sensor send";
const char P05[] PROGMEM = "just the ID and Data fields. The minimum # of bytes.";
const char P06[] PROGMEM = "A '1' adds battery voltage.";
const char P07[] PROGMEM = "A '2' adds the 'SBN', Sensor Board Number.";  
const char P08[] PROGMEM = "A '3' has the transmitter impliment a 'TX Counter', and";
const char P09[] PROGMEM = " adds that 'TX Count #' to the packet. Good for sensors";
const char P10[] PROGMEM = " like Motion, Knock and Button. I.e., event type sensors.";
const char P11[] PROGMEM = "A '4' backs off the 'data' part. Data is redundant as ID,";
const char P12[] PROGMEM = "  SBN and TX# are still in there for event-type sensors.";
const char P13[] PROGMEM = "BTW... The 'ppv #' in the packet is 3 bits, so can only";
const char P14[] PROGMEM = "  be 0-7. #'s 5-7 are free for your use.";
const char P15[] PROGMEM = "";
PGM_P const table_PPV[] PROGMEM ={
  P00,P01,P02,P03,P04,P05,P06,P07,P08,P09,
  P10,P11,P12,P13,P14,P15};
const byte PPVnum=16;
//Key names for json tx data pair composing

const char KsrcRX[] PROGMEM = "{\"source\":\"rx\",";
const char KsrcRP[] PROGMEM = "{\"source\":\"rp\",";
const char KsrcTX[] PROGMEM = "{\"source\":\"tx\",";
const char Kpdi[] PROGMEM = ",\"pdi\":";    //is number  no pre-quote
const char Khbi[] PROGMEM = ",\"hbi\":";    // is number  no pre-quote
const char Kpwr[] PROGMEM = ",\"power\":";    // is number  no pre-quote
const char Khld[] PROGMEM = ",\"holdoff\":";    //is number  no pre-quote
const char Kopt[] PROGMEM = ",\"option\":";    //??is number  no pre-quote??
//There is a first pair - source:tx - that does not end with ','
const char Krss[] PROGMEM = "\"rss\":";      //0data is number  no pre-quote
const char Kppv[] PROGMEM = ",\"ppv\":\"";    //1data is string
const char Kuid[] PROGMEM = ",\"uid\":\"";    //2data is string 
const char Ksbn[] PROGMEM = ",\"sbn\":";      //3data is number  no pre-quote
const char Klabel[] PROGMEM = ",\"label\":\""; //4data is string,10-char.
const char Kbat[] PROGMEM = ",\"bat\":";      //5data is number  no pre-quote
const char Kdata1[] PROGMEM = ",\"data1\":\"";  //6data is string,
const char Kdata2[] PROGMEM = ",\"data2\":\"";  //7data is string,
const char Kdata3[] PROGMEM = ",\"data3\":\"";  //8data is string,
const char Kdata4[] PROGMEM = ",\"data4\":\"";  //9data is string,
const char Ktxnum[] PROGMEM = ",\"tx#\":";      //10data is number  no pre-quote 
//misc parameter echo Key names
const char Kidadd[] PROGMEM = "\"ID_ADDED\":\"";
const char Kidrem[] PROGMEM = "\"ID_REMOVED\":\"";
const char Kkeyset[] PROGMEM = "\"KEY_SET\":\"";
const char Krxkey[] PROGMEM = "\"KEY_RX\":\"";
const char Kkss[] PROGMEM = "\"KEY_RSS\":"; // data is number
const char Kkey2tx[] PROGMEM = "\"KEY2TX\":\"";
const char Knamerep[] PROGMEM = "\"LABEL_REPLACE\":\"";
const char Krdm[] PROGMEM = "\"RDM\":\"";
const char Ktxpwr[] PROGMEM = "\"PWR\":\"";
const char Kprmout[] PROGMEM = "\"PRM_OUT\":\"";
const char Kprmack[] PROGMEM = "\"PRM_ACK\":\"";
const char Kprmpkt[] PROGMEM = "\"PRM_QUEUE\":\""; //PRM:ididid:x:yyy
const char Kprmval[] PROGMEM = "\"PRM_VALUE\":\""; 
const char Kprmprm[] PROGMEM = "\"PRM\":\"";
const char Kpurpur[] PROGMEM = "\"PUR\":\"";
const char Kprm[] PROGMEM = "PRM:"; 
const char Kpak[] PROGMEM = "PAK:"; 
const char Kpur[] PROGMEM = "PUR:";
const char Kver[] PROGMEM ="VER:";
const char Ktxver[] PROGMEM = "\"TX_VER\":\"";
const char Krxver[] PROGMEM = "\"RX_VER\":\"";
const char Krf95fail[] PROGMEM = "\"RFM95\":\"!FAIL!\"";

const char chrCOLON[] PROGMEM = ":";
const char chrCOMMA[] PROGMEM = ",";
const char chrQUOTE[] PROGMEM = "\"";
const char KEY_HARD[] PROGMEM = "thisisamagiclime";

const char Pkss[] PROGMEM = "kss";
const char Pslr[] PROGMEM = "slr";
const char Pida[] PROGMEM = "ida";
const char Pidd[] PROGMEM = "idd";
const char Pidl[] PROGMEM = "idl"; 
const char Pkey[] PROGMEM = "key";
const char Pkye[] PROGMEM = "kye";
const char Pide[] PROGMEM = "ide";
const char Peee[] PROGMEM = "eee";
const char Pkys[] PROGMEM = "kys";
const char Pprm[] PROGMEM = "prm";
const char Pppv[] PROGMEM = "ppv";
const char Prdm[] PROGMEM = "rdm";
const char Ptxp[] PROGMEM = "txp";
const char Pver[] PROGMEM = "ver";

static char rxBUF[80]={0}; //for the rx buf
static char rxKEY[18]={0}; //16 char + null

byte keyRSS=90;
byte rxRSS;
byte rxLEN;
byte txPWR=13;
byte modeRDM; //'Raw Data Mode' what to spit out, raw data or decoded
byte sysByte; //stores modeRDM in eeprom bits 1,0

const byte rssOFFSET=140; //I saw -134 get good data once
const int defaultINTERVAL = 35; //*8.6 sec = 5 min
const int defaultHEARTBEAT = 157;//*8.6 sec *16 = 360 min (6 hrs)
const byte wdmPDI=1; //multiplier to get Periodic Data Interval into byte range
const byte wdmHBI=16; //multiplier to get Heart Beat Interval into byte range

unsigned long rxTimeThen;
unsigned long rxTimeNow;
unsigned long deltaT;
//**********************************************************************
void setup() { 
  while (!Serial);
  Serial.begin(57600);
  json_VER(VER);
  if (EEPROM.read(EE_KEY_RSS)<200) {keyRSS=EEPROM.read(EE_KEY_RSS);} 
  json_KSS(keyRSS);
//--------------------------------------------------
  key_EE_GET(rxKEY); //Serial.print(F("* key="));Serial.println(rxKEY);
  if (key_VALIDATE(rxKEY)==false) { //Serial.println(F("* key_VALIDATE(rxKEY)==false"));Serial.flush(); 
    key_EE_MAKE(); key_EE_GET(rxKEY); //Serial.print(F("* key new="));Serial.println(rxKEY);
  }
  json_KEY(rxKEY);

//--------------------------------------------------
  sysByte=EEPROM.read(EE_SYSBYTE);
  modeRDM=sysByte & 0x03; //0 is normal, 1 is RDM on, 2 is both, 3 adds decoded msg
  //if (modeRDM==3) {modeRDM=0; EEPROM.write(EE_SYSBYTE,(sysByte & 0xFC));}
  byte pwr=sysByte >> 3; //top 5 bits for power = 5-23
  if ((pwr<5) || (pwr>23)) { pwr=txPWR; //not good? set to init default
    byte txp=txPWR<<3; //back to top 5
    sysByte=((sysByte & 0x03)| txp); //merge with rdm
    EEPROM.write(EE_SYSBYTE,sysByte);
  } 
  txPWR=pwr; 

//-------------------------------------------------- 
  if (rf95.init()) {
     rf95.setFrequency(RF95_FREQ); delay(5);
     rf95.setTxPower(txPWR, false);  delay(5); }
  else { char jsnSET[20]; strlcpy_P(jsnSET,Krf95fail,20); //"\"RFM95\":\"!FAIL!\"";
    json_RX(jsnSET);
    //no RX - no need for program to run any more
    while (1) {delay(1000);}
  }


} // End Of SETUP ******************
//***************************************************************

//***************************************************************
void loop(){ rxRSS=rxBUF_CHECK();
  if (rxRSS>0) { 
    if (modeRDM>0) {Serial.println(""); Serial.print(F("rxRSS: "));Serial.print(rxRSS);
                      Serial.print(F(", "));Serial.print(F("rxLEN: "));Serial.print(rxLEN);
                      Serial.print(F(", "));Serial.println(rxBUF); }
    if ((modeRDM==0)||(modeRDM>1)){rxBUF_PROCESS(rxRSS);}  
  }
  look4_PCbuf();
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
      rxBUF[len]=0;
      rxLEN=len;
      rss=rf95.lastRssi()+rssOFFSET;  
    }
  }
  return rss;
}

// *********************************************************
//looking for these things, in this order...
//  1.  a pairing key - if rss is big enough.
//  2.  data
//  3.  a request for stored parameter settings. ('PUR') that has the sensor label
//but... don't overwrite label if rss < rxKEY
//  4.  an ack of those parameters ('PAK:ididid:0:sec:min:ppvpwr:holdoff')
//  5.  a tx version info packet ('VER:ididid:version#')

void rxBUF_PROCESS(byte rss) { 
  char msg[64]={0}; //for incoming packets
  char ret[24]={0}; //for returning processed packet data 
  //Serial.println(rss);Serial.flush();
  if (rss>=keyRSS) { //first requirement for pairing
    char pairKEY[18]; strlcpy_P(pairKEY,KEY_HARD,18);
    pair_PROCESS(msg,rxBUF,rxLEN,pairKEY); //!ididid!txkey.. --> ididid:keykeykey
    if ((msg[0]!=0) && (msg[6]==':')) {
      char txid[8]={0}; char txkey[18]={0};
      mySubStr(txid,msg,0,6);//
      mySubStr(txkey,msg,7,strlen(msg)-7);
      key_SEND(txid,txkey,rxKEY);
      word addr=addr_ID_FIND(txid);
      if (addr==0) {addr=addr_ID_NEXT();}
      if ((byte(EEPROM.read(addr))>=byte(0x5A)) || (byte(EEPROM.read(addr))<=byte(0x31))) { //new addition?
        char lbl[]="????";
        prm_EE_SET_DFLT(txid,addr,lbl);  } 
      return;
    }
  } 
  
//************ not key stuff ... *************
//Serial.print(F("* key: ")); Serial.println(rxKEY);Serial.flush();
  rx_DECODE(msg,rxBUF,rxLEN, rxKEY); //decode using rx KEY
  if (msg[0]!=0) {
    //rxTimeNow=millis(); deltaT=(rxTimeNow-rxTimeThen); rxTimeThen=rxTimeNow;
    //if (deltaT<70) {return;} //no repeat of repeater which is usually 65
    //is it 'PUR'? Or DATA? or INFO?
    byte msgLEN=strlen(msg);
    if (modeRDM==3) {Serial.print(rss);Serial.print(F(", "));Serial.print(msgLEN);Serial.print(F(", "));Serial.println(msg);}
    byte pNum=0; //json Number of Pairs for json_DATA()
    for (byte ix=0;ix<msgLEN;ix++) { if ( msg[ix] == '|') {pNum=pNum+1;} }
    pNum=pNum+2; //+2 for rss and name not part of msg
    //Serial.print(F("pNum:"));Serial.println(pNum);
    bool good2go=false;
    char jPs[20]={0};  //Json Pair String
    if (msg[1] == '|') { //short quick first eval
      if ((msg[0]>='0') && (msg[0]<='7')) {
        strlcpy_P(jPs,KsrcTX,16); Serial.print(jPs); //{"source":"tx",
        good2go=true; }
      else if ((msg[0]>='A') && (msg[0]<='H')) {
        msg[0]=msg[0]-17; //'A'-17='0'
		    strlcpy_P(jPs,KsrcRP,16); Serial.print(jPs); //{"source":"rp",}  
        good2go=true; }
    }   
    if (good2go==true) {  
      char ppv[2]; ppv[0]=msg[0]; ppv[1]=0; //c-string it
      strlcpy_P(jPs,Krss,12); //rss - first data out
      char rs[4];itoa(rss,rs,10); strcat(jPs,rs); //rss as number (no " at end)
      Serial.print(jPs);

      strlcpy_P(jPs,Kppv,12);  //ppv - 1ST MSG DATA IN, 2nd data out
      strcat(jPs,ppv); strcat_P(jPs,chrQUOTE); Serial.print(jPs); //jPx=0

      strlcpy_P(jPs,Kuid,12);  //uid - 2ND MSG DATA IN, 3rd data out
      char uid[10];  //for getting sensor label from eeprom ?
      mySubStr(uid,msg,2,6);  //uid at 2-7. 8 is next dlm   
      strcat(jPs,uid); strcat_P(jPs,chrQUOTE); Serial.print(jPs); //jPx=1

      strlcpy_P(jPs, Klabel,12);  //4th data out, from eeprom
      char sNM[15]; labelFROM_EE(sNM,uid);
      strcat(jPs,sNM); strcat_P(jPs,chrQUOTE); Serial.print(jPs);

      byte Bx=9; //Buf indeX after 'ppv|ididid|' (0-8)
      byte Dx=0; //Data string builder  indeX pointer
      byte jPx=0; //Json Pair # indeX pointer starting after UID
      char jDs[10]={0};  //Json Data String from msg[]
      
      switch (msg[0]) { //if (ppv=='0') {ppv='1';} 
        case '0': { //starting with Bx=9 after ID,dlm - just data
          while (Bx<=msgLEN) {
            if ( (msg[Bx]=='|') || (Bx==msgLEN) )  { jDs[Dx]=0; 
              switch (jPx) {  //0 and 1 done already
                case 0: { strlcpy_P(jPs, Kdata1,20); //data1
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);
                    } break; 
                case 1: { strlcpy_P(jPs,Kdata2,20); //data2
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);
                    } break; //bat, number - no closing quotea
                case 2: { strlcpy_P(jPs,Kdata3,20); //data3
                    strcat(jPs,jDs);strcat_P(jPs,chrQUOTE);  Serial.print(jPs);
                    } break; //data1
                case 3: { strlcpy_P(jPs,Kdata4,20); //data4
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);
                    } break;//data2
              } //put in the keynames for that data field 
              Dx=0;  jPx++; //done with this 'jPx', prep for next pair
            } //got a data string and assigned it
            else { jDs[Dx]=msg[Bx]; Dx++; } //build up jDs data
            Bx++; //next char in msg
          } //next char in msg
        } break;//End of case:'ppv=0'

        case '1': {  //plus ID, BV and data
          while (Bx<=msgLEN) {
            if ( (msg[Bx]=='|') || (Bx==msgLEN) )  { // hit next delim? - got a pair
              jDs[Dx]=0; 
              switch (jPx) { 
                case 0: { strlcpy_P(jPs, Kbat,20); //bat
                    strcat(jPs,jDs);  Serial.print(jPs);
                    }  break; 
                case 1: { strlcpy_P(jPs, Kdata1,20); //data1
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);
                    } break; 
                case 2: { strlcpy_P(jPs,Kdata2,20); //data2
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);
                    } break; 
                case 3: { strlcpy_P(jPs,Kdata3,20); //data3
                    strcat(jPs,jDs);strcat_P(jPs,chrQUOTE);  Serial.print(jPs);
                    } break; 
                case 4: { strlcpy_P(jPs,Kdata4,20); //data4
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);
                    } break;
              } 
              Dx=0;  jPx++; //done with this 'jPx', prep for next pair
            } //got a data string and assigned it
            else { jDs[Dx]=msg[Bx]; Dx++; } //build up pair # jpx's data
            Bx++; //next char in msg
          } //next char in msg
        } break;//End of case:'ppv=1'

        case '2': {  //plus ID, BV, SBN and Data
          while (Bx<=msgLEN) {
            if ( (msg[Bx]=='|') || (Bx==msgLEN) )  { jDs[Dx]=0; 
              switch (jPx) { 
                case 0: { strlcpy_P(jPs, Kbat,12); //bat
                    strcat(jPs,jDs); Serial.print(jPs);  }  break;
                case 1: { strlcpy_P(jPs, Ksbn,12); //sbn
                    strcat(jPs,jDs); Serial.print(jPs);  } break; 
                case 2: { strlcpy_P(jPs, Kdata1,12); //data1
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);  } break; 
                case 3: { strlcpy_P(jPs,Kdata2,12); //data2
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);  } break; 
                case 4: { strlcpy_P(jPs,Kdata3,12); //data3
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE);  Serial.print(jPs);  } break; 
                case 5: { strlcpy_P(jPs,Kdata4,12); //data4
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs); } break;
              } 
              Dx=0;  jPx++; //done with this 'jPx', prep for next pair
            } //got a data string and assigned it
            else { jDs[Dx]=msg[Bx]; Dx++; } //build up pair # jpx's data
            Bx++; //next char in msg
          } //next char in msg
        } break;//End of case:'ppv=2'

        case '3': {  //plus ID, BV, TX Count and Data 
          while (Bx<=msgLEN) {
            if ( (msg[Bx]=='|') || (Bx==msgLEN) )  { jDs[Dx]=0; 
              switch (jPx) { 
                case 0: { strlcpy_P(jPs, Kbat,12); //bat
                    strcat(jPs,jDs); Serial.print(jPs);  }  break;
                case 1: { strlcpy_P(jPs, Ktxnum,12);  char txn[7]; B962L(txn,jDs); //tx#
                    strcat(jPs,txn); Serial.print(jPs);  } break;    
                case 2: { strlcpy_P(jPs, Kdata1,12); //data1
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);  } break; 
                case 3: { strlcpy_P(jPs,Kdata2,12); //data2
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);  } break; 
                case 4: { strlcpy_P(jPs,Kdata3,12); //data3
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE);  Serial.print(jPs);  } break; 
                case 5: { strlcpy_P(jPs,Kdata4,12); //data4
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs); } break;
              } 
              Dx=0;  jPx++; //done with this 'jPx', prep for next pair
            } //got a data string and assigned it
            else { jDs[Dx]=msg[Bx]; Dx++; } //build up pair # jpx's data
            Bx++; //next char in msg
          } //next char in msg
        } break;//End of case:'ppv=3'

        case '4': {  //plus ID, BV, SBN, TXcount 
          while (Bx<=msgLEN) {
            if ( (msg[Bx]=='|') || (Bx==msgLEN) )  { jDs[Dx]=0; 
              switch (jPx) { 
                case 0: { strlcpy_P(jPs, Kbat,12); //bat
                    strcat(jPs,jDs); Serial.print(jPs);  }  break;
                case 1: { strlcpy_P(jPs, Ksbn,12); //sbn
                    strcat(jPs,jDs); Serial.print(jPs);  } break; 
                case 2: { strlcpy_P(jPs, Ktxnum,12);  char txn[7]; B962L(txn,jDs); //tx#
                    strcat(jPs,txn); Serial.print(jPs);  } break;    
                case 3: { strlcpy_P(jPs, Kdata1,12); //data1
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);  } break; 
                case 4: { strlcpy_P(jPs,Kdata2,12); //data2
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs);  } break; 
                case 5: { strlcpy_P(jPs,Kdata3,12); //data3
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE);  Serial.print(jPs);  } break; 
                case 6: { strlcpy_P(jPs,Kdata4,12); //data4
                    strcat(jPs,jDs); strcat_P(jPs,chrQUOTE); Serial.print(jPs); } break;
              } //put in the keynames for that data field 
              Dx=0;  jPx++; //done with this 'jPx', prep for next pair
            } //got a data string and assigned it
            else { jDs[Dx]=msg[Bx]; Dx++; } //build up pair # jpx's data
            Bx++; //next char in msg
          } //next char in msg
        } break;//End of case:'ppv=4'                
      } //EndOfSwtchProtocol
      Serial.println(F("}")); Serial.flush();
      return;
    } //End of PPV check good2go==true validate protocol format

//****** so... not
    look4_PUR(ret,msg); //PUR:IDxxxx:Label67890
    if (ret[0]!=0) { char jsnPUR[30]; strlcpy_P(jsnPUR,Kpurpur,10);
      strcat(jsnPUR,ret); strcat_P(jsnPUR,chrQUOTE);
      json_TX(jsnPUR);
      pur_PROCESS(ret,rxRSS,keyRSS); //sends paramters if OK
      //clear RX buffer and wait a bit to ignore possible repeater msg?
      return;
    }
//******    
    look4_PAK(ret,msg); //PAK:IDxxxx:10:30:2,7 -ish , 
    if (ret[0]!=0) {  char jsnPAK[30]; strlcpy_P(jsnPAK,Kprmack,12);
      strcat(jsnPAK,ret); strcat_P(jsnPAK,chrQUOTE);
      json_TX(jsnPAK);
      //clear RX buffer and wait a bit to ignore possible repeater msg?
      return;
    }
//******    
    look4_VER(ret,msg); //VER:ididid:version# , 
    if (ret[0]!=0) { char jsnVER[24]; strlcpy_P(jsnVER,Ktxver,12); 
      strcat(jsnVER,ret); strcat_P(jsnVER,chrQUOTE);
      json_TX(jsnVER);
      //clear RX buffer and wait a bit to ignore possible repeater msg?
      return;
    }
//******
  } //end of rxDECODED OK msg[0]!=0
} //end of rxBUF_PROCESS() 

//**********************************************************************
void look4_PCbuf() { // Look for Commands from the Host PC
  if (Serial.available())  {
    char cbyte;   byte c_pos = 0;  char PCbuf[50]={0};
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
     
    if (PCbuf[0]=='?') { showHELP(HLPnum); return; } 

    char pfx[6]={0}; mySubStr(pfx,PCbuf,0,3); //6 bytes max expected
    if (strcmp_P(pfx,Pkss)==0) {key_SETREF(PCbuf);} //Key Signal Strength
    if (strcmp_P(pfx,Pslr)==0) {label_REPLACE(PCbuf);}
    if (strcmp_P(pfx,Pida)==0) {id_ADD(PCbuf);}//set/clear flgRDM
    if (strcmp_P(pfx,Pidd)==0) {id_DELETE(PCbuf);}
    if (strcmp_P(pfx,Pidl)==0) {id_LIST();}  
    if (strcmp_P(pfx,Pkey)==0) {json_KEY(rxKEY);}
    if (strcmp_P(pfx,Pkye)==0) {eeprom_ERASE_KEY();}
    if (strcmp_P(pfx,Pide)==0) {eeprom_ERASE_ID();}
    if (strcmp_P(pfx,Peee)==0) {eeprom_ERASE_ALL();}
    if (strcmp_P(pfx,Pkys)==0) {eeprom_set_KEY(PCbuf);}
    if (strcmp_P(pfx,Pprm)==0) {prm_PROCESS(PCbuf);}
    if (strcmp_P(pfx,Pppv)==0) {showPPV(PPVnum);}    
    if (strcmp_P(pfx,Prdm)==0) {rdm_mode(PCbuf);}//set/clear flgRDM
    if (strcmp_P(pfx,Ptxp)==0) {setTXPWR(PCbuf);}
    if (strcmp_P(pfx,Pver)==0) {json_VER(VER);}
  } //if Serial.aval
} //End Of CheckPCbuf

//**********************************************************************
void rdm_mode(char *buf) {
  if ((buf[3]==':') and (buf[4]!=0)) { 
    modeRDM=buf[4]-48;
    sysByte=(sysByte & 0xFC) | modeRDM;
    EEPROM.write(EE_SYSBYTE,sysByte); 
  }
  sysByte=EEPROM.read(EE_SYSBYTE);
  modeRDM=sysByte & 0x03; //0 is normal, 1 is RDM on, 2 is both, 3 is decoded msg
  char N2A[4]; itoa(modeRDM,N2A,10);
  char jsn[14]; strlcpy_P(jsn,Krdm,10); 
  strcat(jsn,N2A); strcat_P(jsn,chrQUOTE);
  json_RX(jsn);
}

//**********************************************************************
void setTXPWR(char *buf) { byte txp;
  if ((buf[3]==':') and (buf[4]!=0)) { //set it
    txp=buf[4]-48; //single digit or tens value?
    if (buf[5]!=0) {txp=(txp*10) + (buf[5]-48);}
    if ((txp >=5) && (txp<=23)) { txPWR=txp; //assign it
      txp=txp<<3; //move to top 5 bits
      sysByte=EEPROM.read(EE_SYSBYTE);
      sysByte=(sysByte & 0x03) | txp; //put in sysByte
      EEPROM.write(EE_SYSBYTE,sysByte);
    }
  }  //otherwise - show it
  sysByte=EEPROM.read(EE_SYSBYTE);
  txp=sysByte >> 3; //top 5 to bottom 5
  char N2A[4]; itoa(txp,N2A,10);
  char jsn[14]; strlcpy_P(jsn,Ktxpwr,10); 
  strcat(jsn,N2A); strcat_P(jsn,chrQUOTE);
  json_RX(jsn);
}



//**********************************************************************
void eeprom_set_KEY(char *buf) {
  if (buf[3]==':') { //one more validate 
    byte bufLEN=strlen(buf);
    char key[18]={0}; mySubStr(key,buf,4,bufLEN-4);
    if (strlen(key)==16) { 
      for (byte i=0;i<16;i++) { EEPROM.write(EE_KEY-i,buf[i+4]); }  
    }
  }
  key_EE_GET(rxKEY);
  char jsn[32]; strlcpy_P(jsn,Kkeyset,14);
  strcat(jsn,rxKEY); strcat_P(jsn,chrQUOTE);
  json_RX(jsn); 
}

//**********************************************************************
void key_SETREF(char *buf) {
  if (buf[3]==':') { //one more validate
    byte bufLEN=strlen(buf);
    char kss[4];  mySubStr(kss,buf,4,bufLEN-4);  //get kss value at PCbuf[3,4 and maybe 5]
    keyRSS=byte(atoi(kss));
    if ((keyRSS<80) and (keyRSS>120)) {keyRSS=90;}
    EEPROM.write(EE_KEY_RSS,keyRSS);
    json_KSS(keyRSS);
  }
  else {json_KSS(keyRSS);}
}

//*************************************************************
void prm_PROCESS(char *buf){  //prm:ididid:x:yyy..
//if no prm# submitted  ( no ':' after id) then return all
//if no value submitted  ( no ':' after prm#) then just return value stored
  //Serial.println(buf);
  if (buf[3]==':')  { // validate
    byte bufLEN=strlen(buf);
    char uid[8]; mySubStr(uid,buf,4,6);
    if (buf[10]==0) { prm_send_prm(uid); } //prm_jsn_prm(jsn,uid); } //just id - get all as individual values
    else if (buf[10]==':') {
      int pnum=buf[11]-48; //from char to value
      if ((pnum>=0) && (pnum<=7)) {  //another validate
        if (buf[12]==0) { prm_send_val(uid,pnum); } //prm_jsn_val(jsn,uid,pnum); } //return one value
        else if (buf[12]==':') { // must be setting a value.
          byte val; char asc[8]; 
          mySubStr(asc,buf,13,(bufLEN-13));
          switch (pnum) { 
            case 0: { 
              val=byte( (atoi(asc)*10) / (86*wdmPDI) ); //sec., 
              if (val==0) {val=1;} else if (val>255) {val=255;}
              prm_EEPROM_SET(uid,0,val);
              } break;

            case 1: { 
              val=byte( long(atol(asc)*600) / long(86*wdmHBI) ); //min., 
              if (val==0) {val=1;} else if (val>255) {val=255;}
              prm_EEPROM_SET(uid,1,val);
              } break;

            case 2: {
              val=byte(atoi(asc));                    //Power 5-23
              if ((val<5) || (val>23)) {val=5;}   
              byte cur=prm_EEPROM_GET(uid,2); //2=PpvPower
              byte rplc=(cur & 0xE0) | val;                     //xxxppppp
              prm_EEPROM_SET(uid,2,rplc);
              } break;

            case 3: { 
              val=byte(atoi(asc));                    //PPV 3 bits 
              if (val>7) { val=1; } //default
              byte cur=prm_EEPROM_GET(uid,2); //2=PpvPower
              byte rplc=(cur & 0x1F) | (val<<5);   //bits 7-5, 4-0 stay same
              //Serial.print(F("3rplc+:"));Serial.println(rplc);Serial.flush();
              prm_EEPROM_SET(uid,2,rplc);
              } break;

            case 4: { 
              val=byte(atoi(asc));                    //hold-off 4 bits .
              if (val>15) {val=0;}
              byte cur=prm_EEPROM_GET(uid,3); //3=HoldOpt
              byte rplc=(cur & 0x0F) | (val<<4) ;     //bits 7-4 , 3-0 stay same
              prm_EEPROM_SET(uid,3,rplc);
              } break;

            case 5: { 
              val=byte(atoi(asc));                    //sensor options 4 bits .
              if (val>15) {val=0;}
              byte cur=prm_EEPROM_GET(uid,3); //3=HoldOpt
              byte rplc=((cur & 0xF0) | val);     //bits 7-4 stay same
              prm_EEPROM_SET(uid,3,rplc);
              } break;
          }
          prm_send_pkt(uid);
        }
      }
    }
  }  //Serial.println("EO_prm_PROCESS");
}

//*****************************************
//this one is for sending to the sensor
void prm_send_pkt(char *id) {  char jsnPKT[20];
  word addr=addr_ID_FIND(id);
  if (addr!=0) {
    int intvl = EEPROM.read(addr-6); //0
    int hb = EEPROM.read(addr-7); //1
    int ppvpwr = EEPROM.read(addr-8); //3,2
    int holdopt = EEPROM.read(addr-9); //4,5
    char n2a[5]; // for Number TO Ascii things
    strlcpy_P(jsnPKT,KsrcRX,18); Serial.print(jsnPKT); 
    strlcpy_P(jsnPKT,Kprmpkt,16); strcat(jsnPKT,id); Serial.print(jsnPKT); 
    strlcpy_P(jsnPKT,chrCOLON,10); itoa(intvl,n2a,10); strcat(jsnPKT,n2a); 
    strcat_P(jsnPKT,chrCOLON); itoa(hb,n2a,10);  strcat(jsnPKT,n2a); 
    strcat_P(jsnPKT,chrCOLON); itoa(ppvpwr,n2a,10); strcat(jsnPKT,n2a);
    strcat_P(jsnPKT,chrCOLON); itoa(holdopt,n2a,10); strcat(jsnPKT,n2a); 
    strcat(jsnPKT,"\"}");  Serial.println(jsnPKT);
    //Serial.print(F("prm_jsn_pkt"));Serial.println(jsnPKT);
  }
}

//*****************************************
//this one is for the human asking the receiver for current parameter value 
void prm_send_val(char *id, byte prmNUM) {  char jsnVAL[20];
  word addr=addr_ID_FIND(id); byte eeVal; byte prmVal; 
  if (addr!=0) {
    if ((prmNUM==2) | (prmNUM==3)) { //pwr or ppv
      eeVal = EEPROM.read(addr-8);
      if (prmNUM==2) { prmVal= (eeVal & 0x1F); } //2=pwr low 5
      else {prmVal=eeVal >> 5; } //3=ppv top 3
    }
    else if ((prmNUM==4) | (prmNUM==5)) { //hold count or options
        eeVal = EEPROM.read(addr-9);
        if (prmNUM==4) { prmVal= (eeVal>>4);} //4=hold count 
        else {prmVal=eeVal & 0x0F; } //5=options
      }
    else { prmVal = EEPROM.read(addr-6-prmNUM); } //0 or 1

    char n2a[5];
    strlcpy_P(jsnVAL,KsrcRX,20); Serial.print(jsnVAL); //Serial.print(F("{\"source\":\"rx\","));
    strlcpy_P(jsnVAL,Kprmval,20); Serial.print(jsnVAL); //{"PRM_VALUE":"
    itoa(int(prmNUM),n2a,10); strcpy(jsnVAL,n2a);
    strcat_P(jsnVAL,chrCOLON);
    itoa(int(prmVal),n2a,10); strcat(jsnVAL,n2a);
    strcat(jsnVAL,"\"}"); Serial.println(jsnVAL); 
    //"PARAMETER VALUE":"p:v"
  }
}

//*****************************************
//const char Kpdi[] PROGMEM = ",\"pdi\":\"";    //is number  no pre-quote
//const char Khbi[] PROGMEM = ",\"hbi\":\"";    // is number  no pre-quote
//const char Kpwr[] PROGMEM = ",\"power\":\"";    // is number  no pre-quote
//const char Khld[] PROGMEM = ",\"holdoff\":\"";    //is number  no pre-quote
//const char Kopt[] PROGMEM = ",\"option\":\"";    //is number  no pre-quote
//json packet of all parameters for PYSIMPLEGUI program to fill its blanks
void prm_send_prm(char *id) {  char jsnPRM[20];
  word addr=addr_ID_FIND(id);
  if (addr!=0) {
    char label[12]={0}; labelFROM_EE(label,id); 
    int pdi = EEPROM.read(addr-6); //0
    int hbi = EEPROM.read(addr-7); //1
    byte ppvpwr = EEPROM.read(addr-8);//2,3
    int ppv=(ppvpwr >> 5);
    int pwr=(ppvpwr & 0x1F);
    byte holdopt = EEPROM.read(addr-9); //4,5
    int hold=holdopt>>4;
    int opt=holdopt & 0x0F;
    char n2a[5];
    strlcpy_P(jsnPRM,KsrcRX,20); Serial.print(jsnPRM); //Serial.print(F("{\"source\":\"rx\","));
    strlcpy_P(jsnPRM,Kprmprm,20); strcat(jsnPRM,id); strcat_P(jsnPRM,chrQUOTE); Serial.print(jsnPRM);
    strlcpy_P(jsnPRM,Klabel,20);  strcat(jsnPRM,label); strcat_P(jsnPRM,chrQUOTE); Serial.print(jsnPRM);
    strlcpy_P(jsnPRM,Kpdi,10); itoa(pdi,n2a,10); strcat(jsnPRM,n2a); Serial.print(jsnPRM);
    strlcpy_P(jsnPRM,Khbi,10); itoa(hbi,n2a,10);  strcat(jsnPRM,n2a); Serial.print(jsnPRM);
    strlcpy_P(jsnPRM,Kpwr,10); itoa(pwr,n2a,10); strcat(jsnPRM,n2a); Serial.print(jsnPRM);
    strlcpy_P(jsnPRM,Kppv,10); itoa(ppv,n2a,10); strcat(jsnPRM,n2a); strcat_P(jsnPRM,chrQUOTE); Serial.print(jsnPRM);
    strlcpy_P(jsnPRM,Khld,12); itoa(hold,n2a,10); strcat(jsnPRM,n2a); Serial.print(jsnPRM);
    strlcpy_P(jsnPRM,Kopt,12); itoa(opt,n2a,10); strcat(jsnPRM,n2a); 
    strcat(jsnPRM,"}"); Serial.println(jsnPRM);
  }
}

//*****************************************
char *pair_PROCESS(char *idkey, char *rxbuf, byte bufLEN,char *pkey) { char *ret=idkey; 
  //Serial.print(F("pair_PROCESS:")); Serial.print(rxbuf); Serial.flush();
  idkey[0]=0; //fail flag
  char msg[64]={0}; char txid[8]={0}; char txkey[18]={0};
  rx_DECODE(msg,rxbuf,bufLEN,pkey);
  byte msgLEN=strlen(msg);
  if ((msg[0]=='!') && (msg[7]=='!')) {
    mySubStr(txid,msg,1,6);
    mySubStr(txkey,msg,8,msgLEN-8);
    strcpy(idkey,txid); strcat_P(idkey,chrCOLON); strcat(idkey,txkey);
  }
  return ret;
}

//*****************************************
char *look4_PUR(char *purOUT, char *purIN) {char *ret=purOUT; // RX expects PUR:IDxxxx:SENSORlabel
  purOUT[0]=0; //default failflag
  char pfx[6]; mySubStr(pfx,purIN,0,4);
  if (strcmp_P(pfx,Kpur)==0) { mySubStr(purOUT,purIN,4,strlen(purIN)); }//strip off the 'PUR:'
  return ret;
}

//***************************************** Paramter AcK - spit back to rcvr as info
//expecting... PAK:IdIdId:0:10:30:2:0 - like
char *look4_PAK(char *pakOUT, char *pakIN) {char *ret=pakOUT; 
  pakOUT[0]=0; //default failflag
  char pfx[6];  mySubStr(pfx,pakIN,0,4);
  if (strcmp_P(pfx,Kpak)==0) { mySubStr(pakOUT,pakIN,4,strlen(pakIN)); }
  return ret;
}

//***************************************** 
//expecting... VER:IdIdId:version#
char *look4_VER(char *verOUT, char *msgIN) {char *ret=verOUT; 
  verOUT[0]=0;
  char tmp[10];  mySubStr(tmp,msgIN,0,4); //first 4 char match
  if (strcmp_P(tmp,Kver)==0) { 
    mySubStr(tmp,msgIN,4,(strlen(msgIN)-4) );
    strcat(verOUT,tmp); }
  return ret;
}

//*****************************************
void pur_PROCESS(char *pktIDNM,byte rssLVL,byte rssLIM) { //looking for txidxx:SENSORlabel
  byte pktLEN=strlen(pktIDNM);
  char txid[10]; mySubStr(txid,pktIDNM,0,6); //0-5,
  word addr= addr_ID_FIND(txid); //overflow writes over addr=10
  if (addr==0) {addr=addr_ID_NEXT(); }
  char lbl[12]; mySubStr(lbl,pktIDNM,7,pktLEN-7); //ididid: 7-end
  if (byte(EEPROM.read(addr))==byte(0xFF)) { //new addition
    prm_EE_SET_DFLT(txid,addr,lbl);  } 
  else {if (rssLVL>=rssLIM) { nameTO_EE(addr,lbl); } } 
  prm_SEND(addr); 
}

//*****************************************
void prm_EE_SET_DFLT(char *id, word addr, char *lbl) {//ID,Interval,Power
  for(byte b=0;b<6;b++) { EEPROM.write((addr-b),id[b]); } //0-5
  EEPROM.write(addr-6,defaultINTERVAL);                   //6
  EEPROM.write(addr-7,defaultHEARTBEAT);                  //7
  EEPROM.write(addr-8,0x22); //Ppv Power default 1,2      //8 001 0,0010
  EEPROM.write(addr-9,0);   //HOLD OPT byte bits all 0    //9
  addr=addr-10;                                           //name
  for(byte b=0;b<10;b++) {EEPROM.write((addr-b),lbl[b]);}  //10-19  
  delay(20); //?? seems to like this to finish up writing to eeprom.
}

//*****************************************
void prm_SEND(word addr) { 
  byte msg[24]; word eeID;  //Param Pointer 
  //PRM:ididid:i:h:ppvpwr:holdopt
  memcpy(msg,"PRM:",4); //0-3
  for (byte i=0;i<6;i++) { msg[i+4]=EEPROM.read(addr-i); } //4-9, ID to prm
  msg[10]=':'; msg[11]=EEPROM.read(addr-6);//interval in wdt timeouts * multiplier in sensor (wdmTXI, probably=1 )
  msg[12]=':'; msg[13]=EEPROM.read(addr-7);//heartbeat, in wdt timeouts * multiplier in sensor (wdmHBI, probably=16 )
  msg[14]=':'; msg[15]=EEPROM.read(addr-8); //PpvPower  (3,5 bits)
  msg[16]=':'; msg[17]=EEPROM.read(addr-9); //HOLDOPT byte, #19, 20th byte 
  //print_HEX(prm,20);
  msg_SEND_HEX(msg,20,rxKEY,2);

  byte id[6];
  for (byte i=0;i<6;i++) { id[i]=msg[i+4]; }
  byte val[4];
  val[0]=msg[11]; val[1]=msg[13]; val[2]=msg[15]; val[3]=msg[17];
  json_PRM_OUT(id,val);
}

//*****************************************
void nameTO_EE(word addr, char *sName) { 
  byte snLEN=strlen(sName);
  addr=addr-10; //where the name goes - after 6 char ID and four paramters
  for (byte b=0;b<10;b++) { char t=sName[b];
    if (b>=snLEN) {t=0;} //fill remaining with 0's
    EEPROM.write(addr-b, t); } //0-5
}

//*****************************************
char* labelFROM_EE(char *sName, char *id) { char *ret=sName;
  word addr=addr_ID_FIND(id);
  byte bp;
  if (addr>0) {
    for (bp=0;bp<10;bp++) { //10 char max
      sName[bp]=EEPROM.read(addr-10-bp); //starting at '6 char id + 4 paramters = 10
      if ((sName[bp]<33) || (sName[bp]>126)) {sName[bp]=0; break;} //10-19 126 ascii is last of 'normal' char.
    }
    sName[10]=0; //insurance
  }  
  return ret;
}

//**********************************************************************
void label_REPLACE(char *buf){ // snr:ididid:snsnsnsnsn
  if ((buf[3]==':') && (buf[10]==':')) {//one more validate
    byte bufLEN=strlen(buf);
    char id[8]; mySubStr(id,buf,4,6);
    char nm[12]; mySubStr(nm,buf,11,bufLEN-11);
    word addr=addr_ID_FIND(id);
    if (addr>0) {nameTO_EE(addr, nm);
    char jsn[34]; strlcpy_P(jsn,Knamerep,20); strcat(jsn,buf); strcat_P(jsn,chrQUOTE);
    json_RX(jsn); }
  }
}

//*****************************************
int prm_EEPROM_GET(char *id, byte pnum) { int val=-1;
  word addr=addr_ID_FIND(id);
  if (addr>0) {val=EEPROM.read(addr-(6+pnum)); }
  return val;
}

//*****************************************
void prm_EEPROM_SET(char *id, byte pnum, byte val) { 
  word addr=addr_ID_FIND(id);
  if (addr>0) {EEPROM.write(addr-(6+pnum),val); }
}

//*****************************************
//uses key in idkey to encode response containing rxKEY
void key_SEND(char *txid, char *txkey, char *rxkey) {
  //Serial.print(F("key_SEND:")); Serial.println(rxkey);
  char txBUF[26]; strcpy(txBUF,txid); strcat_P(txBUF,chrCOLON); strcat(txBUF,rxkey); //ididid:rxkeyrxkeyrxkeyr
  msg_SEND(txBUF,txkey,1); //the TX will decode this. IT made the key
  char jsnKEY[20];
  strlcpy_P(jsnKEY,KsrcRX,20); Serial.print(jsnKEY); //Serial.print(F("{\"source\":\"rx\","));
  strlcpy_P(jsnKEY,Kkey2tx,20); strcat(jsnKEY,txid); strcat(jsnKEY,"\"}");
  Serial.println(jsnKEY); Serial.flush();
}

//*****************************************
void msg_SEND(char *msgIN, char *key, int pwr) { 
  //Serial.print(F("msg_SEND:")); Serial.println(msgIN);
  char txBUF[64]={0}; byte txLEN=strlen(msgIN);
  tx_ENCODE(txBUF,msgIN,txLEN,key);
  rf95.setTxPower(pwr, false); // 2-20
  rf95.send(txBUF,txLEN); rf95.waitPacketSent();
}

//*****************************************
void msg_SEND_HEX(char *hexIN, byte len, char *key, int pwr) { 
  //Serial.print(F("msg_SEND_HEX:"));  Serial.print(hexIN);
  char hexBUF[64]={0}; // memcpy(hexBUF,&hexIN,len);
  tx_ENCODE(hexBUF,hexIN,len,key);
  rf95.setTxPower(pwr, false); //from  2-20dB
  rf95.send(hexBUF,len); rf95.waitPacketSent();
}

//*****************************************
//encode is just before sending, so is non-string char array for rf95.send
//*****************************************
char *tx_ENCODE(char *txBUF, char *msgIN, byte msgLEN, char *key) { char *ret=txBUF;
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
char* rx_DECODE(char *msgOUT, char *rxBUF, byte bufLEN, char *key) { char *ret=msgOUT;
  byte i; byte k=0;
  byte keyLEN=strlen(key); 
  //byte bufLEN=strlen(rxBUF);
  if (modeRDM==3) {Serial.println(F("DecodeIN: "));print_HEX(rxBUF,bufLEN); }
  for (i=0;i<bufLEN;i++) {
    if (k==keyLEN) {k=0;}
    msgOUT[i]=byte((byte(rxBUF[i])^byte(key[k])));
    k++;
  }
  msgOUT[i]=0;
  if (modeRDM==3) {Serial.println(F("DecodeOUT: "));print_HEX(msgOUT,bufLEN); }
  return ret;
}

//*****************************************
void key_EE_MAKE() {
  long rs=analogRead(2);  rs=rs+analogRead(3); rs=rs+analogRead(4);
  rs=rs+analogRead(5); randomSeed(rs);
  for (byte i=0;i<16;i++) { EEPROM.write(EE_KEY-i,random(36,126)); } //exclude " for json string value issue
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
  if (len!=16) { return false; }
  for (byte i=0;i<len;i++) {
    if ((key[i]<36) || (key[i]>126)) { return false; }
  }
  return true;
}

//**********************************************************************
void json_PRM_OUT(byte id[], byte val[]) { char jsnPRM[24];//prm has hex values
  strlcpy_P(jsnPRM,Kprmout,12); // "\"PRM_OUT\":\"";
  char N2A[6]; char cID[8];
  for (byte i=0;i<6;i++) {cID[i]=id[i];} cID[6]=0;
  strcat(jsnPRM,cID); 
  strcat_P(jsnPRM,chrCOLON); itoa(val[0],N2A,10); strcat(jsnPRM,N2A);
  strcat_P(jsnPRM,chrCOLON); itoa(val[1],N2A,10); strcat(jsnPRM,N2A);
  strcat_P(jsnPRM,chrCOLON); itoa(val[2],N2A,10); strcat(jsnPRM,N2A);
  strcat_P(jsnPRM,chrCOLON); itoa(val[3],N2A,10); strcat(jsnPRM,N2A);
  strcat_P(jsnPRM,chrQUOTE);
  json_RX(jsnPRM);
}

//**********************************************************************
void json_KEY(char *key) { char jsnKEY[30];//Krxkey[] PROGMEM = "\"KEY_RX\":\"";
  strlcpy_P(jsnKEY,Krxkey,12); // \"KEY_RX\":\";
  strcat(jsnKEY,key); strcat_P(jsnKEY,chrQUOTE);
  json_RX(jsnKEY);
}

//**********************************************************************
void json_KSS(byte rss) { char jsnKSS[16];
  strlcpy_P(jsnKSS,Kkss,12); // \"KEY_RSS\":\";
  char kss[5]; itoa(int(rss),kss,10); strcat(jsnKSS,kss);
  json_RX(jsnKSS);
}

//**********************************************************************
void json_VER(char *ver) { char jsnVER[24];
  strlcpy_P(jsnVER,Krxver,12); //"\"RX_VER\":\""
  strcat(jsnVER,ver); strcat_P(jsnVER,chrQUOTE);
  json_RX(jsnVER);
}

//**********************************************************************
void json_RX(char *info) {  char src[16]; 
  strlcpy_P(src,KsrcRX,18); Serial.print(src); //Serial.print(F("{\"source\":\"rx\","));
  Serial.print(info);
  Serial.println(F("}")); Serial.flush();
}

//**********************************************************************
void json_TX(char *info) {  char src[16]; 
  strlcpy_P(src,KsrcTX,18); Serial.print(src); //Serial.print(F("{\"source\":\"tx\","));
  Serial.print(info);
  Serial.println(F("}")); Serial.flush();
}

//*****************************************
bool id_VALIDATE(char *id) { //check EEPROM for proper character range
  for (byte i=0;i<6;i++) { 
    if ( ((id[i]<'2')||(id[i]>'Z')) || ((id[i]>'9')&&(id[i]<'A')) ) { return false; }
  }
  return true;
}

//*****************************************
void id_ADD(char *buf) { 
	if (buf[3]==':') {
    char id[8]={0}; mySubStr(id,buf,4,6);
    if (id_VALIDATE(id)==true) {
      word addr= addr_ID_NEXT(); 
      for(byte b=0;b<6;b++) { EEPROM.write((addr-b),id[b]); }
      char jsn[24]; strlcpy_P(jsn,Kidadd,16);
      strcat(jsn,id); strcat_P(jsn,chrQUOTE);
      json_RX(jsn);
    }
  }
}

//*****************************************
void id_DELETE(char *buf) {
  if (buf[3]==':') {
    char id[8]={0}; mySubStr(id,buf,4,6);
    word addr=addr_ID_FIND(id);
    if (addr!=0) {
      for (byte i=0;i<EE_BLKSIZE;i++) { EEPROM.write(addr-i,0xFF); }
      char jsn[26];
      strlcpy_P(jsn,Kidrem,20); strcat(jsn,id); strcat_P(jsn,chrQUOTE);
      json_RX(jsn);
    }
  }
}

//*****************************************
void id_LIST() { char id[8]; char lbl[12]; byte blknum=0; char aVal[6];
  Serial.println("");
  for (word addr=EE_ID;addr>EE_BLKSIZE;addr-=EE_BLKSIZE) {
    for (byte i=0;i<6;i++) { id[i]=EEPROM.read(addr-i); }
    id[6]=0; //null term stringify
    Serial.print(F("* ee-id:"));Serial.print(addr);Serial.print(F(", "));
    blknum++;
    if (byte(id[0])!=byte(0xFF)) { byte n;
      for (n=0;n<10;n++) { //placeholder name=id
        lbl[n]= EEPROM.read(addr-10-n);
        if ((byte(lbl[n])==0) || (byte(lbl[n])==byte(0xFF))) {break;}
      }
      lbl[n]=0; //string term

      Serial.print(blknum); Serial.print(F(":"));
      Serial.print(id); Serial.print(F(":"));
      itoa(int(EEPROM.read(addr-6)),aVal,10);
      Serial.print(aVal); Serial.print(F(":"));
      itoa(int(EEPROM.read(addr-7)),aVal,10);
      Serial.print(aVal); Serial.print(F(":"));
      itoa(int(EEPROM.read(addr-8)),aVal,10);    
      Serial.print(aVal); Serial.print(F(":"));
      itoa(int(EEPROM.read(addr-9)),aVal,10);
      Serial.print(aVal); Serial.print(F(":"));
      Serial.print(lbl);  
    }
    Serial.println("");
  }
  Serial.println("**");Serial.flush();
}

//*****************************************
word addr_ID_NEXT() { word ret=0;
  for (word addr=EE_ID;addr>EE_BLKSIZE;addr-=EE_BLKSIZE) {
    if (byte(EEPROM.read(addr))==byte(0xFF)) {ret=addr; break;}
  }
  return ret;  
}

//*****************************************
word addr_ID_FIND(char *id) { word ret=0; word addr;
  byte ptr; byte eeByte;
  for (addr=EE_ID;addr>EE_BLKSIZE;addr-=EE_BLKSIZE) {
    ptr=0; eeByte=byte(EEPROM.read(addr-ptr));
    while ( eeByte==byte(id[ptr]) ) {  ptr++;
      if (ptr==6) { ret=addr; break; }
      eeByte=byte(EEPROM.read(addr-ptr));
    }
    if (ret>0) {break;}
  }
  return ret;
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

//**********************************************************************
char *B962L(char *lngOUT, char *b96) {char *ret=lngOUT; //Base96 characters TO Long - string - LSB first
//Serial.print("b96:");Serial.println(b96);
  long n=0; long bm32; long l96=1; byte t=1;
  long b=b96[0];
  while (b>=32) { bm32=b-32;
    n=n+(bm32* l96);
    b=b96[t]; 
    l96=l96*96;
    t++;
  }
  ltoa(n,lngOUT,10);
  return ret;
}

//*****************************************
void print_HEX(char *buf,byte len) { byte i;
  Serial.print(len);Serial.print(F(": "));
  for (i=0;i<(len-1);i++) {  if (buf[i]< 0x10) {Serial.print(F("0"));}
    Serial.print(byte(buf[i]),HEX);Serial.print(F(" ")); }
  Serial.println(byte(buf[len-1]),HEX); Serial.flush();
}

//*****************************************
void print_CHR(char *buf,byte len) { byte i;
  Serial.print(len);Serial.print(F(": "));
  for (i=0;i<(len-1);i++) {Serial.print( buf[i]);}
  Serial.println(buf[len-1]);Serial.flush();
}

void showHELP(byte HlpNum) { char sHLP[80];
  Serial.print(F("* ")); Serial.println(VER);
  for (byte i=0;i<HlpNum-1;i++) { *sHLP=0;
    strcat_P(sHLP, (char*)pgm_read_word(&(table_HLP[i])));
    Serial.print(F("* ")); Serial.println(sHLP); }
  Serial.println(F("* "));Serial.flush();
}

void showPPV(byte HlpNum) { char sHLP[80];
  for (byte i=0;i<HlpNum-1;i++) { *sHLP=0;
    strcat_P(sHLP, (char*)pgm_read_word(&(table_PPV[i])));
    Serial.print(F("* ")); Serial.println(sHLP); }
  Serial.println(F("* "));Serial.flush();
}

//*****************************************
char *mySubStr(char *out, char* in,byte from,byte len) { char *ret=out;
  byte p=0; 
  for (byte i=from;i<(from+len);i++) {out[p]=in[i]; p++;}
  out[p]=0;
  return ret;
}

//*****************************************
ISR(WDT_vect) { //in avr library
}
