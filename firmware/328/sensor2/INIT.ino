
//*****************************************
void init_SETUP(){ 
  //pinPAIR_SW is connected to the reset pin.
  pinMode(pinPAIR_SW, INPUT); digitalWrite(pinPAIR_SW, HIGH); //so, do first
  pinMode(pinLED, OUTPUT);   digitalWrite(pinLED, HIGH); delay(20); digitalWrite(pinLED, LOW);
  pinMode(pinRF95_INT, INPUT);
  pinMode(pinRF95_CS, OUTPUT); digitalWrite(pinRF95_CS, LOW);
  pinMode(pinBOOST, OUTPUT); digitalWrite(pinBOOST, LOW);
  pinMode(pinPAIR_LED, OUTPUT);   digitalWrite(pinLED, LOW);
  analogReference(EXTERNAL); //3.0V vref.
      
  Serial.begin(57600);
  Serial.println(F("sensor2"));

  //EE_ERASE_all();
  //EE_ERASE_key();
  //EE_ERASE_id(SBN); //assuming you set SBN to something '22 or less'.

  boost_ON();
  digitalWrite(pinLED, HIGH);
  delay(200);
  prm0_EE_GET(); //from eeprom
  digitalWrite(pinLED, LOW);
  
  if (SBN==255) {SBN=get_SBNum();}
  id_MAKEifBAD(SBN); //into eeprom
  id_GET(txID,SBN);  //from eeprom
  Serial.print(F("txID: "));Serial.print(txID);
  Serial.print(F(", SBN: "));Serial.println(SBN);Serial.flush();
  delay(1000); //keeps green led on for a sec before checking pair pin
  txBV = get_BatteryVoltage(); //good time to get this?
    
  if (pinPAIR_SW==LOW) { //long press means - remove key/disassociate from rx.
    key_NEW(rxKEY); //probably could just ="1234567890123456\0"
    key_EE_SET(rxKEY);
    led_PAIR_BLINK(10,50,50);
  }
 
  else {
    if (init_RF95(txPWR)==true) {
      key_REQUEST(rxKEY,txID,keyRSS); //ask for RX's KEY
      if (rxKEY[0]!=0) { //good key returned
        Serial.print(F("rxKEY=")); Serial.println(rxKEY);
        if (key_VALIDATE(rxKEY)==false){strcpy(rxKEY,"thisisamagiclime"); }
        key_EE_SET(rxKEY);
      }
    }
    key_EE_GET(rxKEY);
    delay(100); //it can take the receiver a bit to stash things in eeprom
  }
    
  init_SENSOR(SNM,SBN); //name of sensor returned in SNM

  txBV = get_BatteryVoltage();
 
//***********************************************
  //Serial.println(F("requesting paramters... "));Serial.flush();
  //and now the TX parameters? just look/expect or, yes...  ask for them.
  // RX expects PUR:IDxxxx:PRM0:NAME - that's Parameter Update Request, the TXID and PaRaMeter set #0. 
  // RX responds with  ididid:i:h:p:s 
  prm0_EE_GET(); //first, in case the following fails...
  char msg[40];
  strcpy(msg,"PUR:"); strcat(msg,txID);
  strcat(msg,":PRM0:"); strcat(msg,SNM);
  msg_SEND(msg, rxKEY,txPWR); //String &msgIN, String &key, int txPWR)
  // and now look for ... 500mSec?
  byte timeout=0;
  while (!rf95.available() && timeout<250) { delay(10); timeout++; }
  Serial.print(F("PUR timeout<250): "));Serial.println(timeout);Serial.flush();
  if (timeout<250) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {// print_HEX(buf,len);
      rx_DECODE_0(msg,buf,len,rxKEY);
      if (prm0_PKT2EE(msg,txID)) {  
        prm0_EE_GET(); //from eeprom
        prm0_PAK(); //ack? just info = PAK:ididid:int,hb,pwr,sysbyte
        prm0_OPTIONS(SBN,optBYTE); //something to do in that option byte?
      }
    }
  }
  
//***********************
  get_DATA(txDATA,SBN,1);
  packet_SEND(SBN,txID,txBV,rxKEY,txDATA,1); // does boost_OFF();
