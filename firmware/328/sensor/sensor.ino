
#include "radio_sensor.h"

const int INTERVAL_DATA = 8;       // 8  X 16 sec = 124 sec.
const int INTERVAL_HEARTBEAT = 113; // 113 x 64 sec = 2 hour (approx) 

//*****************************************
void setup () { 
//Sensor Board Number...
  //randomSeed(analogRead(5));
  SBN=255; //3;//255; //set to '255' to get SBN via resistors.
// otherwise, SBN will be the one you specify here
  init_SETUP();
}
//*****************************************

//*****************************************
void loop () {
  if (sendDATA!=0) {//1 is Data, 2 is Heartbeat
    if (digitalRead(pinBOOST)==0){boost_ON();}
      if (debugON>0) {Serial.print(F("\n\n*sendDATA=")); Serial.print(sendDATA);}
    /*if (SBN==1) {
      packet_SEND(SBN,get_DATA(20,sendDATA),0);
      packet_SEND(SBN,get_DATA(21,sendDATA),0);
      packet_SEND(SBN,get_DATA(22,sendDATA),0);
      packet_SEND(SBN,get_DATA(23,sendDATA),0);
      packet_SEND(SBN,get_DATA(24,sendDATA),0);
      packet_SEND(SBN,get_DATA(25,sendDATA),0);
      packet_SEND(SBN,get_DATA(26,sendDATA),0);
      packet_SEND(SBN,get_DATA(27,sendDATA),0);
      packet_SEND(SBN,get_DATA(28,sendDATA),0);
      packet_SEND(SBN,get_DATA(29,sendDATA),0);
    }
    else {packet_SEND(SBN,get_DATA(SBN,sendDATA),0);}
    */
    packet_SEND(SBN,get_DATA(SBN,sendDATA),0);
    trigger_RESET(SBN); //mostly because Motion chip E931 needs this.
    sendDATA=0; 
  }
  //sleepStuff();
  systemSleep(); 
   
}//End Of Loop ****************************
//*****************************************
