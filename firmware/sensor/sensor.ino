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
*/
const static char VER[] = "TX250714";
#include "ML_sensor.h"

//*****************************************
void setup () {
  SBN = 255; //'255' => use the 'ID' voltage on A6
  // otherwise, Sensor Board Number will be the one you specify here
  //SBN=0;
  init_SETUP();
}
//*****************************************

//*****************************************
void loop () {
  if ((wakeWHY != 0) && (flgHoldOff==false)) { //wakeWHY=1 is Data, 2 is Heartbeat
    get_DATA(sbnDATA, SBN, wakeWHY);
    packet_SEND(txPPV, SBN, txID, txBV, rxKEY, sbnDATA, txPWR);

    trigger_RESET(SBN); //some sensors need this to work again
    wakeWHY = 0;
    Serial.println(F("Z-z-z-z-z-z...")); Serial.flush();
  }
  systemSleep();

} //End Of Loop ****************************
//*****************************************

//*****************************************
char *get_DATA(char *data, int sbn, byte why ) { char *ret=data; 
  data[0]=0;
  detachInterrupt(digitalPinToInterrupt(pinEVENT));
  if (why==2) { strlcpy_P(data,HB,12);} //HEARTBEAT
  else {  //if (sbn==0) {sbn=12;}
    switch (sbn) { 
      case -1: { strlcpy_P(data,SDM1,10);  } break; //BEACON
      case 0: { strlcpy_P(data,SD00,10); } break; //for experimenting
      case 1: { strlcpy_P(data,SD01,10);  } break; //  PUSH
      case 2: { get_TILT(data,txSNSROPT); } break; // 
      case 3: { get_REED(data);   } break; //
      case 4: { strlcpy_P(data,SD04,10); } break; //SHAKE
      case 5: { strlcpy_P(data,SD05,10); } break; //MOTION
      case 6: { strlcpy_P(data,SD06,10); } break; //KNOCK
      case 7: { get_2BTN(data);       } break; // //2BUTTON
      case 8: { strlcpy_P(data,SD08,10);  } break; //
      case 9: { strlcpy_P(data,SD09,10);  } break; //
      case 10: { strlcpy_P(data,SD10,10);    } break; //SB#10 //100K/82K TMP36
      case 11: { get_LIGHT(data,txSNSROPT);   } break; //82K/100K photocell
      case 12: { get_TH1of5(data,txLABEL,txSNSROPT); } break; //75K/100K Si7020
      case 13: { get_MAX1of2(data,txLABEL,txSNSROPT);} break; //get_MAX6675(data,txSNSROPT);    } break; //62K/100K  MAX31855K
      case 14: { get_THL(data,txLABEL,txSNSROPT);    } break; //47K/100K
      case 15: { strlcpy_P(data,SD15,10);    } break; //SB#15 39K/100K SB#15
      case 16: { strlcpy_P(data,SD16,10);    } break; //SB#16
      case 17: { strlcpy_P(data,SD17,10);    } break; //SB#17
      case 18: { strlcpy_P(data,SD18,10);    } break; //SB#18
      case 19: { strlcpy_P(data,SD19,10);    } break; //SB#19
      case 20: { strlcpy_P(data,SD20,10);    } break; //SB#20
      case 21: { get_DOT(data);}  break; //
      case 22: { strlcpy_P(data,SD22,10);    } break; // SB#22 SBN pin tied to Aref
    } //eo_switch
    if (txHOLDOFF>0) {flgHoldOff=true; HoldOffCtr=txHOLDOFF; wdt_RESET();}
  }
  return ret;
}

//*****************************************
void init_SETUP(){ 
  flgLED_KEY=false;
  //pinBOOT_SW is connected to the reset pin.
  pinMode(pinBOOT_SW, INPUT); digitalWrite(pinBOOT_SW, HIGH); //so, do first
  pinMode(pinLED_TX, OUTPUT);
  digitalWrite(pinLED_TX, HIGH); delay(50);
  digitalWrite(pinLED_TX, LOW);
  pinMode(pinRF95_INT, INPUT);
  pinMode(pinRF95_CS, OUTPUT); digitalWrite(pinRF95_CS, LOW);
  pinMode(pinBOOST, OUTPUT); digitalWrite(pinBOOST, LOW);
  pinMode(pinLED_BOOT, OUTPUT); digitalWrite(pinLED_BOOT, LOW);
  analogReference(EXTERNAL); //3.0V vref.
      
  Serial.begin(57600);
  Serial.println("");Serial.println(VER);
  //freeMemory();
  boost_ON();
  if (longPress()==true) {EE_ERASE_all(); }
    
  if (SBN==255) {SBN=get_SBNum();}
  //!!!!!!!!!!!!!!!!!!!!!
  //if (SBN==0) {SBN=12;}
  //!!!!!!!!!!!!!!!!!!!!!
  Serial.print(F("SBN: "));Serial.print(SBN);Serial.flush();
  
  id_GET(txID,SBN);
  Serial.print(F(", txID: "));Serial.println(txID);Serial.flush();

  prm_EE_GET(SBN); prmPRINT();

  txBV = get_BatteryVoltage();
 
  init_SENSOR(txLABEL,SBN); //txLABEL gets assigned here
  Serial.print(F("txLABEL:"));Serial.println(txLABEL);

  //name_EE_GET(txLABEL,SBN); //why? name is a receiver thing from now on.
  //Serial.print(F("chr12:"));Serial.println(chr12);
  
  if (init_RF95(txPWR)==true) {
    //key_REQUEST(rxKEY,txID);
    key_REQUEST(rxKEY,txID);
    Serial.print(F("rxKEY: "));Serial.println(rxKEY);Serial.flush();
    if (rxKEY[0]!=0) {
      if (key_VALIDATE(rxKEY)==true) { key_EE_SET(rxKEY); flgLED_KEY=true; }
      delay(10); ver_SEND(txID,rxKEY,VER);  
    }
  }
  rxKEY[0]=0;
  key_EE_GET(rxKEY);
  Serial.print(F("ee-rxKEY: "));Serial.println(rxKEY);Serial.flush();
  flgKEY_GOOD=key_VALIDATE(rxKEY);
  delay(100);
//******************
  if (flgKEY_GOOD==true) {
    char sbnNAME[20];//={0}; //to play with txLABEL without changing it.
    strcpy(sbnNAME,txLABEL);
    //txLABEL may have a sensor-variation routing suffix that begins
    //with delimiter '|'. If it does, it needs to be stripped off
    // before sending txLABEL to receiver - by changing it to 0.
    byte pos=myChrPos(sbnNAME,'|'); if (pos>0) {sbnNAME[pos]=0;}
    char MSG[40]; 
    strlcpy_P(MSG,PUR,6); strcat(MSG,txID); strcat(MSG,":"); strcat(MSG,sbnNAME);
    msg_SEND(MSG,rxKEY,txPWR); //Parameter Update Request out
    rx_LOOK(MSG,rxKEY,20); //200mSec
    //Serial.print(F("MSG: "));Serial.println(MSG);Serial.flush();
    if (MSG[0]!=0) { prm_PROCESS(MSG,txID,SBN); prmPRINT();}
    //***********************
    Serial.print(F("rxKEY: "));Serial.println(rxKEY);Serial.flush();
    get_DATA(sbnDATA,SBN,1);
    packet_SEND(txPPV, SBN, txID, txBV, rxKEY, sbnDATA, txPWR);
    //***********************
  }
  if (HrtBtON==true) { wd_TIMER=wd_HEARTBEAT; }
  else { wd_TIMER=wd_INTERVAL; }

  //sleep and 'power down' mode bits
  cbi( SMCR, SE ); cbi( SMCR, SM0 ); sbi( SMCR, SM1 ); cbi( SMCR, SM2 ); 
  //watchdog timer - 8 sec
  cli(); wdt_reset();
  WDTCSR |= B00011000; WDTCSR = B01100001;
  sei(); //watchdog timer - 8 sec   
  wd_COUNTER=wd_TIMER-1; //for first HeartBeat = 8 sec. 
  Serial.print(F("wd_TIMER: "));Serial.println(wd_TIMER);Serial.flush();
  Serial.print(F("rxKEY: "));Serial.println(rxKEY);Serial.flush();
  ledBOTTOM_OnOffCnt(1000,500,1);
  if (flgKEY_GOOD==false){ledBOTTOM_OnOffCnt(1000,500,1);}  //2nd long flash?
  if (flgLED_KEY==true) { ledBOTTOM_OnOffCnt(200,200,3); } //3 flashes
  Serial.print(F("txLABEL: "));Serial.println(txLABEL);Serial.println("");Serial.flush();
  wakeWHY=0; 

} //* END OF init_SETUP ************************
//**********************************************************************************

