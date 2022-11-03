
#include "radio_sensor.h"

const int INTERVAL_DATA = 8;       // 8  X 16 sec = 124 sec.
const int INTERVAL_HEARTBEAT = 113; // 113 x 64 sec = 2 hour (approx) 

//*****************************************
void setup () { 
//SBN -> Sensor Board Number...
  SBN=255; //3;//255; //set to '255' to get SBN via resistors.
// otherwise, SBN will be the one you specify here
  init_SETUP();

}
//*****************************************

//*****************************************
void loop () {

  if (sendWHY!=0) {//1 is Data, 2 is Heartbeat
    if (digitalRead(pinBOOST)==0){boost_ON();}
     get_DATA(txDATA,SBN,sendWHY);
    //(byte sbn, char *id, double bv, char *key, char *data, int pwr, int wait) {
    packet_SEND(SBN,txID,txBV,rxKEY,txDATA,txPWR,0);
    trigger_RESET(SBN); //mostly because Motion chip E931 needs this.
    sendWHY=0; 
  }
  //sleepStuff();
  systemSleep(); 
  
} //End Of Loop ****************************
//*****************************************
