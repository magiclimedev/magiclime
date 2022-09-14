
//*****************************************
void init_SETUP(){
  debugON=0;
  TX_ID.reserve(8);
  TX_KEY.reserve(34);
  sSTR8.reserve(8);
  sSTR18.reserve(18);
  sSTR34.reserve(34); //key_GET_PUBLIC only
  sMSG.reserve(64); //packet_SEND, key_GET_PUBLIC
  sRET.reserve(64); //msg_GET, key_GET_PUBLIC, 
  
  analogReference(EXTERNAL); //3.0V vref.

  Serial.begin(57600);
  Serial.println(F("sensor"));
  //freeMemory();
  for (byte i=4;i<=13;i++) {pinMode(i, OUTPUT);digitalWrite(i, LOW);}
  pinMode(pinRF95_INT, INPUT);
  pinMode(pinRF95_CS, OUTPUT);
  pinMode(pinBOOST, OUTPUT); digitalWrite(pinBOOST, LOW);
  pinMode(pinLED, OUTPUT);   digitalWrite(pinLED, LOW);
  pinMode(pinPAIR_SW, INPUT); digitalWrite(pinPAIR_SW, HIGH); //pull-up on
  
  //key_EE_ERASE(); id_EE_ERASE(SBN);
  boost_ON();
  digitalWrite(pinLED, HIGH);
  delay(200);
  param0_GET(); 
  digitalWrite(pinLED, LOW);
  
   if (SBN==255) {SBN=get_SBNum();}
  Serial.print(F("SBN: "));Serial.println(SBN);Serial.flush();
  id_MAKE(SBN);
  TX_ID=id_GET(SBN);
  Serial.print(F("TX_ID: "));Serial.println(TX_ID);Serial.flush();
  delay(1000); //keeps green led on for a sec before checking pair pin
  
  if (pinPAIR_SW==LOW) { //long press means - remove key/disassociate from rx.
    TX_KEY=key_NEW();
    key_EE_SET(TX_KEY);
    led_PAIR_BLINK(5,50,50);
      if (debugON>0) {Serial.println(F("TX_KEY randomized... "));Serial.flush();}
  } 
  else {
    if (init_RF95()==true) {
      TX_KEY=key_GET_PUBLIC(TX_ID); 
      if (TX_KEY!="") {
        if (key_VALIDATE(TX_KEY)==false){TX_KEY="thisisathingbits"; }
        key_EE_SET(TX_KEY);
      }
    }
    TX_KEY=key_EE_GET();
  }

    if (debugON>0) {Serial.print(F("TX_KEY: "));Serial.println(TX_KEY);Serial.flush();}
    
  init_SENSORS(SBN);

  dBV = get_BatteryVoltage();
 
//***********************************************
  //and now params? just look/expect or, yes...  ask for it.
  // RX expects INFO: IDxxxx:param0 as a request. 
  // RX responds with  ididid:ml:p:s 
    sMSG="INFO: "; sMSG+= TX_ID; sMSG+= ":param0";
    msg_SEND(sMSG, TX_KEY,10); //String &msgIN, String &key, int txPWR)
    // and now look for ... 500mSec?
    byte tryCtr=50; 
  if (debugON>0) {Serial.println(F("looking for param: "));Serial.flush();}
    while (tryCtr>0) { delay(20); tryCtr--;
      if (rf95.available()==true) {
        uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);
        if (rf95.recv(buf, &len)) {// print_HEX(buf,len);
          param0_SET(msg_GET2(buf,len,TX_KEY),TX_ID); 
          param0_GET();
          tryCtr=0;
        }
      }
    }
    
//***********************************************
  //sMSG="INFO:"; sMSG+=TX_ID; 
  //sMSG+=",D:"; sMSG+=String(((float(txINTERVAL)*8.0)/60.0),1);
  //sMSG+=",HB:"; sMSG+=String(((float(txHRTBEAT)*8.0)/60.0),1); 
  //sMSG+=",PWR:";  sMSG+=String((txPWR));
  sMSG="{\"source\":\"tx\",\"info\":\"ver=sensor\"}";
  msg_SEND(sMSG, TX_KEY,10);
  delay(100);
  sMSG=get_DATA(SBN,1);
  packet_SEND(SBN,sMSG,0); // does boost_OFF();
     //watchdog timer - 8 sec
  cli(); wdt_reset(); WDTCSR |= B00011000; WDTCSR = B01100001; sei(); //watchdog timer - 8 sec
  //sleep and 'power down' mode bits
  cbi( SMCR, SE ); cbi( SMCR, SM0 ); sbi( SMCR, SM1 ); cbi( SMCR, SM2 );  
 
} //* END OF init_SETUP ************************
//*****************************************

//*****************************************
void param0_SET(byte *buf, String &sID) {//ididid:d:h:p:s
    if (debugON>0) {Serial.print(F("param_SET... "));print_HEX(buf,13);}
  byte pp; for (pp=0;pp<6;pp++) {if (sID[pp]!= buf[pp]) {break;} }
  if (pp==6) { 
    if ((buf[6]==':') && (buf[8]==':')&&(buf[10]==':')&&(buf[12]==':')) {
      EEPROM.write(EE_INTERVAL,buf[7]);
    if (debugON>0) {Serial.print(F(" INT:"));Serial.print(buf[7],HEX);} //sec./16
      EEPROM.write(EE_HRTBEAT,buf[9]);
    if (debugON>0) {Serial.print(F(", HB:")); Serial.print(buf[9],HEX); }//sec./64
      EEPROM.write(EE_POWER,buf[11]);
    if (debugON>0) {Serial.print(F(", PWR:")); Serial.print(buf[11],HEX); }//1-10
      EEPROM.write(EE_SYSBYTE,buf[13]);
    if (debugON>0) {Serial.print(F(", SYS:")); Serial.println(buf[13],HEX);} // ??
      led_GREEN_BLINK(3,3,3);
    }
  }
}


