/* Copyright (C) 2022 Marlyn Anderson - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the ...
 *
 * You should have received a copy of the ... with
 * this file. If not, please write to: , or visit :
 * magiclime.com
 */
 const static char VER[] = "TX221121";

#include "radio_sensor.h"

const int wdmTXI = 1; //WatchDogMultiplier for TX data Interval
//wdmTXI=1 means (1*8) sec. per 'unit' of txINTERVAL.
const int wdmHBP = 16; //WatchDogMultiplier for Heartbeat period 
//wdmHBP=16 means (16*8) sec. per 'unit' of txHEARTBEAT.
const int defaultINTERVAL = 16;       // periodic TX interval 8  X 16 sec = 128 sec.
const int defaultHEARTBEAT = 113; //'I'm still alive' period 113 x 128 sec = 4 hour (approx)

//*****************************************
void setup () { 
//SBN -> Sensor Board Number 1-21. A '-1' is 'no board', 0 is A6 pin grounded, 22 is A6 tied to Aref.
  SBN=255; //3;//255; //set to '255' to get SBN via resistors.
// otherwise, SBN will be the one you specify here
  init_SETUP();
}
//*****************************************

//*****************************************
void loop () {

  if (wakeWHY!=0) {//1 is Data, 2 is Heartbeat
    if (digitalRead(pinBOOST)==0){boost_ON();}
    get_DATA(txDATA,SBN,wakeWHY);
     
    //(int sbn, char *id, double bv, char *key, char *data, int pwr, int wait) 
    packet_SEND(SBN,txID,txBV,rxKEY,txDATA,txPWR);
    trigger_RESET(SBN); //mostly because Motion chip E931 needs this.
    wakeWHY=0; 
  }
  //sleepStuff();
  systemSleep(); 

} //End Of Loop ****************************
//*****************************************