//**********************************************************************************
char* init_SENSOR(char *sbName, int sbNum) { char* ret=sbName; 
  DATA_TYPE = BEACON; //preset default
  Serial.print(F("boost="));Serial.println(digitalRead(pinBOOST));Serial.flush();
  switch (sbNum) {  strlcpy_P(sbName,SNqqqq,10); //default to '????' not found'
    case -1: { DATA_TYPE = BEACON; strlcpy_P(sbName,SNM1,10);      } break; //Minus 1 BEACON
    case 0: { DATA_TYPE = DIGITAL_I2C; strlcpy_P(sbName,SN00,10);  } break;
    case 1: { DATA_TYPE = EVENT_RISE;  strlcpy_P(sbName,SN01,10);//"BUTTON"); 
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW);  } break;
    case 2: { DATA_TYPE = EVENT_RISE;  strlcpy_P(sbName,SN02,10); strcpy(dataOLD,"????"); //"TILT");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW);  } break;
    case 3: { DATA_TYPE = EVENT_CHNG; strlcpy_P(sbName,SN03,10);//"REED");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW);  } break;
    case 4: { DATA_TYPE = EVENT_FALL; strlcpy_P(sbName,SN04,10);//"SHAKE");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, HIGH); } break;
    case 5: { DATA_TYPE = EVENT_RISE; strlcpy_P(sbName,SN05,10);//"MOTION");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW);
      //pinMode(pinSWITCH, OUTPUT);  digitalWrite(pinSWITCH, LOW);
      pinMode(A0, OUTPUT);  digitalWrite(A0, LOW);
      init_E931();} break;
    case 6: { DATA_TYPE = EVENT_FALL; strlcpy_P(sbName,SN06,10);//"KNOCK");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, HIGH); } break; 
     case 7: { DATA_TYPE = EVENT_RISE; strlcpy_P(sbName,SN07,10);//"2BTN");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT,LOW); } break; 
    case 10: { DATA_TYPE = ANALOG; strlcpy_P(sbName,SN10,10); CAL_VAL=analogRead(pinTrimPot); } break;//"TMP36"); 
    case 11: { DATA_TYPE = ANALOG; strlcpy_P(sbName,SN11,10); //"LIGHT%");
                MAX1=0;//eeREAD2(EE_MAX1);
                MIN1=1023;//eeREAD2(EE_MIN1);
                } break;  
    case 12: { DATA_TYPE = DIGITAL_I2C; strlcpy_P(sbName,SN12,10); } break; //"T-RH");
    case 13: { DATA_TYPE = DIGITAL_SPI; strlcpy_P(sbName,SN13,10);  //"MAX6675K");//or MAX31855?
              SPI_SI=4; SPI_CS=5; SPI_CLK=6; 
              pinMode(SPI_SO, INPUT);    // SO=D4
              pinMode(SPI_CS, OUTPUT);  digitalWrite(SPI_CS,HIGH); // /CS=D5
              pinMode(SPI_CLK, OUTPUT);  digitalWrite(SPI_CLK,LOW);  } break;   
    case 14: { DATA_TYPE = DIGITAL_I2C; strlcpy_P(sbName,SN14,10); } break;//"THL");       
    case 21: { DATA_TYPE = EVENT_RISE; strlcpy_P(sbName,SN21,10);  //"MOT-DOT");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW);
      pinMode(A0, OUTPUT);  digitalWrite(A0, LOW);
      pinMode(A4,INPUT);digitalWrite(A4, LOW);
      pinMode(A5,INPUT);digitalWrite(A5, LOW);
      init_E931();} break;
    case 22: { DATA_TYPE = BEACON; strlcpy_P(sbName,SN22,10); } break; //"SBN22"); } break;
  }
  init_TYPE(DATA_TYPE);// enable interrupts, wire, etc.
  Serial.print(F("sbName:"));Serial.println(sbName);
  switch (sbNum) { //supplimental sensor-specific inits
    case 12:  { //Find the one - Si7020, SHT40, DHT11, DHT22 or HS3004.
      char ssfx[10]; whichTH(ssfx,txSNSROPT); strcat(sbName,"|"); strcat(sbName,ssfx);  } break;
    case 13:  { //Find the MAX- 6675 or 31855.
      char ssfx[10]; whichMAX(ssfx,txSNSROPT); strcat(sbName,"|"); strcat(sbName,ssfx);  } break;      
    case 14:  { //Find the one - Si7020, SHT40, DHT11, DHT22 or HS3004.
      char ssfx[10]; whichTH(ssfx,txSNSROPT); strcat(sbName,"|"); strcat(sbName,ssfx); } break;
  }
  return ret;
} //eo_Init_Sensor

//*****************************************
char *whichTH(char *sfx, byte optns) { char *ret=sfx;
  int rslt; char wTH[10];
  //crap... SHT40 and HS3004 share same address - both will return data.
  //hey!- HS3X reading a SHT40 returns ---,--- but
  // SHT40 reading a HS3004 returns data, so...
  // read HS3X first.- but do them last.
  Serial.print(F("Si7020="));
  Wire.begin(); Wire.setWireTimeout(2000, true);
  get_TH_Si7020(wTH,optns); 
  Serial.println(wTH);Serial.flush();
  rslt=strcmp_P(wTH,FAIL2); //"---|---"
  if (rslt!=0) {strcpy(sfx,"Si7020"); return ret;} 
  //----------------
  Serial.print(F("DHTxx=")); //a DHT22?
  get_TH_DHTxx(wTH,11,0); //optns=0->degF-> DHT22 = "32.0|" or "33.8|"
  Serial.println(wTH);Serial.flush();
  //got a problem here... DHT22 when read as 11 can return different values
  //depending on temp. Seems to be 'cool' is 32.0, warmer is 33.8, warmer still
  //goes up by 1.8 more
  //so, four checks for that, DHT22a,b,c,d
  char tmp[6]; mySubStr(tmp,wTH,0,5); //rh can vary - so ignore it
  rslt=strcmp_P(tmp,DHT22a);
  if (rslt==0) { strcpy(sfx,"DHT22");return ret;}
  rslt=strcmp_P(tmp,DHT22b);
  if (rslt==0) { strcpy(sfx,"DHT22");return ret;}
  rslt=strcmp_P(tmp,DHT22c);
  if (rslt==0) { strcpy(sfx,"DHT22");return ret;}
  rslt=strcmp_P(tmp,DHT22d);
  if (rslt==0) { strcpy(sfx,"DHT22");return ret;}
  rslt=strcmp_P(wTH,FAIL2);
  if (rslt!=0) { strcpy(sfx,"DHT11");return ret;}
  //----------------     
  Serial.print(F("HS3X=")); 
  Wire.begin(); Wire.setWireTimeout(2000, true);
  get_TH_HS3X(wTH,optns); 
  Serial.println(wTH); Serial.flush();
  rslt=strcmp_P(wTH,FAIL2);
  if (rslt!=0) {strcpy(sfx,"HS3X"); return ret;}
  //----------------
  Serial.print(F("SHT40=")); 
  Wire.begin(); Wire.setWireTimeout(2000, true);
  get_TH_SHT40(wTH,optns);
   Serial.println(wTH);Serial.flush();
  rslt=strcmp_P(wTH,FAIL2);
  if (rslt!=0) { strcpy(sfx,"SHT40");  return ret;}

  strlcpy_P(sfx,FAIL1,10); //"---"
  return ret;   
}

//*****************************************
void init_TYPE(TYPE sbt){
   //="BEACON", "EVENT_LOW","EVENT_CHNG","EVENT_RISE",
   //="EVENT_FALL","ANALOG", "DIGITAL_SPI", "DIGITAL_I2C"
  switch (sbt) {
    case BEACON :{ HrtBtON=true; } break;
    case EVENT_LOW :{ HrtBtON=true;
      attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, LOW); } break;
    case EVENT_CHNG :{HrtBtON=true;  
      attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, CHANGE); } break;
    case EVENT_RISE :{ HrtBtON=true;  
      attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, RISING); } break;
    case EVENT_FALL :{ HrtBtON=true;  
      attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, FALLING); } break; 
    
    case ANALOG :{ HrtBtON=false; } break;
    case DIGITAL_SPI :{ HrtBtON=false; } break;
    case DIGITAL_I2C :{ HrtBtON=false; } break;
  }
}

