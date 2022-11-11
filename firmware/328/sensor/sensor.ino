/* Copyright (C) 2022 Marlyn Anderson - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the ...
 *
 * You should have received a copy of the ... with
 * this file. If not, please write to: , or visit :
 * magiclime.com
 */
 
#include "radio_sensor.h"

const int defaultINTERVAL = 8;       // periodic TX interval 8  X 16 sec = 124 sec.
const int defaultHEARTBEAT = 113; //'I'm still alive' period 113 x 64 sec = 2 hour (approx) 

//*****************************************
void setup () { 
//SBN -> Sensor Board Number 1-21..-1 is 'no board', 0 is A6 pin grounded, 22 is A6 tied to Aref.
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
     
    //(byte sbn, char *id, double bv, char *key, char *data, int pwr, int wait) 
    packet_SEND(SBN,txID,txBV,rxKEY,txDATA,txPWR);
    trigger_RESET(SBN); //mostly because Motion chip E931 needs this.
    sendWHY=0; 
  }
  //sleepStuff();
  systemSleep(); 
  //if (txCOUNTER % 32 ==0) {Serial.println("");}
  //Serial.print(F(":"));Serial.print(sendWHY);Serial.flush();

} //End Of Loop ****************************
//*****************************************
