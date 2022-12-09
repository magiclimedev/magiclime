
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
  Serial.println(F("trigger_RESET"));Serial.flush();
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
  if (flgKEY_GOOD==true) {
    wakeWHY=1;}
  wd_COUNTER=wd_TIMER;
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
  pinMode(pinBOOT_SW, OUTPUT); digitalWrite(pinBOOT_SW, HIGH); 
  delay(10);
  pinMode(pinBOOT_SW, INPUT); digitalWrite(pinBOOT_SW, LOW); 
  byte ctr=0;
  if (digitalRead(pinBOOT_SW)==1){delay(200);}
  while ((ctr<20) && (digitalRead(pinBOOT_SW)==0)) {
    ctr++; delay(50); }
  if (ctr==20) { ret=true; }
  Serial.print(F("longPress="));Serial.println(ctr);Serial.flush();
  digitalWrite(pinBOOT_SW, HIGH);
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
