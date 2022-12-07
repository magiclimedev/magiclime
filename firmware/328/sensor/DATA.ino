//*****************************************
char *get_DATA(char *data, int sbn, byte why ) { char *ret=data;
  detachInterrupt(digitalPinToInterrupt(pinEVENT)); //in IRPT3?
  if (why==2) {strcpy(data,"HEARTBEAT");}
  else {
    switch (sbn) { 
      case -1: { strcpy(data,"BEACON");  } break;//not used as HB fills the bill.
      case 0: { strcpy(data,"MY_SBN0");  } break;//SBN pin grounded
      case 1: { strcpy(data,"PUSH");    } break;//button
      case 2: { get_TILT(data);         } break; //tilt
      case 3: { get_REED(data);         } break; //reed
      case 4: { strcpy(data,"SHAKE");   } break; //
      case 5: { strcpy(data,"MOTION");  } break; //
      case 6: { strcpy(data,"KNOCK");   } break; //
      case 7: { get_2BTN(data);         } break; //2BUTTON
      case 8: { strcpy(data,"S08");     } break; //
      case 9: { strcpy(data,"S09");     } break; //
      case 10: { get_TMP36_F(data);     } break; //TMP36 
      case 11: { get_LIGHT(data);       } break; //photocell        
      case 12: { get_TRH(data);         } break; //SI7020 
      case 13: { strcpy(data,"S13");    } break; //
      case 14: { strcpy(data,"S14");    } break; //
      case 15: { strcpy(data,"S15");    } break; //
      case 16: { strcpy(data,"S16");    } break; //
      case 17: { strcpy(data,"S17");    } break; //
      case 18: { strcpy(data,"S18");    } break; //
      case 19: { strcpy(data,"S19");    } break; //
      case 20: { strcpy(data,"S20");    } break; //
      case 21: { get_DOT(data);         } break; //
      case 22: { strcpy(data,"MY_SBN22");    } break; // SBN pin tied to Aref
    }
  }  
  return ret;;
}


//************************************************************
char *get_2BTN(char *data) { char *ret=data; 
    delay(100); //not a spike?
    data[0]=0; //default fail flag
    if ((digitalRead(A5)==1) && (digitalRead(A4)==1)) { strcpy(data,"PUSH-3");}
    else if (digitalRead(A5)==1) { strcpy(data,"PUSH-1");}
    else if (digitalRead(A4)==1) { strcpy(data,"PUSH-2");}
  return ret;
}    

//************************************************************
char *get_TILT(char *data) { char *ret=data; //uses global dataOLD
  byte smplCtr=0; byte d1=0; byte d2=0;
  data[0]=0; //default fail flag
  while (smplCtr<200) {//2 sec of stable
    d1=digitalRead(A4);
    if (d1==d2) {smplCtr++;}
    else {smplCtr=0;}
    d2=d1;
    delay(10); //10 * 200 = 2 sec.
  }
  Serial.print(F("d1: "));Serial.println(d1);Serial.flush();
  //if (bitRead(optBYTE,0)==1){d1=!d1;} //optBYTE says - 'flip the state'
  //Serial.print(F("d1-flip: "));Serial.println(d1);Serial.flush();
  if ( (d1==0) && (strcmp(dataOLD,"LOW")!=0) ){ strcpy(data,"LOW"); strcpy(dataOLD,data); }
  else if ((d1==1)&&(strcmp(dataOLD,"HIGH")!=0) ) { strcpy(data,"HIGH"); strcpy(dataOLD,data); }
  return ret;
}

//************************************************************
char *get_REED(char *data) { char *ret=data;
  if (digitalRead(pinEVENT)==HIGH){strcpy(data, "CLOSE");}
  else {strcpy(data,"OPEN");}
  return ret;
}

//************************************************************
char *get_DOT(char *data) { char *ret=data; 
  if ((digitalRead(A4)==0)&& (digitalRead(A5)==1)){strcpy(data,"MOT-LEFT");}
  else if ((digitalRead(A4)==1)&& (digitalRead(A5)==0)){strcpy(data,"MOT-RIGHT");}
  else if ((digitalRead(A4)==1)&& (digitalRead(A5)==1)){strcpy(data,"MOT-BOTH");}
  return ret;
 } 
 