//*****************************************
void param0_GET() { //and set to defaults if EEPROM erased.
    if (debugON>0) {Serial.println(F("param0_GET..."));Serial.flush();}
  if (EEPROM.read(EE_POWER)>10){ //the one that should be 1-10
    if (debugON>0) {Serial.println(F("writing default params..."));}   
    EEPROM.write(EE_INTERVAL,INTERVAL_DATA); //*16=64 sec. (might be 255)
    EEPROM.write(EE_HRTBEAT,INTERVAL_HEARTBEAT); //*64 about an hour. (might be 255)
    EEPROM.write(EE_POWER,1); //low power default value
  }
  txINTERVAL=EEPROM.read(EE_INTERVAL)*2; // 16 sec to 8 sec per wdt.
    if (debugON>0) {Serial.print(F("txINTERVAL:"));Serial.print(txINTERVAL);}
  txHRTBEAT=EEPROM.read(EE_HRTBEAT)*8; //from 64 sec to 8 sec per wdt.
    if (debugON>0) {Serial.print(F(", txHRTBEAT:"));Serial.print(txHRTBEAT);}
  txPWR=EEPROM.read(EE_POWER); //1-10
    if (debugON>0) {Serial.print(F(", txPWR:"));Serial.print(txPWR);}
  sysBYTE=EEPROM.read(EE_SYSBYTE); //
    if (debugON>0) {Serial.print(F(", sysBYTE:"));Serial.println(sysBYTE,HEX); } 
}

//*****************************************
void init_SENSORS(byte sbn) { DATA_TYPE = BEACON; //preset default
  switch (sbn) { //init sensors as needed
    case 1: { DATA_TYPE = EVENT_RISE;                                  //button
      pinMode(pinEVENT, INPUT);  } break;
                
    case 2: { DATA_TYPE = EVENT_CHNG; //(int.30K off, ext.pullup =1M  //tilt
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); } break;
      
    case 3: { DATA_TYPE = EVENT_CHNG; //(int.30K off, ext.pullup =1M  //reed
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); } break;
            
    case 4: { DATA_TYPE = EVENT_FALL;                                    //shake
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, HIGH); } break;
                  
    case 5: { DATA_TYPE = EVENT_RISE;                                     //motion
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); //no pullup
      pinMode(pinSWITCH, OUTPUT);  digitalWrite(pinSWITCH, LOW); //ser.prog.
      init_E931();} break;
       
    case 6: { DATA_TYPE = EVENT_FALL;                                        //knock
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, HIGH); } break; 
 
    case 10: { DATA_TYPE = ANALOG; CAL_VAL=analogRead(pinTrimPot); }break;   //tmp36 
    case 11: { DATA_TYPE = ANALOG; MAX1=0; MIN1=1023;
               pinMode(4, OUTPUT); digitalWrite(4, LOW); } break;            //photocell              
    // get max,min from EEprom ? 
    case 12: { DATA_TYPE = DIGITAL_I2C; } break;                            //Si7020 Temp-RH sensor
  }
  
  init_TYPE(DATA_TYPE);// enable interrupts, etc.
  
  if (HrtBtON==true) { txTIMER=txHRTBEAT; }
  else { txTIMER=txINTERVAL; }
  
    if (debugON>0) {Serial.print(F("txTIMER: "));Serial.println(txTIMER);Serial.flush();}
}

   
//*****************************************
void init_TYPE(TYPE sbt){ //
   //="BEACON", "EVENT_LOW","EVENT_CHNG","EVENT_RISE",
   //="EVENT_FALL","ANALOG", "DIGITAL_SPI", "DIGITAL_I2C"
  switch (sbt) {
    case BEACON :{ HrtBtON=true; } break;
    case EVENT_LOW :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, LOW);
      HrtBtON=true;  } break;
    case EVENT_CHNG :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, CHANGE);
      HrtBtON=true;  } break;
    case EVENT_RISE :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, RISING);
      HrtBtON=true;  } break;
    case EVENT_FALL :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, FALLING);
      HrtBtON=true;  } break; 
    
    case ANALOG :{ HrtBtON=false; } break;
    case DIGITAL_SPI :{ HrtBtON=false; } break;
    case DIGITAL_I2C :{ HrtBtON=false; } break;
  }
}

//*****************************************
bool init_RF95()  {
    if (debugON>0) {Serial.print(F("init_RF95..."));}
  bool ret=false;
  byte VBS = digitalRead(pinBOOST); if (VBS == 0) { boost_ON(); }
  byte timeout=0;
  while (!rf95.init() && (timeout<20)) { delay(10); timeout++; }
if (debugON>0) {Serial.print(F(" timeout<20: "));Serial.println(timeout);Serial.flush();}
  if (timeout!=20) {
    rf95.setFrequency(RF95_FREQ); delay(10);
    rf95.setTxPower(txPWR*2, false);  delay(10);
    ret=true;
  }
  RF95_UP=ret;
  return ret;
}

 