//*****************************************  
void name_EE_SET(char *sName, int sbn) {  sbn++;
  word addr=EE_NAME-((sbn)*EE_BLKSIZE);
  for (byte b=0;b<10;b++) {
    if ((sName[b]==0) || (sName[b]==0xFF)) { break; }
    else { EEPROM.write(addr-b,sName[b]); }
  }
}

//*****************************************  
char *name_EE_GET(char *sName, int sbn) { sbn++; char *ret=sName;
  word addr=EE_NAME-((sbn)*EE_BLKSIZE);
  if (EEPROM.read(addr)==0xFF) {return ret; }
  else { byte b;
    for (b=0;b<10;b++) {  sName[b]=EEPROM.read(addr-b);
      if ((sName[b]==0) || (sName[b]==0xFF)) { break; }
    }
  sName[b]=0;
  return ret;
  }
}

//*****************************************
//Sensor NaMe returned,Sensor Board Num, Name Length, Block Size
char *eeSTR_GET(char *str, word addr, byte bix,  byte nl, byte bs) { char *ret=str;
  //a utility to 'get this much from there' from eeprom.
  //and return as a c-sring - if not empty. But does return 0.
  if (EEPROM.read(addr)==0xFF) {return ret; }
  byte b; byte nb; 
  for (b=0;b<nl;b++) {
    str[b]=EEPROM.read(addr-b);
    if ((str[b]==0xFF) || (str[b]==0)) { break; }
  } 
  str[b]=0;
  return ret;
}

//*****************************************
void ver_SEND(char *id,char *key, char *ver) {
  char MSG[70]; 
  strcpy(MSG,"VER:"); strcat(MSG,ver);        
  //Serial.print(F("ver_SEND:"));Serial.println(MSG);Serial.flush();
  msg_SEND(MSG,key,1);
}  
  
//*****************************************
void prm_PROCESS(char *buf, char *id, int sbn) {
  char mySS[10];
  mySubStr(mySS,buf,0,4);//PRM:ididid:ivl:hrt:ppvpwr:holdopt
  if (strcmp(mySS,"PRM:")==0) {
    mySubStr(mySS,buf,4,6);
    if (strcmp(mySS,id)==0) {
      if ((buf[3]==':') && (buf[10]==':') && (buf[12]==':')
       && (buf[14]==':') && (buf[16]==':') ) {
        prm_EE_SET(buf,sbn);
        prm_PAKOUT();
        prm_EE_GET(sbn);
        ledBOTTOM_OnOffCnt(10,10,3); }        
    }
  }
}

//*****************************************
void prm_PAKOUT() { //ack paramter settings back to Rx as...
  // 
  char N2A[10]; // for Number TO Ascii things
  char MSG[70];
  strcpy(MSG,"PAK:"); strcat(MSG,txID);
  strcat(MSG,":"); itoa(int((wd_INTERVAL*86)/10),N2A,10);
  strcat(MSG,N2A); //sec.
  strcat(MSG,":"); itoa(int((long(wd_HEARTBEAT)*86)/600),N2A,10);
  strcat(MSG,N2A); //min.
  strcat(MSG,":"); itoa(txPWR,N2A,10);
  strcat(MSG,N2A);
  strcat(MSG,":"); itoa(txPPV,N2A,10);
  strcat(MSG,N2A);
  strcat(MSG,":"); itoa(txHOLDOFF,N2A,10);
  strcat(MSG,N2A);    
  strcat(MSG,":"); itoa(txSNSROPT,N2A,10);
  strcat(MSG,N2A); 
  //Serial.print(F("prm_PAKOUT:"));Serial.println(MSG);Serial.flush();
  msg_SEND(MSG,rxKEY,1);
  delay(10);
}

//*****************************************
void prmPRINT() {
  Serial.print(F("* wd_INTERVAL:"));Serial.print(wd_INTERVAL);
  Serial.print(F(", wd_HEARTBEAT:"));Serial.println(wd_HEARTBEAT);
  Serial.print(F("* txPPV:"));Serial.print(txPPV);
  Serial.print(F(", txPWR:"));Serial.println(txPWR);
  Serial.print(F("* txHOLDOFF:"));Serial.print(txHOLDOFF);
  Serial.print(F(", txSNSROPT:"));Serial.println(txSNSROPT);
}

//*****************************************
void prm_EE_SET(char *buf,int sbn) { sbn++; //PRM:ididid:ivl:hrt:ppvpwr:holdopt
  EEPROM.write((EE_INTERVAL-(sbn*EE_BLKSIZE)),buf[11]);
  wd_INTERVAL=int(byte(buf[11])*wdmPDI);
  EEPROM.write((EE_HRTBEAT-(sbn*EE_BLKSIZE)),buf[13]);
  wd_HEARTBEAT=int(byte(buf[13])*wdmHBI);
  EEPROM.write((EE_PPVPWR-(sbn*EE_BLKSIZE)),buf[15]);
  txPWR=byte(buf[15]) & 0x1F; //low 5 bits
  txPPV=byte(buf[15])>>5;     //top 3 -> 2-0
  EEPROM.write((EE_HOLDOPT-(sbn*EE_BLKSIZE)),buf[17]);
  txHOLDOFF=byte(buf[17]) >> 4; // top 4 -> 3-0
  txSNSROPT=byte(buf[17]) & 0x0F; //low 4 
  //low 4 bits avail for... ?
}

