
//*****************************************
void packet_SEND(byte sbn, char *id, double bv, char *key, char *data, int pwr) { 
char n2a[10]; // for Number TO Ascii things
char msg[48];
byte PV=1; //Protocol Version - very first char byte out;
  if (data!="NULL") {
    itoa(int(PV),n2a,10); strcpy(msg,n2a); //Prot-Ver always very first char out
    strcat(msg,"|");strcat(msg,id);     
    strcat(msg,"|"); itoa(int(sbn),n2a,10); strcat(msg,n2a); 
    strcat(msg,"|"); dtoa(bv,n2a,1); strcat(msg,n2a);    
    strcat(msg,"|"); strcat(msg,data);                   
    msg_SEND(msg,key,pwr); 
  }
}

//*****************************************
void msg_SEND(char *msgIN, char *key, int pwr) { 
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(100);}
  digitalWrite(pinLED, HIGH);
  byte txLEN=strlen(msgIN);
  char txBUF[64];
  tx_ENCODE_0(txBUF,msgIN,txLEN,key);
  if (RF95_UP==false) { RF95_UP=init_RF95(); }
  rf95.setTxPower(pwr*2, false); //from 1-10 to 2-20dB
  rf95.send(txBUF,txLEN); rf95.waitPacketSent();
  txBV = get_BatteryVoltage();
}

//*****************************************
char *tx_ENCODE_0(char *txBUF, char *msgIN, byte msgLEN, char *key) { char *ret=txBUF;
  byte k=0;//random(0,8); //0-7 (8 numbers)
  byte keyLEN=strlen(key);  
  for (byte i=0;i<msgLEN;i++) {
    if (k==keyLEN) {k=0;}
    txBUF[i]=byte((byte(msgIN[i])^byte(key[k])));
    k++;
  }
  return ret;
}  

//*****************************************
char *rx_DECODE_0(char *msgOUT,char *rxBUF, byte rxLEN, char *key) {char *ret=msgOUT; //if (debugON>0) {Serial.println(F("...decode")); Serial.flush();}
  byte i; byte k=0;
  byte keyLEN=strlen(key); 
  for (i=0;i<rxLEN;i++) {
    if (k==keyLEN) {k=0;}
    msgOUT[i]=byte((byte(rxBUF[i])^byte(key[k])));
    k++;
  }
  msgOUT[i]=0;
  return ret;
}

//*****************************************
void print_HEX(char *buf,byte len) { byte i;
  Serial.print(len);Serial.print(F(": "));
  for (i=0;i<(len-1);i++) {Serial.print(byte(buf[i]),HEX);Serial.print(F(" "));}
  Serial.println(byte(buf[len-1]),HEX); Serial.flush();
}
//*****************************************
void print_CHR(char *buf,byte len) { byte i;
  Serial.print(len);Serial.print(F(": "));
  for (i=0;i<(len-1);i++) {Serial.print( buf[i]);}
  Serial.println(buf[len-1]);Serial.flush();
}
