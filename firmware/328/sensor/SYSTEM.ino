
//*****************************************
int get_SBNum() {// -1 is 'no board', 0 is grounded, 22 is tied high
  int sbn; //Sensor Board Number
  byte VBS=digitalRead(pinBOOST);
  if (VBS==0) {digitalWrite(pinBOOST, HIGH); delay(10);}
  int pinRead1=analogRead(pinSBID);//A6 is ADC only - can't yank it.
  int pinRead2;
  int pinACC=0;
  int pinAVG=pinRead1;
  for (byte i=0;i<10;i++) { delay(10);
    pinRead2=analogRead(pinSBID);
    pinACC=pinACC+abs(pinRead1- pinRead2);
    pinRead1=pinRead2;
    pinAVG=int((pinAVG+pinRead2)/2);
  }
//Serial.print(F("pinACC: "));Serial.println(pinACC);Serial.flush();
//Serial.print(F("pinAVG: "));Serial.println(pinAVG);Serial.flush();

  if (pinACC>10) { sbn=-1; } //too much wiggling, no-board/beacon mode
  else if (pinAVG<3) { sbn=0; } //grounded
  else if (pinAVG>1020) { sbn=22; } //tied high
  else { sbn=byte((int(pinAVG/51)+1) );} //about 20 sensors #1 to 21 and 0 and 22
  if (VBS==0) {digitalWrite(pinBOOST, LOW); delay(5); }
  return sbn;
}  
  
//*****************************************
void boost_ON() {
  if (digitalRead(pinBOOST) == 0) { 
    pinMode(pinMOSI, OUTPUT);
    pinMode(pinSCK, OUTPUT);
    digitalWrite(pinRF95_CS,HIGH);
    digitalWrite(pinBOOST, HIGH); delay(20); }
}

//*****************************************
void boost_OFF() {
  digitalWrite(pinRF95_CS,LOW);
  pinMode(pinMOSI, INPUT);digitalWrite(pinMOSI,LOW);
  pinMode(pinSCK, INPUT);digitalWrite(pinSCK,LOW);
  digitalWrite(pinLED_TX, LOW);
  digitalWrite(pinLED_BOOT, LOW); 
  digitalWrite(pinBOOST, LOW);
  RF95_UP=false;
}

//*****************************************
/*void led_TX_BLINK(byte count,byte bON,byte bOFF) { //dur,rate is 10mS per
  byte pStat=digitalRead(pinLED_TX);
  if (pStat==1) { digitalWrite(pinLED_TX, LOW); delay(200);}
  for (byte i=0;i<count;i++) {
    word onDly=bON*10; word offDly=bOFF*10;
    digitalWrite(pinLED_TX, HIGH); delay(onDly);
    digitalWrite(pinLED_TX, LOW); delay(offDly);
  }
  if (pStat==1) {digitalWrite(pinLED_TX, HIGH);}
}
*/
//*****************************************
void led_BLINK_BOOT(byte count,byte bON,byte bOFF) { //dur,rate is 10mS per
  digitalWrite(pinLED_BOOT, LOW); delay(200);
  for (byte i=0;i<count;i++) {
    word onDly=bON*10; word offDly=bOFF*10;
    digitalWrite(pinLED_BOOT, HIGH); delay(onDly);
    digitalWrite(pinLED_BOOT, LOW); delay(offDly);
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
void trigger_RESET(int sbn){ //global DATA_TYPE req,
  switch (sbn) { //some sensors need resetting after activation, like E931
    case 5 : {pinMode(pinEVENT, OUTPUT);digitalWrite(pinEVENT,LOW);delay(1);pinMode(pinEVENT, INPUT);} break;
    case 21 : {pinMode(pinEVENT, OUTPUT);digitalWrite(pinEVENT,LOW);delay(1);pinMode(pinEVENT, INPUT);} break;
  }
  switch (DATA_TYPE) { //={"BEACON", "E_LOW","E_CHANGE","E_RISING","E_FALLING","DATA_ANA", "DATA_SPI", "DATA_I2C"};
    case EVENT_LOW :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, LOW); } break;
    case EVENT_CHNG :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, CHANGE); } break;
    case EVENT_RISE :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, RISING); } break;
    case EVENT_FALL :{ attachInterrupt(digitalPinToInterrupt(pinEVENT), IRPT_D3, FALLING); } break;
  }
}

//*******//Interrupt on D3 (interrupt #1) ******
void IRPT_D3() {  
  wakeWHY=1; //do the data for this sensor
  txCOUNTER=txTIMER; //reset heartbeat timer 
} 

//*****************************************
ISR(WDT_vect) { //in avr library
  txCOUNTER++;  
  if (txCOUNTER==txTIMER) {
    if (HrtBtON==true) {wakeWHY=2;} else {wakeWHY=1;};
    txCOUNTER=0;
  }
}

//*****************************************
void wakeStuff(){if (digitalRead(pinBOOST)==0){boost_ON();}}//SPI.begin();}

//*****************************************
void sleepStuff(){boost_OFF(); SPI.end();}

//*****************************************
void systemSleep() {
//Serial.print(F("Z"));Serial.flush();
  if (digitalRead(pinBOOST)==1){boost_OFF();}
  cli();   cbi(ADCSRA, ADEN);   sbi(ACSR, ACD);
  sleep_enable();  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sei(); sleep_mode();
  //sleep_cpu();
  
  //Z-Z-Z-Z-Z-Z-Z-Z-Z-Z-Z until an interrupt, then...
  sleep_disable(); 
  sbi(ADCSRA, ADEN);

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
Serial.print(F("Free Mem: "));Serial.println(fm);Serial.flush();
}

//*****************************************
bool longPress() { bool ret=false;
  byte ctr=0;
  if (digitalRead(pinBOOT_SW)==1){delay(100);}
  while ((ctr<50) && (digitalRead(pinBOOT_SW)==0)) {
    ctr++; delay(50); }
  if (ctr==50) { ret=true; }
  Serial.print(F("longPress="));Serial.println(ctr);Serial.flush();
  return ret;
}

//*****************************************
void EE_ERASE_all() {
  Serial.print(F("EE_ERASE_all...#"));Serial.flush();
  for (word i=0;i<1024;i++) { EEPROM.write(i,255); //FF's
    if (i % 200 == 0) { Serial.println(F(" . "));
      digitalWrite(pinLED_BOOT, HIGH);
      delay(20);
      digitalWrite(pinLED_BOOT, LOW);
    }
  } 
  Serial.println(F(" ...Done#"));Serial.flush();
}

//*****************************************
void EE_ERASE_id(int sbn) { sbn++; //beacon is -1, so all bump up
  Serial.print(F("EE_ERASE_id...#"));Serial.print(sbn);Serial.flush();
  word idLoc=(EE_ID-((sbn)*6)); //sbn can be -1 for beacon, so bump all up
  for (byte i=0;i<6;i++) { EEPROM.write(idLoc-i,255); } //the rest get FF's
  Serial.println(F(" ...Done#"));Serial.flush();
}

 //*****************************************
void EE_ERASE_key() { Serial.print(F("EE_ERASE_key"));Serial.flush();
  for (byte i=0;i<16;i++) { EEPROM.write(EE_KEY-i,255); } //the rest get FF's
  Serial.println(F(" ...Done#"));Serial.flush();
}
