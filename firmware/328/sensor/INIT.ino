
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
  Serial.println(F("sensor"));

  //EE_ERASE_all();
  //EE_ERASE_key();
  //EE_ERASE_id(SBN); //assuming you set SBN to something 'less than 20'.

  boost_ON();
  digitalWrite(pinLED, HIGH);
  delay(200);
  param0_GET(); //from eeprom
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
    if (init_RF95()==true) {
      key_REQUEST(rxKEY,txID,keyRSS); //ask for RX's KEY
      if (rxKEY[0]!=0) { //good key returned
        Serial.print(F("rxKEY=")); Serial.println(rxKEY);
        if (key_VALIDATE(rxKEY)==false){strcpy(rxKEY,"thisisamagiclime"); }
        key_EE_SET(rxKEY);
      }
    }
    key_EE_GET(rxKEY);
  }
    
  init_SENSORS(SBN);

  txBV = get_BatteryVoltage();
 
//***********************************************
  //Serial.println(F("requesting paramters... "));Serial.flush();
  //and now the TX parameters? just look/expect or, yes...  ask for them.
  // RX expects PUR:IDxxxx:PRM0 - that's Parameter Update Request, the TXID and PaRaMeter set #0. 
  // RX responds with  ididid:ml:p:s 
    char msg[32]; strcpy(msg,"PUR:"); strcat(msg,txID); strcat(msg,":PRM0");
    msg_SEND(msg, rxKEY,1); //String &msgIN, String &key, int txPWR)
    // and now look for ... 500mSec?
    byte timeout=0;
    while (!rf95.available() && timeout<250) { delay(10); timeout++; }
    //Serial.print(F("timeout<250): "));Serial.println(timeout);Serial.flush();
    if (timeout<250) {
      uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(buf);
      if (rf95.recv(buf, &len)) {// print_HEX(buf,len);
        rx_DECODE_0(msg,buf,len,rxKEY);
        param0_SET(msg,txID);  
        param0_GET(); //from eeprom
        param0_SEND(); //ack? just info = PAK:ididid:int,hb,pwr,sysbyte
      }
    }
//***********************************************
  get_DATA(txDATA,SBN,1);
  packet_SEND(SBN,txID,txBV,rxKEY,txDATA,1); // does boost_OFF();
  //watchdog timer - 8 sec
  cli(); wdt_reset(); WDTCSR |= B00011000; WDTCSR = B01100001; sei(); //watchdog timer - 8 sec
  //sleep and 'power down' mode bits
  cbi( SMCR, SE ); cbi( SMCR, SM0 ); sbi( SMCR, SM1 ); cbi( SMCR, SM2 ); 
   
 sendWHY=0;
 freeMemory();
} //* END OF init_SETUP ************************
//*****************************************

//*****************************************
void param0_SET(byte *buf, char *sID) {//ididid:d:h:p:s
  byte pp; for (pp=0;pp<6;pp++) {if (sID[pp]!= buf[pp]) {break;} }
  if (pp==6) { 
    if ((buf[6]==':') && (buf[8]==':')&&(buf[10]==':')&&(buf[12]==':')) {
      EEPROM.write(EE_INTERVAL,buf[7]);
      EEPROM.write(EE_HRTBEAT,buf[9]);
      EEPROM.write(EE_POWER,buf[11]);
      EEPROM.write(EE_SYSBYTE,buf[13]);
      led_GREEN_BLINK(3,3,3);
    }
  }
}

//*****************************************
void param0_SEND() { //if (debugON>0) {Serial.println(F("...param0_SEND"));Serial.flu
  char n2a[10]; // for Number TO Ascii things
  char msg[32];
  strcpy(msg,"PAK:");
  strcat(msg,txID); 
  strcat(msg,":"); dtoa(((float(txINTERVAL)*8.0)/60.0),n2a,1); strcat(msg,n2a);
  strcat(msg,":"); dtoa(((float(txHRTBEAT)*8.0)/60.0),n2a,1);  strcat(msg,n2a);
  strcat(msg,":"); itoa(txPWR,n2a,10); strcat(msg,n2a);
  static const char hex[] = "0123456789ABCDEF";
  byte msnb = byte((sysBYTE>>4)& 0x0F); byte lsnb=byte(sysBYTE & 0x0F);
  char msn[2]; msn[0]=hex[msnb]; msn[1]=0;
  char lsn[2]; lsn[0]=hex[lsnb]; lsn[1]=0;
  strcat(msg,":"); strcat(msg,msn); strcat(msg,lsn);
  msg_SEND(msg,rxKEY,1);
}

