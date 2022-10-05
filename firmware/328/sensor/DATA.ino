//*****************************************
String get_DATA(byte SBN, byte why ) {  sSTR18="NULL"; //false trigger default
  detachInterrupt(digitalPinToInterrupt(pinEVENT));
  if (why==2) {sSTR18="HEARTBEAT";}
  else {
    switch (SBN) { 
      case 0: { sSTR18="BEACON"; } break;//not used as HB fills the bill.
      case 1: { sSTR18="PUSH";  } break;//button
      case 2: { if (get_PIN_DB(pinSDA,50)==0){sSTR18="LOW";}
                else {if (get_PIN_DB(pinSDA,50)==1){ sSTR18="HIGH";}}
              } break; //tilt
      case 3: { sSTR18="OPEN"; 
        if (digitalRead(pinEVENT)==HIGH){sSTR18= "CLOSE";}  } break; //reed
      case 4: { sSTR18="KNOCK";   } break; //
      case 5: { sSTR18="MOTION"; } break; //
      case 6: { sSTR18="S06";   } break; //
      case 7: { sSTR18="S07";   } break; //
      case 8: { sSTR18="S08";   } break; //
      case 9: { sSTR18="S09";   } break; //
      case 10: { sSTR18= Get_TMP36_F(); } break; //TMP36 
      case 11: { sSTR18= Get_Light(); } break; //photocell        
      case 12: { sSTR18= Get_TRH(); } break; //SI7020      case 20: { sSTR18="S20";   } break; //
      case 13: { sSTR18="S13";   } break; //
      case 14: { sSTR18="S14";   } break; //
      case 15: { sSTR18="S15";   } break; //
      case 16: { sSTR18="S16";   } break; //
      case 17: { sSTR18="S17";   } break; //
      case 18: { sSTR18="S18";   } break; //
      case 19: { sSTR18="S19";   } break; //
      case 20: { sSTR18="S20";   } break; //
      case 21: { sSTR18=Get_DOT(); } break; //
      case 22: { sSTR18="S22";   } break; //
      case 23: { sSTR18="S23";   } break; //
      case 24: { sSTR18="S24";   } break; //
      case 25: { sSTR18="S25";   } break; //
      case 26: { sSTR18="S26";   } break; //
      case 27: { sSTR18="S27";   } break; //
      case 28: { sSTR18="S28";   } break; //
      case 29: { sSTR18="S29";   } break; //
    }
  } //if heartbeat  
  return sSTR18;
}


//***************************************** 
void init_E931() { //Elmos motion IC
  
  //pin D3 is the usual interrupt pin
  //!! NOT !! pin D4 (pinSWITCH) is the serial programming pin
  //!!NEW!! for Tiny2040 compatibility - no D4 but A0 is avail, so A0 is used
  //programmable, so, config registers...
  //reg. bits (from:thru) - desc - param.
  //1. 24:17 (8)- Sensitivity - Register value * 6.5uV
  byte reg_H=B00100000; //24-17 (8 bits), 64=1/4 max  [High Byte]
  
  //2. 16:13 (4)- Blind Time  - Reg. value * .5 sec. (.5-8)
  byte reg_M=B10000000; //1111 (15*.5 sec.)           [Middle Byte]
  //3. 12:11 (2)- Pulse Counter - Reg.Val. +1 (1-4)
  reg_M=reg_M|B00000000; //11 -> 3 
  //4. 10:9 (2)- Window Time - (RV * 4 sec) + 4 sec. (4-16)
  reg_M=reg_M|B00000000; //01 -> 1*4+4=8
  
  //5. 8 - MotDet Enable - 0,1
  byte reg_L=B10000000; //1 -> yes, enable            [Low Byte]
  //6. 7 - Irpt. Source - 0=motion, 1=dec.filter
  reg_L=reg_L|B00000000; 
  //7. 6:5 - ADC V.Source - 0=PIR-HFP, 1=PIR-LPF, 2=Supply V., 3=Temp Sensor?
  reg_L=reg_L|B00000000;
  //8. 4 - 2.2V. V.Reg Enable - 0=ON
  reg_L=reg_L|B00000000;
  //9. 3 - self test - 0->1=start a self test
  //10. 2 - sample cap size - 1=2*default????
  //11. 1:0 - User test modes - reserved=0
  //That's 25 bits, not 3 bytes, so do one more at end.
  
  //Fosc(min) ~ 50kHz -> 20uS
  //Fclk(min) ~ 50kHz/2 -> 25kHz,40uS
  //tWL(16/Fclk) -> 640uS
  //tBW(2/Fclk) -> 80uS 
  //tL and tH clock 200nS -> 1uS

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

//************************************************************
String Get_DOT() {//read two pins - one should be 'the one'.
  sSTR18="";
  if ((digitalRead(A4)==1)&& (digitalRead(A5)==1)){sSTR18+="MOT-MUCH";}
  //crap - one always gets there first and resets the other. :-(
  else if ((digitalRead(A4)==0)&& (digitalRead(A5)==1)){sSTR18+="MOT-LEFT";}
  else if ((digitalRead(A4)==1)&& (digitalRead(A5)==0)){sSTR18+="MOT-RIGHT";}
  return sSTR18;
 } 
 
//************************************************************
String Get_TMP36_F() { 
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(10);}
  float TC=get_MilliVolts(0);
  TC=(TC-500.0)/10.0; //Celcius
  float TF=((TC * 9.0) / 5.0) + 32.0;  //Fahrenheit
  float TP=get_Average(pinTrimPot,10);
  float CalAdj=TF*( ((512.0-TP)/128.0 )/100.0); //+-4%
  return String(TF+CalAdj,1);
} 