//*****************************************
void prm_EE_GET(int sbn) { sbn++;
  if (flgEE_ERASED==true){
    Serial.println(F("flgEE_ERASED"));
    EEPROM.write((EE_INTERVAL-(sbn*EE_BLKSIZE)),defaultINTERVAL);
    EEPROM.write((EE_HRTBEAT-(sbn*EE_BLKSIZE)),defaultHEARTBEAT);
    EEPROM.write((EE_PPVPWR-(sbn*EE_BLKSIZE)),0x22); //001 00010 ppv=1, pwr=2
    EEPROM.write((EE_HOLDOPT-(sbn*EE_BLKSIZE)),0);
    flgEE_ERASED=false;
  }
  wd_INTERVAL=EEPROM.read(EE_INTERVAL-(sbn*EE_BLKSIZE))*wdmPDI;
  wd_HEARTBEAT=EEPROM.read(EE_HRTBEAT-(sbn*EE_BLKSIZE))*wdmHBI;
  byte pvvpwr=EEPROM.read(EE_PPVPWR-(sbn*EE_BLKSIZE));
  txPWR= pvvpwr & 0x1F;   //5 bits
  txPPV= (pvvpwr >> 5) ;  //3 bits 
  byte holdopt=EEPROM.read(EE_HOLDOPT-(sbn*EE_BLKSIZE));
  txHOLDOFF=(holdopt>>4); //top 4 -> 3-0
  txSNSROPT=(holdopt & 0x0F); //low 4
}

//*****************************************
void eeWRITE2(word eeloc, word data) {
  byte msb=byte((data >> 4)& 0xFF);
  byte lsb=byte( data & 0xFF);
  EEPROM.write(eeloc,msb);
  EEPROM.write(eeloc-1,lsb);
}

//*****************************************
word eeREAD2(word eeloc) {
  byte msb=EEPROM.read(eeloc);
  byte lsb=EEPROM.read(eeloc-1);
  return ((msb*256)+lsb);
}

//*****************************************
bool init_RF95(int txpwr)  {
  if (RF95_UP==false) { //not already done before?
    if (digitalRead(pinBOOST) == 0) { boost_ON(); }
    byte timeout=0;
    while (!rf95.init() && (timeout<20)) { delay(10); timeout++; }
    if (timeout!=20) {
      rf95.setFrequency(RF95_FREQ); delay(5);
      if (txpwr<5) {txpwr=5;}
      if (txpwr>23) {txpwr=23;} 
      rf95.setTxPower(txpwr, false);  delay(5);
      RF95_UP=true;
    }
  }
  return RF95_UP;
}

//*****************************************
void packet_SEND(byte ppv, int sbn, char *id, double bv, char *key, char *data, int pwr) { 
  char N2A[6]; char MSG[50]; char dlm[2]="|"; 
  
  if (data[0]!=0) { char pv[2]; pv[0]= 48+ppv; pv[1]=0;
    strcpy(MSG,pv); //ppv, packet Protocol Version - very first char byte out;
    strcat(MSG,dlm);strcat(MSG,id); //Sensor ID
    
    switch (ppv) {
      //0 - just add Data
      case 0: { } break; //nada - just ID and Data
      //1 - plus BV 
      case 1: { strcat(MSG,dlm); dtoa(N2A,bv,1); strcat(MSG,N2A); } break;
      //2 - plus BV, SBN
      case 2: { strcat(MSG,dlm); dtoa(N2A,bv,1); strcat(MSG,N2A); }  
                strcat(MSG,dlm); itoa(sbn,N2A,10); strcat(MSG,N2A); break;
      //3 - plus BV, TX count
      case 3: { strcat(MSG,dlm); dtoa(N2A,bv,1); strcat(MSG,N2A);
                char txCtr[4];  L2B96_3(txCtr,txCOUNT); //Long to Base96 as 3 char.
                strcat(MSG,dlm); strcat(MSG,txCtr); 
                if (wakeWHY==1) {txCOUNT++;} } break;
      //4 - plus BV, SBN, TX count??
      case 4: { strcat(MSG,dlm); dtoa(N2A,bv,1); strcat(MSG,N2A);
                strcat(MSG,dlm); itoa(sbn,N2A,10); strcat(MSG,N2A);
                char txCtr[4];  L2B96_3(txCtr,txCOUNT); //Long to Base96 as 3 char.
                strcat(MSG,dlm); strcat(MSG,txCtr);
                if (wakeWHY==1) {txCOUNT++;} } break;  
    }
    //and Data - which can be 4 delimiters + (4*10 char) more, so...
    //1+1+6+1+3+1+2+1+6+1+44 -ish bytes max ( 67 )
    strcat(MSG,dlm); strcat(MSG,data);                   
    msg_SEND(MSG,key,pwr); 
    sbnDATA[0]=0;
  }
}

//*****************************************
void msg_SEND(char *msg, char *key, int pwr) { 
  Serial.print(F("msg_SEND: "));Serial.println(msg);Serial.flush();
   if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(100);}
  digitalWrite(pinLED_TX, HIGH);
  byte txLEN=strlen(msg);
  char txENbuf[70]; tx_ENCODE(txENbuf,msg,txLEN,key);
  if (RF95_UP==false) { RF95_UP=init_RF95(pwr); }
  else {rf95.setTxPower(pwr, false); }//from 1-10 to 2-20dB
  rf95.send(txENbuf,txLEN); rf95.waitPacketSent();
  txBV = get_BatteryVoltage();
  digitalWrite(pinLED_TX, LOW); //or let boost_OFF do that?
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

//************************************************************
char *get_TILT(char *data, byte optns) { char *ret=data;
  byte smplCtr=0; byte d1=0; byte d2=0;
  while (smplCtr<200) {//2 sec of stable
    d1=digitalRead(A4);
    if (d1==d2) {smplCtr++;}
    else {smplCtr=0;}
    d2=d1;
    delay(10); //10 * 200 = 2 sec.
  }
  //the 4 bits in txSNSROPT used here.
  if (bitRead(optns,3)==1){d1=!d1;} //??what high and low mean wrt tilt contact?
  if ( (d1==0) && (strcmp_P(dataOLD,TILT00)!=0) ){ strlcpy_P(data,TILT00,10); strcpy(dataOLD,data); }
  else if ((d1==1) && (strcmp_P(dataOLD,TILT01)!=0) ) { strlcpy_P(data,TILT01,10); strcpy(dataOLD,data); }

  switch (optns & 0x07) { //not the state bit
    case 1: {if (strcmp_P(data,TILT00)==0) {strlcpy_P(data,TILT10,10);} else {strlcpy_P(data,TILT11,10);} } break; //horiz/vert
    case 2: {if (strcmp_P(data,TILT00)==0) {strlcpy_P(data,TILT20,10);} else {strlcpy_P(data,TILT21,10);} } break; //left/right
    case 3: {if (strcmp_P(data,TILT00)==0) {strlcpy_P(data,TILT30,10);} else {strlcpy_P(data,TILT31,10);} } break; //open/close
    case 4: {if (strcmp_P(data,TILT00)==0) {strlcpy_P(data,TILT40,10);} else {strlcpy_P(data,TILT41,10);} } break; //up/down
  }
  return ret;
}