//*****************************************
void param0_GET() { //and set to defaults if EEPROM erased.
  if (EEPROM.read(EE_POWER)>10){ //the one that should be 1-10
    EEPROM.write(EE_INTERVAL,INTERVAL_DATA); //*16=64 sec. (might be 255)
    EEPROM.write(EE_HRTBEAT,INTERVAL_HEARTBEAT); //*64 about an hour. (might be 255)
    EEPROM.write(EE_POWER,1); //low power default value
  }
  txINTERVAL=EEPROM.read(EE_INTERVAL)*2; // 16 sec to 8 sec per wdt.
  txHRTBEAT=EEPROM.read(EE_HRTBEAT)*8; //from 64 sec to 8 sec per wdt.
  txPWR=EEPROM.read(EE_POWER); //1-10
  sysBYTE=EEPROM.read(EE_SYSBYTE); //
}

//*****************************************
void init_SENSORS(int sbn) { DATA_TYPE = BEACON; //preset default
  switch (sbn) { //init sensors as needed
    case 1: { DATA_TYPE = EVENT_RISE;                                  //button
      pinMode(pinEVENT, INPUT);  } break;
    case 2: { DATA_TYPE = EVENT_RISE;  strcpy(dataOLD,"NULL");//tilt
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); } break;
    case 3: { DATA_TYPE = EVENT_CHNG; //(int.30K off, ext.pullup =1M  //reed
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); } break;
    case 4: { DATA_TYPE = EVENT_FALL;                                    //shake
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, HIGH); } break;
    case 5: { DATA_TYPE = EVENT_RISE;                                     //motion
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); //no pullup
      //pinMode(pinSWITCH, OUTPUT);  digitalWrite(pinSWITCH, LOW); //ser.prog.on D4
      pinMode(A0, OUTPUT);  digitalWrite(A0, LOW); //ser.prog.on A0 for ML/Tiny2040
      init_E931();} break;
    case 6: { DATA_TYPE = EVENT_FALL;                                        //knock
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, HIGH); } break; 
     case 7: { DATA_TYPE = EVENT_RISE;                                        //2 button
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT,LOW); } break; 
    case 10: { DATA_TYPE = ANALOG; CAL_VAL=analogRead(pinTrimPot); }break;   //tmp36 
    case 11: { DATA_TYPE = ANALOG; MAX1=0; MIN1=1023; // get max,min from EEprom ?
               pinMode(4, OUTPUT); digitalWrite(4, LOW); } break;            //photocell              
    case 12: { DATA_TYPE = DIGITAL_I2C; } break;                            //Si7020 Temp-RH sensor
    
    case 21: { DATA_TYPE = EVENT_RISE;                                     //motion
      pinMode(pinEVENT, INPUT); digitalWrite(pinEVENT, LOW); //no pullup
      //pinMode(pinSWITCH, OUTPUT);  digitalWrite(pinSWITCH, LOW); //ser.prog.on D4
      pinMode(A0, OUTPUT);  digitalWrite(A0, LOW); //ser.prog.on A0 for ML/Tiny2040
      pinMode(A4,INPUT);digitalWrite(A4, LOW); //D.O.T. pin#1
      pinMode(A5,INPUT);digitalWrite(A5, LOW); //D.O.T. pin#2
      init_E931();} break;
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
  if (RF95_UP==false) { //not already done before?
    if (digitalRead(pinBOOST) == 0) { boost_ON(); }
    byte timeout=0;
    while (!rf95.init() && (timeout<20)) { delay(10); timeout++; }
    if (timeout!=20) {
      rf95.setFrequency(RF95_FREQ); delay(10);
      if (txPWR==0) {txPWR=1;}
      rf95.setTxPower(txPWR*2, false);  delay(10);
      RF95_UP=true;
    }
  }
  return RF95_UP;
}

 