//*****************************************
String Get_TRH(){ 
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(100);}
  init_Si7020(); delay(10);
  sSTR18=Get_Si7020_F(); sSTR18+=",";
  sSTR18+=Get_Si7020_RH();
  return sSTR18;
} 

//***********************  
void init_Si7020() {
    if (debugON>0) {Serial.println(F("init_Si7020()... "));}
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(500);} 
  Wire.begin();
  byte ureg=Get_Si7020_UREG();
  ureg=(ureg & 0x7E)| 0x01;
  Set_Si7020_UREG(ureg);
  digitalWrite(A4,LOW); digitalWrite(A5,LOW);delay(10);
}

//***********************
byte Get_Si7020_UREG(){  byte ureg;
  Wire.beginTransmission(0x40);  Wire.write(0xE7);  Wire.endTransmission();
  Wire.requestFrom(0x40, 1);  ureg=Wire.read();  return ureg;
} 
  
//***********************
void Set_Si7020_UREG(byte val){ Wire.beginTransmission(0x40);
  Wire.write(0xE6); Wire.write(val); Wire.endTransmission();
}
  
//***********************  
String Get_Si7020_F(){  
    if (debugON>0) {Serial.println(F("Get_Si7020_F()... ")); }
  byte msb; byte lsb; word wTMP; float fTMP; char cTMP[6];
  sSTR8="999";
  Wire.beginTransmission(0x40);  Wire.write(0xF3); 
  Wire.endTransmission();  delay(20);
  Wire.requestFrom(0x40, 2);
  if(Wire.available() >= 2) {    
    msb=Wire.read(); lsb=Wire.read(); 
    wTMP=(msb*256)+lsb; 
    fTMP=((175.72 * float(wTMP))/ 65536.0)-46.85; //celcius
    fTMP=((fTMP*9.0)/5.0)+32.0; //Fahrenheit
    sSTR8=String(fTMP,1);
  }
  return sSTR8;
}

//***********************
String Get_Si7020_RH(){
    if (debugON>0) {Serial.print(F("Get_Si7020_RH()... "));}
  byte msb; byte lsb; word RH; float fRH; char cRH[4];
  sSTR8="999";
  Wire.beginTransmission(0x40);  Wire.write(0xF5);
  Wire.endTransmission();   delay(20);
  Wire.requestFrom(0x40, 2); 
  if(Wire.available() >= 2) {
    msb=Wire.read(); lsb=Wire.read(); RH=(msb*256)+lsb; 
    fRH=int(((125.0 * float(RH))/ 65536.0)-6); //RH
    if (fRH>100) { fRH=100; } else if (fRH<0) { fRH=0; }
    sSTR8=String(int(fRH));
  }
  return sSTR8;
} 

//***********************
String Get_Light() {
    if (debugON>0) {Serial.print(F("Get_Light()... ")); }
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(10);}
  digitalWrite(4,HIGH); delay(1);
  int LVL=analogRead(0); 
  digitalWrite(4, LOW);
  if (LVL>MAX1) {MAX1=LVL;} 
  if (LVL<MIN1) {MIN1=LVL;}
  int PCT;
  if (MAX1==MIN1) { PCT=0; }
  else { PCT= int(((long(LVL)-long(MIN1))*100)/(MAX1-MIN1));
    if (PCT>100) {PCT=100;} else if (PCT<0) {PCT=0;} }
  return String(PCT); 
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
  byte VBS = digitalRead(pinBOOST); if (VBS == 0) { digitalWrite(pinBOOST, HIGH); delay(1); }
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