//************************************************************
char *get_REED(char *data) { char *ret=data;
  if (digitalRead(pinEVENT)==HIGH){strlcpy_P(data,REED0,10);}//"CLOSE");}
  else {strlcpy_P(data,REED0,10);} //open
  return ret;
}

//************************************************************
char *get_2BTN(char *data) { char *ret=data;
    delay(10);
    if ((digitalRead(A5)==1) && (digitalRead(A4)==1)) { strlcpy_P(data,PB11,10); } //"PUSH-3");}
    else if (digitalRead(A5)==1) { strlcpy_P(data,PB01,10); } //"PUSH-1");}
    else if (digitalRead(A4)==1) { strlcpy_P(data,PB10,10); } //"PUSH-2");}
  return ret;
}

//************************************************************
char *get_DOT(char *data) { char *ret=data;
  if ((digitalRead(A4)==0)&& (digitalRead(A5)==1)){strlcpy_P(data,MOT10,10); }//"MOT-LEFT",);}
  else if ((digitalRead(A4)==1)&& (digitalRead(A5)==0)){strlcpy_P(data,MOT01,10); }//"MOT-RIGHT");}
  else if ((digitalRead(A4)==1)&& (digitalRead(A5)==1)){strlcpy_P(data,MOT11,10); }//"MOT-BOTH");}
  return ret;
 }

//************************************************************
char *get_TH1of5(char *dataTH, char *sName, byte optns) { char *ret=dataTH;
  byte pos=myChrPos(sName,'|')+1; //is -1 if not found so +1 is 0
  //Serial.print(F("pos: "));Serial.println(pos);Serial.flush();
  if (pos>0) {
    char sfx[10]={0}; mySubStr(sfx,sName,pos,(strlen(sName)-(pos)));
//Serial.print(F("sfx: "));Serial.println(sfx);Serial.flush();
    if (strcmp(sfx,"Si7020")==0) { get_TH_Si7020(dataTH, optns); }
    else if (strcmp(sfx,"SHT40")==0) { get_TH_SHT40(dataTH, optns); }
    else if (strcmp(sfx,"HS3X")==0) { get_TH_HS3X(dataTH, optns); }
    else if (strcmp(sfx,"DHT11")==0) { get_TH_DHTxx(dataTH,11, optns); }
    else if (strcmp(sfx,"DHT22")==0) { get_TH_DHTxx(dataTH,22, optns); }
  }
  //Serial.print(F("dataTH: "));Serial.println(dataTH);Serial.flush();
  return ret;
}

//************************************************************
char *get_THL(char *dataTHL, char *sName ,byte opt) { char *ret=dataTHL;
  char myTHL[20]; //could be 12, but am doing x10 for compiler re-use.
  get_TH1of5(dataTHL,sName,opt);
  get_LIGHT(myTHL,opt); //global chr12F[10];
  strcat(dataTHL,"|");  strcat(dataTHL,myTHL);
  return ret;
}

//*********************** Light uses option bit 1 (=2)
char *get_LIGHT(char *dataL, byte optns) { char *ret=dataL;
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(10);}
  long lLVL=analogRead(0); //1st pancake of Light Level
  lLVL=analogRead(0); lLVL=((lLVL+analogRead(0))/2);
  int iPct;  
  if ((optns & 0x02)==2) { //auto-range on
    if (lLVL>MAX1) {MAX1=lLVL;}
    if (lLVL<MIN1) {MIN1=lLVL;} 
    if (MAX1==MIN1) { iPct=0; }
    else { iPct= int(((lLVL-long(MIN1))*100)/(MAX1-MIN1)); }
  }
  else {iPct= int((lLVL*100)/1023); }
  if (iPct>100) {iPct=100;} else if (iPct<0) {iPct=0;}  
  itoa(iPct,dataL,10); 
  return ret;
}

//************************************************************
char *get_TH_DHTxx( char *dataDHT, byte type ,byte options) { char *ret=dataDHT;
  if (digitalRead(pinBOOST) == 0) { boost_ON();}
  strlcpy_P(dataDHT,FAIL2,10);//"---|---"); //prep for fail
  TWCR=0; //!! END any I2C that did a BEGIN !!
  dht DHT;
  delay(1000); //pwr-up stabilize time
  int chk;
  byte trynum=0; byte trylim=5;
  while (trynum<trylim) {
    switch (type) {
      case 11: { chk = DHT.read11(pinSDA);} break;
      case 12: { chk = DHT.read12(pinSDA);} break;
      case 22: { chk = DHT.read22(pinSDA);} break;
    }
    if (chk==DHTLIB_OK) { trynum=trylim;
      float val = DHT.temperature;
      if ((options & 0x01)==0) {val = (val*1.8)+32;} //33.8 if val=1
      char N2A[10]; 
      dtoa(N2A,val,1); strcpy(dataDHT,N2A);
      val = float(DHT.humidity);
      dtoa(N2A,val,0); strcat(dataDHT,"|"); strcat(dataDHT,N2A);    
    }
    else {trynum++;
      delay(500);
    }
  }
  return ret;
}

//************************************************************
char *get_TH_HS3X(char *data3X, byte optns) { char *ret=data3X;
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(100);}
  strlcpy_P(data3X,FAIL2,10);//"---|---"); //prep for fail
  TWCR=1; Wire.beginTransmission(0x44);  delay(20); //!WHAT! same address as SHT40???
  Wire.requestFrom(0x44, 4,1);
  if(Wire.available() == 4) {
    byte msb=Wire.read(); byte lsb=Wire.read();
    word wHUM=(msb*256)+lsb;
    msb=Wire.read(); lsb=Wire.read();
    word wTEMP=(msb*256)+lsb;
    word wSTATUS = wHUM>>14;
    wHUM=wHUM & 0x3FFF;  //mask 2-bits high
    wTEMP=wTEMP >>2;  //mask 2-bits low
    if ((wHUM==0x3FFF)||(wTEMP==0x3FFF)) { return ret; }
    float fVAL=(float(wTEMP)*0.010071415)- 40.0;  //Cel
    if ((optns & 0x01)==0) {fVAL=(fVAL*1.8)+32.0;} //Fahrenheit
    char myTH[10]; dtoa(myTH,fVAL,1); 
    fVAL=float(wHUM)*0.006163516;  //rh
    char cRH[6]={0}; itoa(int(fVAL),cRH,10);
    strcpy(data3X,myTH);strcat(data3X,"|");strcat(data3X,cRH);
    //Serial.print(F("data3X: "));Serial.println(data3X);Serial.flush();
  }
  return ret;
}

//***********************
char *get_TH_SHT40(char *dataSHT, byte optns) {char *ret=dataSHT;
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(100);}
  strlcpy_P(dataSHT,FAIL2,10);//"---|---"); //prep for fail
  byte msb; byte lsb; byte chksum; word wVAL; float fVAL;
  TWCR=1; Wire.beginTransmission(0x44);  Wire.write(0xFD);
  Wire.endTransmission();  delay(20);
  Wire.requestFrom(0x44, 6);
  if(Wire.available() >= 6) {
    msb=Wire.read(); lsb=Wire.read(); chksum=Wire.read();
    wVAL=(msb*256)+lsb;
    fVAL=((175.0 * (float(wVAL))/ 65535.0))-45; //celcius
    if ((optns & 0x01)==0) {fVAL=(fVAL*1.8)+32.0;} //Fahrenheit
    char cTMP[8]={0}; dtoa(cTMP,fVAL,1);
    msb=Wire.read(); lsb=Wire.read(); chksum=Wire.read();
    wVAL=(msb*256)+lsb;
    fVAL=((125.0 * (float(wVAL))/ 65535.0))-6; //rh
    if (fVAL>100) { fVAL=100; } else if (fVAL<0) { fVAL=0; }
    char cRH[6]={0}; itoa(int(fVAL),cRH,10);
    strcpy(dataSHT,cTMP);strcat(dataSHT,"|");strcat(dataSHT,cRH);
  }
  return ret;
}

