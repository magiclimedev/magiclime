/*
 *  This is code for ThingBits sensors.
 *
 *  ThingBits invests time and resources providing this open source code,
 *  please support ThingBits and open-source hardware by purchasing products
 *  from ThingBits!
 *
 *  Written by ThingBits Inc (http://www.thingbits.com)
 *
 *  MIT license, all text above must be included in any redistribution
 */
const static char VER[] = "TX221214";
#include "radio_sensor.h"

//*****************************************
void setup () { 
  SBN=255; //set to '255' to get Sensor Board Number via resistors.
// otherwise, SBN will be the one you specify here
  init_SETUP();
}
//*****************************************

//*****************************************
void loop () {
  //Serial.println(wd_COUNTER);Serial.flush();
  if (wakeWHY!=0) {//1 is Data, 2 is Heartbeat
    get_DATA(txDATA,SBN,wakeWHY);
    packet_SEND(SBN,txID,txBV,rxKEY,txDATA,txPWR);
    trigger_RESET(SBN); 
    wakeWHY=0; 
  }
  systemSleep(); 

} //End Of Loop ****************************
//*****************************************
