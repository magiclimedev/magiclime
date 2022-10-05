
//*****************************************
byte get_SBNum() {
if (debugON>0) {Serial.print(F("get_SBNum: "));}
  byte SBN; //Sensor Board Number
  byte VBS=digitalRead(pinBOOST);
  digitalWrite(pinBOOST, HIGH); delay(10);
  pinMode(pinSBID,OUTPUT);digitalWrite(pinSBID,HIGH);
  pinMode(pinSBID,INPUT);delay(5);
  int WIGval=analogRead(pinSBID);
  pinMode(pinSBID,OUTPUT);digitalWrite(pinSBID,LOW);
  pinMode(pinSBID,INPUT);delay(5);
  int SBval=analogRead(pinSBID);
  WIGval=abs(WIGval-SBval); 
  if (debugON>0) {Serial.print(F("WIGval="));Serial.println(WIGval);}
  if (WIGval>10) { SBN=0; }
  else { SBN=byte((int(SBval/51)+1) );} //*10); }
  if (VBS==0) {digitalWrite(pinBOOST, LOW); delay(5); }
    if (debugON>0) {Serial.println(SBN);Serial.flush();}
  return SBN;
}  
  
//*****************************************
void boost_ON() {
  if (digitalRead(pinBOOST) == 0) { 
    pinMode(pinMOSI, OUTPUT);
    pinMode(pinSCK, OUTPUT);
    digitalWrite(pinRF95_CS,HIGH);
    digitalWrite(pinBOOST, HIGH); delay(10); }
  digitalWrite(pinLED, HIGH);
  if (debugON>0) {Serial.println(F("\nboost-ON *****"));Serial.flush();}
}

//*****************************************
void boost_OFF() {
    if (debugON>0) {Serial.println(F("\n ****** boost-OFF"));Serial.flush();}
  digitalWrite(pinBOOST, LOW);
  pinMode(pinMOSI, INPUT);digitalWrite(pinMOSI,LOW);
  pinMode(pinSCK, INPUT);digitalWrite(pinSCK,LOW); 
  digitalWrite(pinRF95_CS,LOW);
  digitalWrite(pinLED, LOW);
  RF95_UP=false;
}

//*****************************************
void led_GREEN_BLINK(byte count,byte bON,byte bOFF) { //dur,rate is 10mS per
  byte pStat=digitalRead(pinLED);
  if (pStat==1) { digitalWrite(pinLED, LOW); delay(200);}
  for (byte i=0;i<count;i++) {
    word onDly=bON*10; word offDly=bOFF*10;
    digitalWrite(pinLED, HIGH); delay(onDly);
    digitalWrite(pinLED, LOW); delay(offDly);
  }
  if (pStat==1) {digitalWrite(pinLED, HIGH);}
}

//*****************************************
void led_PAIR_BLINK(byte count,byte bON,byte bOFF) { //dur,rate is 10mS per
  digitalWrite(pinPAIR_LED, LOW); delay(200);
  for (byte i=0;i<count;i++) {
    word onDly=bON*10; word offDly=bOFF*10;
    digitalWrite(pinPAIR_LED, HIGH); delay(onDly);
    digitalWrite(pinPAIR_LED, LOW); delay(offDly);
  }
}

//*****************************************
float get_BatteryVoltage() {
  byte VBS = digitalRead(pinBOOST); if (VBS == 0) { boost_ON(); delay(50); }
  float fBV = get_Average(pinBV, 5); fBV = (fBV * mV_bit) / 1000.0;
if (debugON>1) {Serial.print(F("BV: "));Serial.println(fBV);}
  if (VBS == 0) { boost_OFF(); }
  return fBV;
}

//*****************************************
void trigger_RESET(byte SBN){ //global DATA_TYPE req,
  switch (SBN) { //some sensors need resetting after activation, like E931
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
void IRPT_D3() {  sendDATA=1; txCOUNTER=txTIMER; 
} 

//*****************************************
ISR(WDT_vect) { //in avr library
  txCOUNTER--;  //Serial.print(txCOUNTER);Serial.print(F(" "));Serial.flush();
  if (txCOUNTER<=0) {
    if (HrtBtON==true) {sendDATA=2;} else {sendDATA=1;};
    txCOUNTER=txTIMER;// Serial.print(F("^"));Serial.println(txCOUNTER);Serial.flush();
  }
}

//*****************************************
void wakeStuff(){if (digitalRead(pinBOOST)==0){boost_ON();}}//SPI.begin();}

//*****************************************
void sleepStuff(){boost_OFF(); SPI.end();}

//*****************************************
void systemSleep() {
if (debugON>0) {Serial.print(F("Z"));}  
  if (digitalRead(pinBOOST)==1){boost_OFF();}
  cli();   cbi(ADCSRA, ADEN);   sbi(ACSR, ACD);
  sleep_enable();  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sei(); sleep_mode();
  //sleep_cpu();
  
  //Z-Z-Z-Z-Z-Z-Z-Z-Z-Z-Z (until an interrupt) Z-Z-Z-Z-Z-Z-Z-Z-Z-Z-Z-Z-Z
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