//*****************************************
char *get_TH_Si7020(char *dataTH, byte optns) { char *ret=dataTH;
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(100);}
  init_Si7020(); delay(10);
  char data1[10]={0}; char data2[10]={0};
  get_T_Si7020(data1,optns); //can be "---" FAIL1
  get_H_Si7020(data2);//and with this FAIL1 makesd FAIL2
  strcpy(dataTH,data1); strcat(dataTH,"|"); strcat(dataTH,data2);
  return ret;
}

//***********************
char *get_T_Si7020(char *dataT, byte optns) { char *ret=dataT;
  byte msb; byte lsb; word wVAL; float fVAL;
  strlcpy_P(dataT,FAIL1,10);//"---"); //prep for fail
  TWCR=1; Wire.beginTransmission(0x40);  Wire.write(0xF3);
  Wire.endTransmission();  delay(20);
  Wire.requestFrom(0x40, 2);
  if(Wire.available() >= 2) {
    msb=Wire.read(); lsb=Wire.read();
    wVAL=(msb*256)+lsb;
    fVAL=((175.72 * float(wVAL))/ 65536.0)-46.85; //celcius
    if ((optns & 0x01)==0) {fVAL=(fVAL*1.8)+32.0;} //Fahrenheit
    dtoa(dataT,fVAL,1);
  }
  return ret;
}

//***********************
char *get_H_Si7020(char *dataH) { char *ret=dataH;
  byte msb; byte lsb; word wVAL; int iVAL;
  strlcpy_P(dataH,FAIL1,10);//"---"); //prep for fail
  TWCR=1; Wire.beginTransmission(0x40);  Wire.write(0xF5);
  Wire.endTransmission();   delay(20);
  Wire.requestFrom(0x40, 2);
  if(Wire.available() >= 2) {
    msb=Wire.read(); lsb=Wire.read(); wVAL=(msb*256)+lsb;
    iVAL=int(float((125.0 * float(wVAL))/ 65536.0)-6.0); //RH
    if (iVAL>100) { iVAL=100; } else if (iVAL<0) { iVAL=0; }
    itoa(int(iVAL),dataH,10);
  }
  return ret;
}

//*****************************************
char *whichMAX(char *sfx, byte optns) { char *ret=sfx;
  int rslt; char wMAX[10];
 //6675 is 16-bit reg, 31855 has 32 bit reg (14Tcpl+12Tref)
 //so... get 32 bits - look at top 16 and bottom 16.
  Wire.begin(); Wire.setWireTimeout(2000, true);
  if (digitalRead(pinBOOST) == 0) { boost_ON(); }
  digitalWrite(SPI_CS,HIGH);
  delay(200);
  unsigned long REG32= spi_IN32(SPI_CS,SPI_SI,SPI_CLK,true); //CS,SI,CLK, stop?
  unsigned int top16=(REG32 >>16);
  unsigned int btm16=(REG32 & 0xFFFF);
  //Serial.print(F("top16="));Serial.println(top16,HEX); 
  //Serial.print(F("btm16="));Serial.println(btm16,HEX); Serial.flush();
  if (top16==btm16){ strcpy(sfx,"6675"); } //one 16 bit reg pulled in twice?
  else {strcpy(sfx,"31855"); }
  return ret;   
}

//************************************************************
char *get_MAX1of2(char *dataTCpl, char *sName, byte optns) { char *ret=dataTCpl;
  byte pos=myChrPos(sName,'|')+1; //is -1 if not found so +1 is 0
  //Serial.print(F("pos: "));Serial.println(pos);Serial.flush();
  if (pos>0) {
    char sfx[10]={0}; mySubStr(sfx,sName,pos,(strlen(sName)-(pos)));
//Serial.print(F("sfx: "));Serial.println(sfx);Serial.flush();
    if (strcmp(sfx,"6675")==0) { get_MAX6675(dataTCpl, optns); }
    else if (strcmp(sfx,"31855")==0) { get_MAX31855(dataTCpl, optns); }
  }
  //Serial.print(F("dataTC: "));Serial.println(dataTC);Serial.flush();
  return ret;
}

//************************************************************
char *get_MAX6675(char *dataMAX,byte optns) { char *ret=dataMAX;
  //SPI_SI=4; SPI_CS=5; SPI_CLK=6;  
  //MAX6675 has 16 bits of temp data
  if (digitalRead(pinBOOST) == 0) { boost_ON(); }
  digitalWrite(SPI_CS,HIGH);
  delay(300);
  word REG16= spi_IN16(SPI_CS,SPI_SI,SPI_CLK,true);//CS,SI,CLK, stop?
  byte Tsign=bitRead(REG16,15); //dummy value =0
  byte Fault=bitRead(REG16,0);
  REG16=(REG16>>3)& 0x0FFF;
  float fVAL=float(REG16)/4.0;
  if ((optns & 0x01)==0) {fVAL=(fVAL*1.8)+32.0;} //Fahrenheit
  dtoa(dataMAX,fVAL,2);
  return ret;
}

//************************************************************
char *get_MAX31855(char *dataMAX,byte optns) { char *ret=dataMAX;
  //SPI_SI=4; SPI_CS=5; SPI_CLK=6;  
  //MAX31855 has 32 bits of data, 31:18 t-couple temp, 15:4 junction ref temp
  if (digitalRead(pinBOOST) == 0) { boost_ON(); }
  digitalWrite(SPI_CS,HIGH);
  delay(300);
  unsigned int REG16= spi_IN16(SPI_CS,SPI_SI,SPI_CLK,true);//CS,SI,CLK, stop?
  byte Tsign=bitRead(REG16,15); //these 16 are actually 31:16 of 31:0
  byte Fault=bitRead(REG16,0);
  REG16=(REG16>>2)& 0x3FFF;
  if (Tsign==1){REG16=~REG16 & 0x3FFF;}
  float fVAL=float(REG16)/4.0;
  if (Tsign==1){fVAL=fVAL * -1.0;}
  if ((optns & 0x01)==0) {fVAL=(fVAL*1.8)+32.0;} //Fahrenheit
  dtoa(dataMAX,fVAL,2);
  return ret;
}

//*****************************************
byte get_PIN(byte pn,int sn) {  //pin number, sample number
  byte acc=0; byte mid=sn/2;
  for (int i=0;i<sn;i++) {acc=acc+digitalRead(pn); delay(1);}
  if (acc>mid) {return 1;} else {return 0;}
}

//*****************************************
byte get_PIN_DB(byte pn,word sn) {  //De Bounce, Pin Number, Sample Number
  byte pinNow=get_PIN(pn,10); //10mS per
  byte pinLast=pinNow;  word StateCtr=0;
  while (StateCtr<sn) { pinNow=get_PIN(pn,10);
    if (pinNow==pinLast) {StateCtr++; } else {StateCtr=0;}
    pinLast=pinNow; }
  return pinNow;
}