//***********************
  
  //sleep and 'power down' mode bits
  cbi( SMCR, SE ); cbi( SMCR, SM0 ); sbi( SMCR, SM1 ); cbi( SMCR, SM2 ); 
  //watchdog timer - 8 sec
  cli(); wdt_reset();
  WDTCSR |= B00011000; WDTCSR = B01100001;
  sei(); //watchdog timer - 8 sec   
  wakeWHY=0;
  txCOUNTER=txTIMER-1;
  //freeMemory();

 Serial.print(F("rxKEY: "));Serial.println(rxKEY);Serial.flush();
} //* END OF init_SETUP ************************
//*****************************************

//*****************************************
bool prm0_PKT2EE(char *buf, char *id) { //ididid:d:h:p:s
  Serial.print(F("prm0_PKT2EE...")); 
  byte i; byte d;
  for (i=0;i<6;i++) {if (id[i]!= buf[i]) {break;} }
  if (i==6) { print_HEX(buf,15);  //all 6 chars match the txid
    if ((buf[6]==':') && (buf[8]==':')&&(buf[10]==':')&&(buf[12]==':')) {
      EEPROM.write(EE_INTERVAL,buf[7]);
      Serial.print(F("buf7:"));Serial.println(buf[7],HEX);Serial.flush();
      EEPROM.write(EE_HRTBEAT,buf[9]);
      Serial.print(F("buf9:"));Serial.println(buf[9],HEX);Serial.flush();
      EEPROM.write(EE_POWER,buf[11]);
      Serial.print(F("buf11:"));Serial.println(buf[11],HEX);Serial.flush();
      EEPROM.write(EE_OPTBYTE,buf[13]);
      Serial.print(F("buf13:"));Serial.println(buf[13],HEX);Serial.flush();
      led_GREEN_BLINK(3,10,10);
      return true;
    }
  }
  return false;  
}

//*****************************************
void prm0_PAK() {
  //Serial.print(F("...prm0_PAK, txi="));Serial.print(txINTERVAL);
  //Serial.print(F("  txhb="));Serial.println(txHRTBEAT);
  char n2a[10]; // for Number TO Ascii things
  char msg[32];
  strcpy(msg,"PAK:");
  strcat(msg,txID); 
  strcat(msg,":"); 
  strcat(msg,":"); dtoa(float((float(txINTERVAL)*8.0)/60.0),n2a,1); strcat(msg,n2a);
  strcat(msg,":"); dtoa(float((float(txHRTBEAT)*8.0)/60.0),n2a,1); strcat(msg,n2a);
  strcat(msg,":"); itoa((txPWR),n2a,10); strcat(msg,n2a);
  static const char hex[] = "0123456789ABCDEF";
  byte msnb = byte((optBYTE>>4)& 0x0F); byte lsnb=byte(optBYTE & 0x0F);
  char msn[2]; msn[0]=hex[msnb]; msn[1]=0;
  char lsn[2]; lsn[0]=hex[lsnb]; lsn[1]=0;
  strcat(msg,":"); strcat(msg,msn); strcat(msg,lsn);
  Serial.print(F("pak_SEND:"));Serial.println(msg);Serial.flush();
  msg_SEND(msg,rxKEY,1);
  delay(10);
}

//*****************************************
void prm0_EE_GET() { //and set to defaults if EEPROM erased.
  if (EEPROM.read(EE_POWER)>20){ //the one that should be 2-20
    EEPROM.write(EE_INTERVAL,defaultINTERVAL); //  
    EEPROM.write(EE_HRTBEAT,defaultHEARTBEAT); //  
    EEPROM.write(EE_POWER,2); //low power default value
    EEPROM.write(EE_OPTBYTE,0);
  }
  txINTERVAL=EEPROM.read(EE_INTERVAL)*wdmTXI;    //255*(8*1)sec. * 255 = 2,040/60= 34 min.
  txHRTBEAT=EEPROM.read(EE_HRTBEAT)*wdmHBP;    //255*(8*16)sec = 32,640 sec./60 = 544 min
  txPWR=EEPROM.read(EE_POWER);            //2-20
  optBYTE=EEPROM.read(EE_OPTBYTE);        //
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
void prm0_OPTIONS(int sbn, byte optbyte) { sbn++; //to make -1 = 0
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
      if (eeREAD2(EE_MAX1)==0xFFFF) { eeWRITE2(EE_MAX1,0); } //set to default if not set before
      if (eeREAD2(EE_MIN1)==0xFFFF) { eeWRITE2(EE_MIN1,1023); } //set to default if not set before
      if (bitRead(optBYTE,0)==1) { bitClear(optBYTE,0 );  //don't do it again 
        MAX1=0; eeWRITE2(EE_MAX1,MAX1); EEPROM.write(EE_OPTBYTE,optBYTE); }
      if (bitRead(optBYTE,1)==1) { bitClear(optBYTE,1 );  //don't do it again 
        MIN1=1023; eeWRITE2(EE_MIN1,MIN1); EEPROM.write(EE_OPTBYTE,optBYTE);  } } break;
    case 12: { EEPROM.write(EE_OPTBYTE,optBYTE);} break; //Si7020 Temp-RH sensor
    case 21: { EEPROM.write(EE_OPTBYTE,optBYTE);} break; //motion
  }
}

