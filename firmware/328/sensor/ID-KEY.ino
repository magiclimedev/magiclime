
//*****************************************
char *key_REQUEST(char *rxkey, char* TxId, byte rssREF) { char *ret=rxkey;
  rxkey[0]=0; //prep for fail
  //digitalWrite(pinLED_BOOT, HIGH); delay(1000);
  char keyTEMP[18];

  key_NEW(keyTEMP); //returns new key in keyTEMP
  //print_CHR(keyTEMP,16);
  key_TXID_SEND(TxId,keyTEMP); //expect a response = 'keyTEMP' encoded 'ID:RX-KEY'
  char msg[64]; char id[8];
  rx_LOOK(msg,keyTEMP,25);
  //Serial.print(F("keylook: "));Serial.println(msg);Serial.flush(); 
  if (msg[0] !=0) { 
    if (msg[6]==':') { //not a fail-to-find...
      mySubStr(id,msg,0,6); //ID
      if (strcmp(id,TxId)==0) { //does this id match this sensors' ID?
        mySubStr(rxkey,msg,7,16);}
        //Serial.print(F("rxkey: "));Serial.println(rxkey);Serial.flush(); 
    }
  }
  //digitalWrite(pinLED_BOOT,LOW); 
  return ret;
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
  char rxkey[18]; char idkey[26];
  strcpy(rxkey,"thisisamagiclime");
  strcpy(idkey,"!"); strcat(idkey,txid); strcat(idkey,"!"); strcat(idkey,keyTEMP);
  msg_SEND(idkey,rxkey,1); 
}
  
//*****************************************
char *key_EE_GET(char *keyOUT) { char *ret=keyOUT;
  byte i=0;  
  for (i=0;i<16;i++){ keyOUT[i]=EEPROM.read(EE_KEY-i); }
  keyOUT[16]=0; //...and terminate
  return ret; 
}

//*****************************************
void key_EE_SET(char *key) {
  byte lenKEY=strlen(key);
  for (byte i=0;i<lenKEY;i++) {EEPROM.write(EE_KEY-i,key[i]);}
}

//*****************************************
char *key_NEW(char *key) { char *ret=key;
  word rs=analogRead(1); rs=rs+analogRead(2); rs=rs+analogRead(3);
  rs=rs+analogRead(4); rs=rs+analogRead(4); randomSeed(rs);
  for (byte i=0;i<16;i++) { key[i]=random(34,126); } 
  key[16]=0; //?why??
  return ret;
} 
  
//*****************************************
void id_MAKEifBAD(int sbn) { sbn++; //to make -1 = 0
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
    }
  }
}

//*****************************************
char *id_GET(char *idOUT, int sbn) { char *ret=idOUT; sbn++; //to make -1 = 0
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