//*****************************************
float get_MilliVolts(byte anaPIN) {
  byte VBS = digitalRead(pinBOOST); if (VBS == 0) { digitalWrite(pinBOOST, HIGH); delay(5); }
  float fV =get_Average(anaPIN, 5); fV = (fV * mV_bit);
  if (VBS == 0) { digitalWrite(pinBOOST, LOW); }
  return fV;
}

//*****************************************
float get_Average (byte pinANA, unsigned int SampNum) {
  long Accum;
  Accum = analogRead(pinANA); Accum = 0;
  for (int x = 0; x < SampNum; x++) { Accum = Accum + analogRead(pinANA); }
  float fAvg = float(float(Accum) / float(SampNum));
  return fAvg;
}

//************************************************************
unsigned int spi_IN16(byte pinCS, byte pinSI, byte pinCLK, bool spiEND){
  digitalWrite(pinCS,LOW); delayMicroseconds(1);
  unsigned int msw=0;
  for (byte ck=16;ck>0;ck--) {
  if (digitalRead(pinSI)==1) {bitSet(msw,ck-1);}
  digitalWrite(pinCLK,HIGH);delayMicroseconds(1);
  digitalWrite(pinCLK,LOW);delayMicroseconds(1);
  }
  if (spiEND==true){digitalWrite(pinCS,HIGH);} 
  return msw; 
}

//************************************************************
unsigned long spi_IN32(byte pinCS, byte pinSI, byte pinCLK, bool spiEND){
  digitalWrite(pinCS,LOW); delayMicroseconds(1);
  unsigned long msw=0;
  for (byte ck=32;ck>0;ck--) {
  if (digitalRead(pinSI)==1) {bitSet(msw,ck-1);}
  digitalWrite(pinCLK,HIGH);delayMicroseconds(1);
  digitalWrite(pinCLK,LOW);delayMicroseconds(1);
  }
  if (spiEND==true){digitalWrite(pinCS,HIGH);} 
  return msw; 
}

//****************************************************************
void init_Si7020() { //Serial.print(F("init_Si7020 enter"));
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(500);}
  byte ureg=Si7020_UREG_get();
  ureg=(ureg & 0x7E)| 0x01;
  Si7020_UREG_set(ureg);
  digitalWrite(A4,LOW); digitalWrite(A5,LOW);delay(10); 
  //Serial.print(F(", init_Si7020 exit with ureg="));Serial.println( ureg,HEX);
}
//**********
byte Si7020_UREG_get(){  byte ureg;
  Wire.beginTransmission(0x40);  Wire.write(0xE7);  Wire.endTransmission();
  Wire.requestFrom(0x40, 1);  ureg=Wire.read();  return ureg;
}
//**********
void Si7020_UREG_set(byte val){ Wire.beginTransmission(0x40);
  Wire.write(0xE6); Wire.write(val); Wire.endTransmission();
}

//****************************************************************
void init_E931() { //Elmos motion IC
  byte reg_H=B00100000; //24-17 (8 bits), 64=1/4 max  [High Byte]
  byte reg_M=B10000000; //1111 (15*.5 sec.)           [Middle Byte]
  reg_M=reg_M|B00000000; //11 -> 3
  reg_M=reg_M|B00000000; //01 -> 1*4+4=8
  byte reg_L=B10000000; //1 -> yes, enable            [Low Byte]

  for (byte b=8;b>0;b--) { //1+98+1=100uS per bit
      digitalWrite(A0, HIGH); delayMicroseconds(1); //init read of bit
      if (bitRead(reg_H,b-1)==0) {digitalWrite(A0, LOW); }
      delayMicroseconds(98); //give it time to suck it in
      digitalWrite(A0, LOW);
      delayMicroseconds(1); //just to be nice
   }

  for (byte b=8;b>0;b--) {
    digitalWrite(A0, HIGH); delayMicroseconds(1); //init read of bit
    if (bitRead(reg_M,b-1)==0) {digitalWrite(A0, LOW); }
    delayMicroseconds(98); //give it time to suck it in
    digitalWrite(A0, LOW);
    delayMicroseconds(1); //just to be nice
  }

  for (byte b=8;b>0;b--) {
    digitalWrite(A0, HIGH); delayMicroseconds(1); //init read of bit
    if (bitRead(reg_L,b-1)==0) {digitalWrite(A0, LOW); }
    delayMicroseconds(98); //give it time to suck it in
    digitalWrite(A0, LOW);
    delayMicroseconds(1); //just to be nice
  }
  //one more 0 makes 25 bits...
    digitalWrite(A0, HIGH); delayMicroseconds(1);
    digitalWrite(A0, LOW); delayMicroseconds(98); //give it time to suck it in
    delay(1); //milliSec - latch it in
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
void key_TXID_SEND(char *txid, char* keyTEMP) {
  char keyML[20]; char idKeyML[30];
  strlcpy_P(keyML,KEY0,20); //"thisisamagiclime" from PROGMEM
  strcpy(idKeyML,"!"); strcat(idKeyML,txid); strcat(idKeyML,"!"); strcat(idKeyML,keyTEMP);
  msg_SEND(idKeyML,keyML,2);
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
char *id_GET(char *idOUT, int sbNum) { char *ret=idOUT; sbNum++;
  word idLoc=(EE_ID-(sbNum*6)); bool badID=false;
  for (byte i=0;i<6;i++) { idOUT[i]=EEPROM.read(idLoc-i);
    if (((idOUT[i]<'1')||(idOUT[i]>'Z'))|| ((idOUT[i]>'9')&&(idOUT[i]<'A'))) {
      id_NEW(idOUT,sbNum);
      
      return ret;
    }
  }
  idOUT[6]=0;
  return ret;
}

//*****************************************
void id_NEW(char *idNEW, int sbNum) {char *ret=idNEW;
  word idLoc=(EE_ID-(sbNum*6));
  randomSeed(analogRead(1));
  for (byte i=0;i<6;i++) {
    idNEW[i]=pgm_read_byte_near(idChar+random(0,31)); //from PROGMEM
     EEPROM.write((idLoc-i) ,idNEW[i]);
  }
  idNEW[6]=0;
  return ret;
}

//*****************************************
int get_SBNum() {// -1 is 'no board', 0 is grounded, 22 is tied high
  int sbn; //Sensor Board Number
  byte VBS=digitalRead(pinBOOST);
  if (VBS==0) {digitalWrite(pinBOOST, HIGH); delay(10);}
  int pinRead1=analogRead(pinSBID);//A6 is ADC only - can't yank it.
  int pinRead2;
  int pinACC=0;
  int pinAVG=pinRead1;
  for (byte i=0;i<10;i++) { delay(20);
    pinRead2=analogRead(pinSBID);
    pinACC=pinACC+abs(pinRead1- pinRead2);
    pinRead1=pinRead2;
    pinAVG=int((pinAVG+pinRead2)/2);
  }
  if (pinACC>10) { sbn=-1; }
  else if (pinAVG<3) { sbn=0; }
  else if (pinAVG>1020) { sbn=22; }
  else { sbn=byte((int(pinAVG/51)+1) );}
  if (VBS==0) {digitalWrite(pinBOOST, LOW); delay(5); }
  return sbn;
}  

//*****************************************
void boost_ON() {
  if (digitalRead(pinBOOST) == 0) { 
    pinMode(pinMOSI, OUTPUT);
    pinMode(pinSCK, OUTPUT);
    digitalWrite(pinRF95_CS,HIGH);
    digitalWrite(pinBOOST, HIGH); delay(50); }
}

//*****************************************
void boost_OFF() {
  int dlyRecover=int(20-(int(txBV)*10))*50; //50mS per .1V less than 2.0
  if (dlyRecover>0) {delay(dlyRecover);}
  digitalWrite(pinRF95_CS,LOW);
  pinMode(pinMOSI, INPUT);digitalWrite(pinMOSI,LOW);
  pinMode(pinSCK, INPUT);digitalWrite(pinSCK,LOW);
  digitalWrite(SPI_CS,LOW); //is sometimes left high to end cycle
  digitalWrite(SPI_CLK,LOW);
  digitalWrite(pinLED_TX, LOW);
  digitalWrite(pinLED_BOOT, LOW); 
  digitalWrite(pinBOOST, LOW);
  RF95_UP=false;
}

//*****************************************
void ledBOTTOM_OnOffCnt(int msON, int msOFF,byte count) { 
  delay(500); //can't be an ON if not OFF.
  for (byte i=0;i<count;i++) {
    digitalWrite(pinLED_BOOT, HIGH); delay(msON);
    digitalWrite(pinLED_BOOT, LOW); delay(msOFF);
  }
}

//*****************************************
float get_BatteryVoltage() {
  byte VBS = digitalRead(pinBOOST); if (VBS == 0) { boost_ON(); delay(50); }
  float fBV = get_Average(pinBV, 5); fBV = (fBV * mV_bit) / 1000.0;
  if (VBS == 0) { boost_OFF(); }
  return fBV;
}

//*****************************************
void trigger_RESET(int sbn){
  //Serial.println(F("trigger_RESET"));Serial.flush();
  switch (sbn) { //some sensors need resetting after activation, like E931
    case 5 : {pinMode(pinEVENT, OUTPUT);digitalWrite(pinEVENT,LOW);delay(1);pinMode(pinEVENT, INPUT);} break;
    case 21 : {pinMode(pinEVENT, OUTPUT);digitalWrite(pinEVENT,LOW);delay(1);pinMode(pinEVENT, INPUT);} break;
    case 0: {digitalWrite(SPI_CS,LOW);digitalWrite(SPI_CLK,LOW);}
  }
  switch (DATA_TYPE) { //={"BEACON=0, EVENT_LOW, EVENT_CHNG, EVENT_RISE, EVENT_FALL, ANALOG, DIGITAL_SPI, DIGITAL_I2C"};
    case EVENT_LOW :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, LOW); } break;
    case EVENT_CHNG :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, CHANGE); } break;
    case EVENT_RISE :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, RISING); } break;
    case EVENT_FALL :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, FALLING); } break;
  }
}