//************************************************************
char *get_TMP36_F(char *data) { char *ret=data;
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(100);}
  float TC=get_MilliVolts(0);
  TC=(TC-500.0)/10.0; //Celcius
  float TF=((TC * 9.0) / 5.0) + 32.0;  //Fahrenheit
  float TP=get_Average(pinTrimPot,10);
  float CalAdj=TF*( ((512.0-TP)/128.0 )/100.0); //+-4%
  char n2a[10]; dtoa(n2a,(TF+CalAdj),1); 
  strcpy(data,n2a);
  return ret;
} 

//*****************************************
char *get_TRH(char *data) { char *ret=data;
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(100);}
  init_Si7020(); delay(10);
  char data1[10]; char data2[10];
  get_Si7020_F(data1);
  get_Si7020_RH(data2);
  strcpy(data,data1); strcat(data,","); strcat(data,data2); 
  return ret;
} 

//***********************  
void init_Si7020() { 
  //Serial.println(F("init_Si7020()... "));Serial.flush();
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(500);} 
  Wire.begin();
  byte ureg=Si7020_UREG_get();
  ureg=(ureg & 0x7E)| 0x01;
  Si7020_UREG_set(ureg);
  digitalWrite(A4,LOW); digitalWrite(A5,LOW);delay(10);
}

//***********************
byte Si7020_UREG_get(){  byte ureg;
  Wire.beginTransmission(0x40);  Wire.write(0xE7);  Wire.endTransmission();
  Wire.requestFrom(0x40, 1);  ureg=Wire.read();  return ureg;
} 
  
//***********************
void Si7020_UREG_set(byte val){ Wire.beginTransmission(0x40);
  Wire.write(0xE6); Wire.write(val); Wire.endTransmission();
}
  
//***********************  
char *get_Si7020_F(char *data) { char *ret=data;
  byte msb; byte lsb; word wTMP; float fTMP; char cTMP[6];
  Wire.beginTransmission(0x40);  Wire.write(0xF3); 
  Wire.endTransmission();  delay(20);
  Wire.requestFrom(0x40, 2);
  if(Wire.available() >= 2) {    
    msb=Wire.read(); lsb=Wire.read(); 
    wTMP=(msb*256)+lsb; 
    fTMP=((175.72 * float(wTMP))/ 65536.0)-46.85; //celcius
    fTMP=((fTMP*9.0)/5.0)+32.0; //Fahrenheit
    char n2a[10]; dtoa(n2a,fTMP,1);
    strcpy(data,n2a);
  }
  else {strcpy(data,"---"); }
  return ret;
}

//***********************
char *get_Si7020_RH(char *data) { char *ret=data;
  byte msb; byte lsb; word RH; float fRH; char cRH[4];
  Wire.beginTransmission(0x40);  Wire.write(0xF5);
  Wire.endTransmission();   delay(20);
  Wire.requestFrom(0x40, 2); 
  if(Wire.available() >= 2) {
    msb=Wire.read(); lsb=Wire.read(); RH=(msb*256)+lsb; 
    fRH=int(((125.0 * float(RH))/ 65536.0)-6); //RH
    if (fRH>100) { fRH=100; } else if (fRH<0) { fRH=0; }
    char n2a[10]; itoa(int(fRH),n2a,10); 
    strcpy(data,n2a);
  }
  else {strcpy(data,"---"); }
  return ret;
} 

//***********************
char *get_LIGHT(char *data) { char *ret=data;
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
  char n2a[10]; itoa(PCT,n2a,10); 
  strcpy(data,n2a);
  return ret;
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

//*****************************************
char* dtoa(char *cMJA, double dN, int iP) {char *ret = cMJA;
  //arguments... 
  // char array to fill,float-double value,  precision (4 is .xxxx)
  //and... it rounds last digit up/down!
   long lP=1; byte bW=iP;
  while (bW>0) { lP=lP*10;  bW--;  }
  long lL = long(dN); double dD=(dN-double(lL))* double(lP); 
  if (dN>=0) { dD=(dD + 0.5);  } else { dD=(dD-0.5); }
  long lR=abs(long(dD));  lL=abs(lL);  
  if (lR==lP) { lL=lL+1;  lR=0;  }
  if ((dN<0) & ((lR+lL)>0)) { *cMJA++ = '-';  } 
  ltoa(lL, cMJA, 10);
  if (iP>0) { while (*cMJA != '\0') { cMJA++; } *cMJA++ = '.'; lP=10; 
  while (iP>1) { if (lR< lP) { *cMJA='0'; cMJA++; } lP=lP*10;  iP--; }
  ltoa(lR, cMJA, 10); }  return ret; }
  
