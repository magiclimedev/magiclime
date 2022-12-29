
//*****************************************
char *key_REQUEST(char *rxkey, char* TxId, byte rssREF) { char *ret=rxkey;
  rxkey[0]=0; //prep for fail
  char keyTEMP[18];
  key_NEW(keyTEMP);
  //print_CHR(keyTEMP,16);
  key_TXID_SEND(TxId,keyTEMP);
  char msg[64]; char id[8];
  rx_LOOK(msg,keyTEMP,25);
  if (msg[0] !=0) {
    if (msg[6]==':') {
      mySubStr(id,msg,0,6);
      if (strcmp(id,TxId)==0) {
        mySubStr(rxkey,msg,7,16);}
    }
  }
  return ret;
}

//*****************************************
bool key_VALIDATE(char *key2VAL) {
  byte lenKEY=strlen(key2VAL);
  boolean ret=true;
  if (lenKEY<16) {ret=false;}
  for (byte i=0;i<lenKEY;i++) {
    if ((key2VAL[i]<36) || (key2VAL[i]>126)|| (lenKEY<4)) { ret=false; break; }
  }
    return ret;
}

//*****************************************
void key_TXID_SEND(char *txid, char* keyTEMP) {
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
  key[16]=0;
  return ret;
}

//*****************************************
char *id_GET(char *idOUT, int sbn) { char *ret=idOUT; sbn++;
  word idLoc=(EE_ID-(sbn*6)); bool badID=false;
  for (byte i=0;i<6;i++) { idOUT[i]=EEPROM.read(idLoc-i);
    //Serial.print(idOUT[i]);
    if (((idOUT[i]<'1')||(idOUT[i]>'Z'))|| ((idOUT[i]>'9')&&(idOUT[i]<'A'))) {
      id_NEW(idOUT,sbn);
      return ret;
    }
  }
  idOUT[6]=0;
  return ret;
}

//*****************************************
void id_NEW(char *idNEW, int sbn) {char *ret=idNEW;
  word idLoc=(EE_ID-(sbn*6));
  const char idchar[] ="123456789ABCDEFGHJLKMNRSTUVWXYZ"; //31, 0-30
  word rs=analogRead(1); rs=rs*analogRead(2); rs=rs*analogRead(3);
  rs=rs*analogRead(4);rs=rs*analogRead(5);randomSeed(rs);
  for (byte i=0;i<6;i++) {
    idNEW[i]=idchar[random(0,31)];
    EEPROM.write((idLoc-i) ,idNEW[i]);
  }
  idNEW[6]=0;
  return ret;
}

//*****************************************
char *mySubStr(char *out, char* in,byte from,byte len) { char *ret=out;
  byte p=0;
  for (byte i=from;i<(from+len);i++) {out[p]=in[i]; p++;}
  out[p]=0;
  return ret;
}
