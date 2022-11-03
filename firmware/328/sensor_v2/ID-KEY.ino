
//*****************************************
char *key_REQUEST(char *key, char* TxId, byte rssREF) { char *ret=key;
  key[0]=0; //prep for fail
  //Serial.print(F("...key_REQUEST... "));Serial.flush();
  digitalWrite(pinPAIR_LED, HIGH); delay(1000);
  char keyTEMP[18];

  key_NEW(keyTEMP); //returns new key in keyTEMP
  print_CHR(keyTEMP,16);
  key_TXID_SEND(TxId,keyTEMP); //expect a response = 'keyTEMP' encoded 'ID:PrivateKey'

  byte timeout=0;
  while (!rf95.available() && timeout<250) { delay(10); timeout++; }
  //Serial.print(F("timeout(<250): "));Serial.println(timeout);Serial.flush();
  if (timeout==250) { digitalWrite(pinPAIR_LED,LOW);  return ret;}
  
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  if (rf95.recv(buf, &len)) {
    if ((rf95.lastRssi()+150)>=rssREF) { 
      char msg[64]; char id[8];
      msg_GET(msg,buf,len,keyTEMP); //decode with key sent
      if (msg[0] !=0) { byte i=0; 
        while ((msg[i]!=0)&&(msg[i]!=':')) {i++;}
        if (msg[i]==':') { //not a fail-to-find...
          mySubStr(id,msg,0,i); //ID
          if (strcmp(id,TxId)==0) { //does this id match this sensors' ID?
            mySubStr(key,msg,i,len-i-1);} 
        }
      }
    }
  }
  digitalWrite(pinPAIR_LED,LOW); 
  return key;
}

//*****************************************
bool key_VALIDATE(char *key2VAL) { //check EEPROM for proper character range
  byte lenKEY=strlen(key2VAL);
  boolean ret=true;
  if (lenKEY==0) {ret=false;}
  for (byte i=0;i<lenKEY;i++) { //Serial.print(key[i]);Serial.print(F(" ")); 
    if ((key2VAL[i]<34) || (key2VAL[i]>126)|| (lenKEY<4)) { ret=false; break; }
  }
    return ret;
}

//*****************************************
void key_TXID_SEND(char *txid, char* keyTEMP) {
  //Serial.println(F("key_TXID_SEND... "));Serial.flush();
  char key[18]; char idkey[26];
  strcpy(key,"thisisamagiclime");
  strcpy(idkey,"!"); strcat(idkey,txid); strcat(idkey,"!"); strcat(idkey,keyTEMP);
  msg_SEND(idkey,key,1); 
}
  
//*****************************************
char *key_EE_GET(char *keyOUT) { char *ret=keyOUT;
  //Serial.println(F("...key_EE_GET"));Serial.flush();
  byte i=0;  
  for (i=0;i<16;i++){ keyOUT[i]=EEPROM.read(EE_KEY-i); }
  keyOUT[16]=0; //...and terminate
  return ret; 
}

//*****************************************
void key_EE_SET(char *key) {
  //Serial.println(F("...key_EE_SET"));Serial.flush(); // and blinks Blue LED
  byte lenKEY=strlen(key);
  for (byte i=0;i<lenKEY;i++) {EEPROM.write(EE_KEY-i,key[i]);}
  led_PAIR_BLINK(3,5,5); //5*10mS=50mS
}

//*****************************************
char *key_NEW(char *key) { char *ret=key;
  //Serial.println(F("...key_NEW..."));Serial.flush();
  word rs=analogRead(1); rs=rs+analogRead(2); rs=rs+analogRead(3);
  rs=rs+analogRead(4); rs=rs+analogRead(4); randomSeed(rs);
  for (byte i=0;i<16;i++) { key[i]=random(34,126); } 
  key[16]=0; //?why??
  return ret;
} 
  
//*****************************************
void id_MAKEifBAD(byte sbn) {
  //Serial.println(F("id_MAKE: ")); Serial.flush();
  byte idbyte; word idLoc=(EE_ID-(sbn*6)); bool badID=false;
  for (byte i=0;i<6;i++) { idbyte=EEPROM.read(idLoc-i);
    if (((idbyte<'2')||(idbyte>'Z'))|| ((idbyte>'9')&&(idbyte<'A'))) {
      badID=true; break;
    }
  }
  if (badID==true) { const char idchar[] ="23456789ABCDEFGHJKMNRSTUVWXYZ"; //29, 0-28
    word rs=analogRead(1); rs=rs*analogRead(2); rs=rs*analogRead(3);
    rs=rs*analogRead(4);rs=rs*analogRead(5);randomSeed(rs);
    for (byte j=0;j<6;j++) { byte id=idchar[random(0,29)];
      EEPROM.write(idLoc-j ,id);
      //if (debugON>0) {Serial.print(char(id));Serial.flush();}
    }
       //if (debugON>0) { Serial.println(""); Serial.flush();}
  }
}

//*****************************************
char *id_GET(char *idOUT, byte sbn) { char *ret=idOUT;
  //if (debugON>0){Serial.print(F("id_GET: ")); Serial.flush();}
  word idLoc=(EE_ID-(sbn*6));
  for (byte i=0;i<6;i++) { idOUT[i]=EEPROM.read(idLoc-i);
  }
  idOUT[6]=0; //string term
  return ret;
}

//*****************************************
char *mySubStr(char *out, char* in,byte from,byte len) { char *ret=out;
  byte p=0; 
  for (byte i=from;i<(from+len);i++) {out[p]=in[i]; p++;}
  out[p]=0;
  return ret;
}
