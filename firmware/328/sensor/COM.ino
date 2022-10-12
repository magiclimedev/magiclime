
//*****************************************
void packet_SEND(byte sbn, String data, int wait) { if (debugON>0) {Serial.println(F("\n...packet_SEND"));}
if (debugON>1) {Serial.print(F("data="));Serial.print(data);Serial.print(F(", dBV="));Serial.println(dBV);Serial.flush(); }
byte PROTOCOL_VERSION=1;
String sBV = String(dBV,1); 
if (sBV=="") {sBV="3.0";} //WTF?? Why is the first time a null??
if (debugON>1) {Serial.print(F("sBV="));Serial.println(sBV);Serial.flush(); }
  if (data!="NULL") {
    sMSG= "1|"; sMSG+=TX_ID;
    switch (PROTOCOL_VERSION) {
      case 0: { sMSG+= "|"; sMSG+=data; } break;
      //dBV has double value of previous packets' under-load Battery Voltage.
      case 1: { 
        sMSG+="|"; sMSG+=String(sbn);
        sMSG+="|"; sMSG+=sBV; //String(dBV,1);
        sMSG+="|"; sMSG+=data; } break;
      case 2: {
        byte seqnum=EEPROM.read(EE_SEQNUM)+1;
        EEPROM.write(EE_SEQNUM,seqnum);
        sMSG+="|"; sMSG+=String(seqnum);
        sMSG+="|"; sMSG+=String(sbn);
        sMSG+="|"; sMSG+=sBV; //String(dBV,1);
        sMSG+="|"; sMSG+=data; } break;
    }
    msg_SEND(sMSG, TX_KEY,txPWR); 
    delay(wait); //hold-off undesired re-triggers
  }
}

//***********************
//decoded and checksum-tested 
String msg_GET(char *buf, byte len,String &key) { if (debugON>0) {Serial.println(F("...msg GET")); }
  sRET="";
    if (debugON>1) {print_HEX(buf,len); }
    if (debugON>1) {Serial.print(F("decoding with: ")); Serial.println(key);Serial.flush();}
  char *msg=decode(buf,len,key); msg[len]=0;
    if (debugON>1) {Serial.print(F("msg dec: ")); print_CHR(msg,len-1); }
  byte CSB=csb_MAKE(msg,len-1);
  byte csb=msg[len-1];
    if (debugON>1) {Serial.print(F("CSB: ")); Serial.print(CSB,HEX); }
    if (debugON>1) {Serial.print(F(", csb: ")); Serial.println(csb,HEX);Serial.flush(); }
    if (debugON>1) {print_HEX(msg,len-1);print_CHR(msg,len-1);}
  if (csb == CSB) {   msg[len-1]=0; sRET=String(msg); }
    if (debugON>0) {Serial.print(F("sRET: ")); Serial.println(sRET);Serial.flush(); }
  return sRET;
}

//***********************
//decoded and checksum-tested 
char *msg_GET2(char *buf, byte len,String &key) {
    if (debugON>0) {Serial.print(F("\nmsg_GET2in: "));print_HEX(buf,len);}
  byte VBS = digitalRead(pinBOOST); if (VBS == 0) { boost_ON(); }
    //Serial.print(F("RF95_UP="));Serial.println(RF95_UP);
  if (RF95_UP==false) { RF95_UP=init_RF95(); 
    //Serial.print(F(", RF95_UP="));Serial.println(RF95_UP);
  }
  char *msg=decode(buf,len,key);
  byte CSB=csb_MAKE(msg,len-1);
  //char *ret=0; 
    if (debugON>0) {Serial.print(F("CSB=")); Serial.print(CSB,HEX);}
    if (debugON>0) {Serial.print(F(",  msg[")); Serial.print(len-1);Serial.print(F("]=")); Serial.println(msg[len-1],HEX);}
  if (msg[len-1]==CSB) {
    msg[len-1]=0;
    //ret=msg;
  }
  else { msg[0]=0;}
if (debugON>0) {Serial.print(F("\nmsg_GET2out: "));print_CHR(msg,len-1);}//print_CHR(ret,len-1);}
  return msg; //ret;
}