//*****************************************
char * init_SENSOR(char *snm, int sbn) { char* ret=snm; DATA_TYPE = BEACON; //preset default
  switch (sbn) {  strcpy(SNM,"????"); //init sensors as needed
    case -1: { DATA_TYPE = BEACON; strcpy(snm,"BEACON");} break;        //no sensor board detected
    case 0: { DATA_TYPE = BEACON; strcpy(snm,"SBN0"); } break;        //your pick - sbn pin grounded
    case 1: { DATA_TYPE = EVENT_RISE;   strcpy(snm,"BUTTON"); 
      pinMode(pinEVENT, INPUT);  } break;
    case 2: { DATA_TYPE = EVENT_RISE;  strcpy(snm,"TILT"); strcpy(dataOLD,"NULL");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); } break;
    case 3: { DATA_TYPE = EVENT_CHNG; strcpy(snm,"REED");//(int.30K off, ext.pullup =1M  
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); } break;
    case 4: { DATA_TYPE = EVENT_FALL; strcpy(snm,"SHAKE");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, HIGH); } break;
    case 5: { DATA_TYPE = EVENT_RISE; strcpy(snm,"MOTION");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); //no pullup
      //pinMode(pinSWITCH, OUTPUT);  digitalWrite(pinSWITCH, LOW); //ser.prog.on D4
      pinMode(A0, OUTPUT);  digitalWrite(A0, LOW); //ser.prog.on A0 for ML/Tiny2040
      init_E931();} break;
    case 6: { DATA_TYPE = EVENT_FALL; strcpy(snm,"KNOCK");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, HIGH); } break; 
     case 7: { DATA_TYPE = EVENT_RISE; strcpy(snm,"2BTN");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT,LOW); } break; 
    case 10: { DATA_TYPE = ANALOG; strcpy(snm,"TMP36"); CAL_VAL=analogRead(pinTrimPot); }break;
    case 11: { DATA_TYPE = ANALOG; strcpy(snm,"LIGHT%");
                MAX1=eeREAD2(EE_MAX1); MIN1=eeREAD2(EE_MIN1); // get max,min from EEprom ?
                pinMode(4, OUTPUT); digitalWrite(4, LOW); } break;                        
    case 12: { DATA_TYPE = DIGITAL_I2C; } break; strcpy(snm,"T-RH");
    case 21: { DATA_TYPE = EVENT_RISE; strcpy(snm,"MOT-DOT");
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); //no pullup
      //pinMode(pinSWITCH, OUTPUT);  digitalWrite(pinSWITCH, LOW); //ser.prog.on D4
      pinMode(A0, OUTPUT);  digitalWrite(A0, LOW); //ser.prog.on A0 for ML/Tiny2040
      pinMode(A4,INPUT);digitalWrite(A4, LOW); //D.O.T. pin#1
      pinMode(A5,INPUT);digitalWrite(A5, LOW); //D.O.T. pin#2
      init_E931();} break;
    case 22: { DATA_TYPE = BEACON; strcpy(snm,"SBN22"); } break;   ////your pick - sbn pin tied to Aref
  }

  init_TYPE(DATA_TYPE);// enable interrupts, etc.
  
  if (HrtBtON==true) { txTIMER=txHRTBEAT; }
  else { txTIMER=txINTERVAL; }
}

   
//*****************************************
void init_TYPE(TYPE sbt){ //Serial.print(F("...init_TYPE"));
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
      rf95.setTxPower(txpwr, false);  delay(10); //2-20 valid range
      RF95_UP=true;
    }
  }
  return RF95_UP;
}

 
