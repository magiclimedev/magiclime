
//*****************************************
void init_SETUP(){ 
  flgLED_KEY=false;
  //pinBOOT_SW is connected to the reset pin.
  pinMode(pinBOOT_SW, INPUT); digitalWrite(pinBOOT_SW, HIGH); //so, do first
  pinMode(pinLED_TX, OUTPUT); digitalWrite(pinLED_TX, LOW);
  pinMode(pinRF95_INT, INPUT);
  pinMode(pinRF95_CS, OUTPUT); digitalWrite(pinRF95_CS, LOW);
  pinMode(pinBOOST, OUTPUT); digitalWrite(pinBOOST, LOW);
  pinMode(pinLED_BOOT, OUTPUT); digitalWrite(pinLED_BOOT, LOW);
  analogReference(EXTERNAL); //3.0V vref.
      
  Serial.begin(57600);
  Serial.println("");Serial.println(VER);

  boost_ON();
  if (longPress()==true) {EE_ERASE_all(); }
    
  if (SBN==255) {SBN=get_SBNum();}
  Serial.print(F("SBN: "));Serial.print(SBN);Serial.flush();
  id_GET(txID,SBN);
  Serial.print(F(", txID: "));Serial.println(txID);Serial.flush();
  prm0_EE_GET(SBN);
  
  txBV = get_BatteryVoltage();
 
  init_SENSOR(SNM,SBN);
  name_EE_GET(SNM,SBN);
  Serial.print(F("Name:"));Serial.println(SNM);

  if (init_RF95(txPWR)==true) {
    key_REQUEST(rxKEY,txID,keyRSS);
    if (rxKEY[0]!=0) {
      if (key_VALIDATE(rxKEY)==true) { 
        key_EE_SET(rxKEY);
        flgLED_KEY=true;
      }
    }
  }
  
  key_EE_GET(rxKEY);
  flgKEY_GOOD=key_VALIDATE(rxKEY);
  delay(100);
//******************
  if (flgKEY_GOOD==true) {
    char msg[40];
    strcpy(msg,"PUR:0:"); strcat(msg,txID);strcat(msg,":"); strcat(msg,SNM);
    msg_SEND(msg, rxKEY,txPWR);
    rx_LOOK(msg,rxKEY,25);
    if (msg[0]!=0) { prm_PROCESS(msg,txID,SBN); }
    //***********************
    get_DATA(txDATA,SBN,1);
    packet_SEND(SBN,txID,txBV,rxKEY,txDATA,1); 
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
  wakeWHY=0;
  wd_COUNTER=wd_TIMER-1; //for first HeartBeat = 8 sec. 
  Serial.print(F("wd_TIMER: "));Serial.println(wd_TIMER);Serial.flush();
  Serial.print(F("rxKEY: "));Serial.println(rxKEY);Serial.flush();
  ledBOTTOM_OnOffCnt(1000,500,1);
  if (flgKEY_GOOD==false){ledBOTTOM_OnOffCnt(1000,500,1);}  //2nd long flash?
  if (flgLED_KEY==true) { ledBOTTOM_OnOffCnt(200,200,3); } //3 flashes

} //* END OF init_SETUP ************************

//**********************************************************************************
//**********************************************************************************

//*****************************************  
void name_EE_SET(char *snm, int sbn) {  sbn++;
  word addr=EE_NAME-((sbn)*EE_BLKSIZE);
  for (byte b=0;b<10;b++) {
    if ((snm[b]==0) || (snm[b]==0xFF)) { break; }
    else { EEPROM.write(addr-b,snm[b]); }
  }
}

//*****************************************  
char *name_EE_GET(char *snm, int sbn) { sbn++; char *ret=snm;
  word addr=EE_NAME-((sbn)*EE_BLKSIZE);
  if (EEPROM.read(addr)==0xFF) {return ret; }
  else { byte b;
    for (b=0;b<10;b++) {  snm[b]=EEPROM.read(addr-b);
      if ((snm[b]==0) || (snm[b]==0xFF)) { break; }
    }
  snm[b]=0;
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
void prm_PROCESS(char *buf, char *id, int sbn) {
  char cmp[6]; mySubStr(cmp,buf,0,4);//PRM:0:ididid:i:h:p:o
  if (strcmp(cmp,"PRM:")==0) {
    switch (buf[4]) {
      case '0': {  prm0_EE_GET(SBN);
        char tmp[8]; mySubStr(tmp,buf,6,6);
        if (strcmp(tmp,id)==0) {
          if ((buf[3]==':') && (buf[5]==':') && (buf[12]==':') && (buf[14]==':')
          && (buf[16]==':') && (buf[18]==':') ) {
            prm0_EE_SET(buf,sbn);
            prm0_PAKOUT();
            prm_OPTIONS(SBN,optBYTE);
          }//led_TX_BLINK(3,10,10);  }        
        }
      } break;
      case '1': { } break;//wdmTXI and wdmHBI?
    }
  }
}

//*****************************************
void prm0_PAKOUT() {
  Serial.print(F("...prm0_PAK, txi="));Serial.print(wd_INTERVAL);
  Serial.print(F("  txhb="));Serial.println(wd_HEARTBEAT);Serial.flush();
  char n2a[10]; // for Number TO Ascii things
  char msg[32];
  strcpy(msg,"PAK:0:");
  strcat(msg,txID); 
  strcat(msg,":"); dtoa(n2a,(float(wd_INTERVAL)*8.0),1); strcat(msg,n2a); //sec.
  strcat(msg,":"); dtoa(n2a,((float(wd_HEARTBEAT)*8.0)/60.0),1); strcat(msg,n2a); //min.
  strcat(msg,":"); itoa((txPWR),n2a,10); strcat(msg,n2a);
  static const char hex[] = "0123456789ABCDEF";
  byte msnb = byte((optBYTE>>4)& 0x0F); byte lsnb=byte(optBYTE & 0x0F);
  char msn[2]; msn[0]=hex[msnb]; msn[1]=0;
  char lsn[2]; lsn[0]=hex[lsnb]; lsn[1]=0;
  strcat(msg,":"); strcat(msg,msn); strcat(msg,lsn);
  Serial.print(F("pak0_SEND:"));Serial.println(msg);Serial.flush();
  msg_SEND(msg,rxKEY,1);
  delay(10);
}

//*****************************************
void prm0_EE_SET(char *buf,int sbn) { sbn++;
  EEPROM.write((EE_INTERVAL-(sbn*EE_BLKSIZE)),buf[13]);
  wd_INTERVAL=int(byte(buf[13])*wdmTXI);
  EEPROM.write((EE_HRTBEAT-(sbn*EE_BLKSIZE)),buf[15]);
  wd_HEARTBEAT=int(byte(buf[15])*wdmHBI);
  EEPROM.write((EE_POWER-(sbn*EE_BLKSIZE)),buf[17]);
  txPWR=byte(buf[17]);
  EEPROM.write(EE_OPTBYTE,buf[19]);
  optBYTE=byte(buf[19]);
}

//*****************************************
void prm0_EE_GET(int sbn) { sbn++;
  if (flgEE_ERASED==true){
    Serial.println(F("flgEE_ERASED"));
    EEPROM.write((EE_INTERVAL-(sbn*EE_BLKSIZE)),defaultINTERVAL);
    EEPROM.write((EE_HRTBEAT-(sbn*EE_BLKSIZE)),defaultHEARTBEAT);
    EEPROM.write((EE_POWER-(sbn*EE_BLKSIZE)),2);
    EEPROM.write(EE_OPTBYTE,0);
  }
  wd_INTERVAL=EEPROM.read(EE_INTERVAL-(sbn*EE_BLKSIZE))*wdmTXI;
  wd_HEARTBEAT=EEPROM.read(EE_HRTBEAT-(sbn*EE_BLKSIZE))*wdmHBI;
  txPWR=EEPROM.read(EE_POWER-(sbn*EE_BLKSIZE));
  optBYTE=EEPROM.read(EE_OPTBYTE);
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
void prm_OPTIONS(int sbn, byte optbyte) { sbn++;
  switch (sbn) { //init sensors as needed
    case -1: {  EEPROM.write(EE_OPTBYTE,optBYTE);  } break; //beacon
    case 0: {  EEPROM.write(EE_OPTBYTE,optBYTE);  } break; //my_sbn0
    case 1: {  EEPROM.write(EE_OPTBYTE,optBYTE);  } break; //button
    case 2: {  EEPROM.write(EE_OPTBYTE,optBYTE); } break; //tilt
    case 3: {  EEPROM.write(EE_OPTBYTE,optBYTE); } break;//reed
    case 4: {  EEPROM.write(EE_OPTBYTE,optBYTE); } break; //shake} break;
    case 5: {  EEPROM.write(EE_OPTBYTE,optBYTE); } break;//motion
    case 6: {  EEPROM.write(EE_OPTBYTE,optBYTE); } break; //knock
    case 7: {  EEPROM.write(EE_OPTBYTE,optBYTE); } break; //2 button
    case 10: {  EEPROM.write(EE_OPTBYTE,optBYTE); } break;  //tmp36 
    case 11: {  //photocell -adaptive max/min
      if (eeREAD2(EE_MAX1)==0xFFFF) { eeWRITE2(EE_MAX1,0); }
      if (eeREAD2(EE_MIN1)==0xFFFF) { eeWRITE2(EE_MIN1,1023); } 
      if (bitRead(optBYTE,0)==1) { bitClear(optBYTE,0 );
        MAX1=0; eeWRITE2(EE_MAX1,MAX1); EEPROM.write(EE_OPTBYTE,optBYTE); }
      if (bitRead(optBYTE,1)==1) { bitClear(optBYTE,1 );
        MIN1=1023; eeWRITE2(EE_MIN1,MIN1); EEPROM.write(EE_OPTBYTE,optBYTE);  } } break;
    case 12: { EEPROM.write(EE_OPTBYTE,optBYTE);} break; //Si7020 Temp-RH sensor
    case 21: { EEPROM.write(EE_OPTBYTE,optBYTE);} break;
  }
}

//*****************************************
char * init_SENSOR(char *snm, int sbn) { char* ret=snm; DATA_TYPE = BEACON; //preset default
  switch (sbn) {  strcpy(SNM,"????");
    case -1: { DATA_TYPE = BEACON; strcpy(snm,"BEACON");} break;
    case 0: { DATA_TYPE = BEACON; strcpy(snm,"SBN0"); } break;
    case 1: { DATA_TYPE = EVENT_RISE;  strcpy(snm,"BUTTON"); 
      pinMode(pinEVENT, INPUT);  } break;
    case 2: { DATA_TYPE = EVENT_RISE;  strcpy(snm,"TILT"); strcpy(dataOLD,"????");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); } break;
    case 3: { DATA_TYPE = EVENT_CHNG; strcpy(snm,"REED");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); } break;
    case 4: { DATA_TYPE = EVENT_FALL; strcpy(snm,"SHAKE");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, HIGH); } break;
    case 5: { DATA_TYPE = EVENT_RISE; strcpy(snm,"MOTION");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW);
      //pinMode(pinSWITCH, OUTPUT);  digitalWrite(pinSWITCH, LOW);
      pinMode(A0, OUTPUT);  digitalWrite(A0, LOW);
      init_E931();} break;
    case 6: { DATA_TYPE = EVENT_FALL; strcpy(snm,"KNOCK");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, HIGH); } break; 
     case 7: { DATA_TYPE = EVENT_RISE; strcpy(snm,"2BTN");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT,LOW); } break; 
    case 10: { DATA_TYPE = ANALOG; strcpy(snm,"TMP36"); CAL_VAL=analogRead(pinTrimPot); }break;
    case 11: { DATA_TYPE = ANALOG; strcpy(snm,"LIGHT%");
                MAX1=eeREAD2(EE_MAX1); MIN1=eeREAD2(EE_MIN1);
                pinMode(4, OUTPUT); digitalWrite(4, LOW); } break;                        
    case 12: { DATA_TYPE = DIGITAL_I2C; strcpy(snm,"T-RH"); } break; 
    case 21: { DATA_TYPE = EVENT_RISE; strcpy(snm,"MOT-DOT");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW);
      pinMode(A0, OUTPUT);  digitalWrite(A0, LOW);
      pinMode(A4,INPUT);digitalWrite(A4, LOW);
      pinMode(A5,INPUT);digitalWrite(A5, LOW);
      init_E931();} break;
    case 22: { DATA_TYPE = BEACON; strcpy(snm,"SBN22"); } break;
  }

  init_TYPE(DATA_TYPE);// enable interrupts, etc.
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
bool init_RF95(int txpwr)  {
  if (RF95_UP==false) { //not already done before?
    if (digitalRead(pinBOOST) == 0) { boost_ON(); }
    byte timeout=0;
    while (!rf95.init() && (timeout<20)) { delay(10); timeout++; }
    if (timeout!=20) {
      rf95.setFrequency(RF95_FREQ); delay(10);
      if (txpwr<2) {txpwr=2;}
      if (txpwr>20) {txpwr=20;} 
      rf95.setTxPower(txpwr, false);  delay(10);
      RF95_UP=true;
    }
  }
  return RF95_UP;
}

 
