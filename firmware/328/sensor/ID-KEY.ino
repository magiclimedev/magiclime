
// oh, btw... csb is before encoding, csb checked after decoding.
//*****************************************
String key_GET_PUBLIC(String &TXID) { 
  //freeMemory();
if (debugON>0) {Serial.print(F("key_GET_PUBLIC... "));Serial.flush();}
  digitalWrite(pinPAIR_LED, HIGH); delay(1000);
  sRET="";
  sSTR34=key_NEW(); //for return of public key
  key_TX_ID_SEND(TXID,sSTR34); //expected response = 'sTMP' encoded 'ID:PublicKey'
  byte timeout=100;
  while (!rf95.available() && timeout>0) { delay(10); timeout--; }
  if (timeout==0) { digitalWrite(pinPAIR_LED,LOW);  return sRET;}
if (debugON>0) {Serial.print(F("timeout<50: "));Serial.println(timeout);Serial.flush();}

  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  if (rf95.recv(buf, &len)) {  byte rss=rf95.lastRssi()+140;
  if (debugON>0) {Serial.print(F("rss: "));Serial.println(rss);}
    if (rss>=keyRSS) { 
      sMSG=msg_GET(buf,len,sSTR34); //decodes ,if CSB ok
      if (sMSG !="") { byte i=0; 
        while ((sMSG[i]!=0)&&(sMSG[i]!=':')) {i++;}
        if (sMSG[i]==':') { //not a fail-to-find...
          sSTR8=sMSG.substring(0,i); //ID
	        sSTR18=sMSG.substring(i+1); //KEY
        if (debugON>0) {Serial.print(F("ID: "));Serial.print(sSTR8);}
			  if (debugON>0) {Serial.print(F(", KEY: "));Serial.println(sSTR18);Serial.flush();}
          if (sSTR8==TXID) { sRET=sSTR18; } //only if it's for me ?? and if key validate=true?
        }
      }
    }
  }
  digitalWrite(pinPAIR_LED,LOW); 
  //freeMemory();
  return sRET;
}

//*****************************************
bool key_VALIDATE(String &key) { //check EEPROM for proper character range
    if (debugON>0) {Serial.print(F("key_VALIDATE: "));Serial.print(key);}
  byte len=key.length(); bool ret=true;
    if (debugON>0) {Serial.print(F(" of length: "));Serial.print(len);}
  if (len==0) {ret=false;}
  for (byte i=0;i<len;i++) { //Serial.print(key[i]);Serial.print(F(" ")); 
    if ((key[i]<34) || (key[i]>126)|| (len<4)) { ret=false; break; }
  }
    if (debugON>0) {if (ret==1) {Serial.println(F(" = TRUE"));} else {Serial.println(F(" = FALSE"));} Serial.flush();}
  return ret;
}

//*****************************************
void key_TX_ID_SEND(String &TXID, String &key_TEMP) {
    if (debugON>0) { Serial.print(F("key_TX_ID_SEND-> "));Serial.flush();}
  String idkey((char *)0); idkey.reserve(48); //1+6+1+32+null plus a few more
  //idkey="INFO: PRE-KEY FOR RSS"; msg_SEND(idkey, key_TEMP,1);
  idkey="!";idkey+=TXID;idkey+="!";idkey+=key_TEMP;
  byte len = idkey.length(); 
  String key((char *)0);key.reserve(20);key+="thisisathingbits";
    if (debugON>0) {Serial.print(len);Serial.print(F(" "));Serial.print(idkey);}
    if (debugON>0) {Serial.print(F(" , key: "));Serial.println(key);Serial.flush();}
  msg_SEND(idkey,key,10); //highest power - 
}
  
 //*****************************************
void key_EE_ERASE() { Serial.println(F("erasing EE_KEY..."));Serial.flush();
  for (byte i=0;i<33;i++) { EEPROM.write(EE_KEY-i,255); } //the rest get FF's
}

//*****************************************
String key_EE_GET() { 
    if (debugON>0){Serial.println(F("key_EE_GET..."));Serial.flush();}
  char cKey[33];  byte i=0;  
 i=0;  cKey[i]=EEPROM.read(EE_KEY); // this way includes the null at end
  //Serial.print(F("cKey[")); Serial.print(i);Serial.print(F("]="));Serial.println(cKey[i],HEX);
  while ((cKey[i]!=0) && (i<32)) {
    i++; //assign first, check later
    cKey[i]=EEPROM.read(EE_KEY-i); 
    //Serial.print(F("cKey[")); Serial.print(i);Serial.print(F("]="));Serial.println(cKey[i],HEX);
  }
  cKey[i]=0; //term-truncate it again?
  return String(cKey); 
}

//*****************************************
void key_EE_SET(String &key) {
    if (debugON>0) {Serial.println(F("key_EE_SET..."));Serial.flush();} // and blinks Blue LED
  byte len=key.length();
  for (byte i=0;i<len;i++) {EEPROM.write(EE_KEY-i,key[i]);}
  EEPROM.write(EE_KEY-len,0);
  led_PAIR_BLINK(3,5,5); //5*10mS=50mS
}

//*****************************************
String key_NEW() {
    if (debugON>0) {Serial.println(F("key_NEW..."));Serial.flush();}
  char key[18];
  word rs=analogRead(1); rs=rs+analogRead(2); rs=rs+analogRead(3);
  rs=rs+analogRead(4); rs=rs+analogRead(4); randomSeed(rs);
  for (byte i=0;i<16;i++) { key[i]=random(34,126); } 
  key[16]=0;
  return String(key);  
} 

//*****************************************
void id_EE_ERASE(byte SBN) {
    if (debugON>0){Serial.println(F("id_EE_ERASE..."));Serial.flush();}
  word idLoc=(EE_ID-(SBN*6));
  for (byte i=0;i<6;i++) { EEPROM.write(idLoc-i,255); } //the rest get FF's
}
  
//*****************************************
void id_MAKE(byte SBN) {
  if (debugON>0){ Serial.println(F("id_MAKE: ")); }
  byte idbyte; word idLoc=(EE_ID-(SBN*6));
  for (byte i=0;i<6;i++) { idbyte=EEPROM.read(idLoc-i);
    if (((idbyte<'2')||(idbyte>'Z'))|| ((idbyte>'9')&&(idbyte<'A'))) {
      word rs=analogRead(1); rs=rs*analogRead(2); rs=rs*analogRead(3);
      rs=rs*analogRead(4);rs=rs*analogRead(5);randomSeed(rs);
      String idchar="23456789ABCDEFGHJKMNRSTUVWXYZ"; //29, 0-28
      for (byte i=0;i<6;i++) { byte id=idchar[random(0,29)];
        EEPROM.write(idLoc-i ,id);
          if (debugON>0) {Serial.print(char(id));}
      }
      if (debugON>0) { Serial.println(""); Serial.flush(); break;}
    }
  }
}

//*****************************************
String id_GET(byte SBN) {
    if (debugON>0){Serial.print(F("id_GET: ")); }
  char id[7]; word idLoc=(EE_ID-(SBN*6));
   for (byte i=0;i<6;i++) { id[i]=EEPROM.read(idLoc-i);
    if (debugON>0) {Serial.print(char(id[i]));}
   }
    if (debugON>0) {Serial.println("");Serial.flush();}
   id[6]=0; //string term
   return String(id);
}