//*******//Interrupt on D3 (interrupt #1) ******
void IRPT_D3() {  
  if ((flgKEY_GOOD==true) && (HoldOffCtr==0)){
    wakeWHY=1;}
  wd_COUNTER=0;
} 

//*****************************************
ISR(WDT_vect) { //in avr library
  wd_COUNTER++;  
  if (wd_COUNTER==wd_TIMER) {
    if (flgKEY_GOOD==true) {
      if (HrtBtON==true) {wakeWHY=2;} else {wakeWHY=1;};
    }
    wd_COUNTER=0;
  }
  if (HoldOffCtr>0){ HoldOffCtr--;
    if (HoldOffCtr==0) {flgHoldOff=false;}
  }
}

//*****************************************
void wdt_RESET() { 
  cli(); 
  wdt_reset();
  WDTCSR |= B00011000;  // Change enable and WDE
  WDTCSR = B01100001;   // 8 seconds prescaler
  sei(); 
}

//*****************************************
void systemSleep() {
  if (digitalRead(pinBOOST)==1){boost_OFF();}
  cli();   cbi(ADCSRA, ADEN);   sbi(ACSR, ACD);
  sleep_enable();  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sei(); sleep_mode();
  //Z-Z-Z-Z-Z-Z-Z-Z-Z-Z-Z until an interrupt, then...
  sleep_disable(); 
  sbi(ADCSRA, ADEN);
}

//*****************************************
bool longPress() { bool ret=false;
  //pinMode(pinBOOT_SW, OUTPUT); digitalWrite(pinBOOT_SW, HIGH); 
  //delay(10);
  pinMode(pinBOOT_SW, INPUT); digitalWrite(pinBOOT_SW, LOW); 
  byte ctr=0;
  if (digitalRead(pinBOOT_SW)==1){delay(200);}
  while ((ctr<20) && (digitalRead(pinBOOT_SW)==0)) {
    ctr++; delay(50); }
  if (ctr==20) { ret=true; }
  Serial.print(F("longPress="));Serial.println(ctr);Serial.flush();
  //digitalWrite(pinBOOT_SW, HIGH);
  return ret;
}

//*****************************************
void EE_ERASE_all() {
  Serial.print(F("EE_ERASE_all "));Serial.flush();
  for (word i=0;i<1024;i++) { EEPROM.write(i,255); //FF's
    if (i % 205 == 0) { Serial.print(F(". "));
      digitalWrite(pinLED_BOOT, HIGH);
      delay(20);
      digitalWrite(pinLED_BOOT, LOW);
    }
  } 
  flgEE_ERASED=true;
  Serial.println(F(" Done"));Serial.flush();
}

//*****************************************
void EE_ERASE_id(int sbn) { sbn++;
  Serial.print(F("EE_ERASE_id...#"));Serial.print(sbn);Serial.flush();
  word idLoc=(EE_ID-((sbn)*6));
  for (byte i=0;i<6;i++) { EEPROM.write(idLoc-i,255); }
  Serial.println(F(" ...Done#"));Serial.flush();
}

 //*****************************************
void EE_ERASE_key() { Serial.print(F("EE_ERASE_key"));Serial.flush();
  for (byte i=0;i<16;i++) { EEPROM.write(EE_KEY-i,255); }
  Serial.println(F(" ...Done#"));Serial.flush();
}

//*****************************************
//Long TO Base96, 3 char. LSB first
char *L2B96_3(char *Bout, long count) { char *ret=Bout;
  Bout[3]=0; long n;
  n=long(count/9216); //using printable ascii 32->127 (96)
  if (n>0) {Bout[2]=char(n+32); count=count-(n*9216); }
  else {Bout[2]=0;} //c-string terminate
  n=long(count/96);
  if (n>0) {Bout[1]=char(n+32); count=count-(96*n); }
  else {Bout[1]=0;} //c-string terminate
  Bout[0]=char(count+32);
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
char* dtoa(char *cMJA, double dN, int iP) {char *ret = cMJA;
  // destination, float-double value,  precision (4 is .xxxx)
  long lP=1; byte bW=iP;  while (bW>0) { lP=lP*10;  bW--;  }
  long lL = long(dN); double dD=(dN-double(lL))* double(lP);
  if (dN>=0) { dD=(dD + 0.5);  } else { dD=(dD-0.5); }
  long lR=abs(long(dD));  lL=abs(lL);
  if (lR==lP) { lL=lL+1;  lR=0;  }
  if ((dN<0) & ((lR+lL)>0)) { *cMJA++ = '-';  }
  ltoa(lL, cMJA, 10);
  if (iP>0) { while (*cMJA != '\0') { cMJA++; } *cMJA++ = '.'; lP=10;
  while (iP>1) { if (lR< lP) { *cMJA='0'; cMJA++; } lP=lP*10;  iP--; }
  ltoa(lR, cMJA, 10); }  return ret; 
}
  