//*****************************************
void msg_SEND2(char *msgIN, byte msgLEN, String key, byte txPWR) { //txPWR is 1-10
  byte VBS = digitalRead(pinBOOST); if (VBS == 0) { boost_ON(); }
  digitalWrite(pinLED, HIGH);
    if (debugON>0) {Serial.print(F("\nmsg_SEND2: ")); print_CHR(msgIN,msgLEN);}
  //Serial.print(F("RF95_UP="));Serial.print(RF95_UP);
  if (RF95_UP==false) { RF95_UP=init_RF95(); 
    //Serial.print(F(", RF95_UP="));Serial.print(RF95_UP);
  } //Serial.println("");
  byte CSB=csb_MAKE(msgIN,msgLEN);
  msgIN[msgLEN]=CSB;  
    if (debugON>0) {Serial.print(F("CSB=")); Serial.print(CSB,HEX);}
    if (debugON>0) {Serial.print(F(", msgIN[")); Serial.print(msgLEN);Serial.print(F("]=")); Serial.print(msgIN[msgLEN],HEX);}
    if (debugON>0) {Serial.print(F(", key: ")); Serial.println(key);Serial.flush();  }
  msgLEN++;
  char *msg=encode(msgIN,msgLEN,key);
  rf95.setTxPower((txPWR*2), false); // from 1-10 to 2-20dB
  rf95.send(msg,msgLEN);//+1); //rf95 need msgLen to be one more ???
  dBV = get_BatteryVoltage();
  rf95.waitPacketSent();
  if (VBS == 0) {  boost_OFF(); }
    if (debugON>0) {Serial.println(F("EO_msg_SEND2")); Serial.flush();}
}

//*****************************************
void msg_SEND(String &msgIN, String &key, int txPWR) { 
  if (digitalRead(pinBOOST) == 0) { boost_ON(); delay(100);}
  digitalWrite(pinLED, HIGH);
  if (debugON>0) {Serial.print(F("...msg_SEND: "));Serial.print(msgIN);
    Serial.print(F(" ,key=")); Serial.print(key);
    Serial.print(F(" ,txPWR=")); Serial.println(txPWR); Serial.flush();}
  char buf[64]; byte len=msgIN.length(); msgIN.toCharArray(buf,len+1);
    if (debugON>0) {print_CHR(buf,len);}
  byte CSB=csb_MAKE(buf,len); 
  buf[len]=CSB;
    if (debugON>1) {Serial.print(F("CSB=")); Serial.print(CSB,HEX);}
    if (debugON>1) {Serial.print(F(", buf[")); Serial.print(len);Serial.print(F("]=")); Serial.print(buf[len],HEX);}
    if (debugON>1) {Serial.print(F(", key: ")); Serial.println(key);Serial.flush();}
  len++;  
  char *msg=encode(buf,len,key);
  //print_HEX(msg,len);
  init_RF95();
  rf95.setTxPower(txPWR*2, false); //from 1-10 to 2-20dB
   
  //for (byte i=0;i<10;i++) { rf95.send(msg,len);rf95.waitPacketSent();}
  
  rf95.send(msg,len); rf95.waitPacketSent();
  dBV = get_BatteryVoltage();
    if (debugON>0) {Serial.print(F("dBV=")); Serial.print(dBV);     
      Serial.println(F("...EO_msg_SEND")); Serial.flush();}
}

//*****************************************
//encode is just before sending, so is char array for rf95.send
char *encode(char *msg, byte len, String &key) { if (debugON>0) {Serial.println(F("...encode")); }
  randomSeed(analogRead(2)+analogRead(3)+analogRead(4));
  byte sp=random(0,8);
  static byte cOUT[64];  byte i=0; byte k=sp;
  if (key=="") {key[0]=0; } //XOR of zero is 'no change'
  int lenKEY=key.length();  
  while (i<len) {if (k>(lenKEY-1)) {k=0;}
    cOUT[i]=byte((byte(msg[i])^byte(key[k])));
    i++; k++;} 
  return cOUT;
} 

//*****************************************
char *decode(char *msg, byte len, String &key) { if (debugON>0) {Serial.println(F("...decode")); }
  static byte cOUT[64]; cOUT[0]=0; byte i=0; byte k=0;
  if (key=="") {key[0]=0; } //XOR of zero is 'no change'
  int lenKEY=key.length(); 
  while (i<len) {if (k>(lenKEY-1)) {k=0;}
    cOUT[i]=byte((byte(msg[i])^byte(key[k])));
    i++; k++;} 
  return cOUT;
}

//*****************************************
byte csb_MAKE(char *pkt, byte len) {  byte CSB = 0;
  for (byte i = 0; i < len; i++) { CSB = CSB ^ pkt[i]; }
  return CSB;
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
