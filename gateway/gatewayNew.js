


/*
 * gateway.js
 */
const os = require("os");

const { mainModule } = require("process");
const axios = require("axios");

/* Local imports */
const lib = require("./lib.js");
const database = require("./db.js");
const mqttbroker = require("./mqttbroker.js");
const mqttclient = require("./mqttclients.js");
const websocket = require("./web/websocket.js");
const server = require("./web/webserver.js");
const serial = require("./serial.js");
const { Console } = require("console");
const db = require("better-sqlite3");
const express = require("express");
const ejs = require("ejs");
//const open = require('open');
var ip = require("ip");
const { subscribeToMQTT, publishAllSensors,publishSensor, publishSensorLog } = require("./mqttclient.js");
const { getAllSensors } = require("./db.js");

if (os.platform() === "linux") {
  if (process.getuid()){
    console.log("Please run as sudo. Exiting.");
    process.exit(1);
  }
}

database.initialize("gateway.db");
// Launch default browser
//open(`http://${ip.address()}`);

// Main loop
main()

async function main() {
  var Queue = require("queue-fifo");
  global.rxQueue = new Queue();
  setInterval(checkRXQueue, 100);
  mqttclient.checkMQTTServers();
  //publishing sensors and sensor details to MQTT
  publishAllSensors()
  const allSensors = getAllSensors()
  allSensors.forEach(({uid})=>{
    publishSensor(uid);
    publishSensorLog(uid)
  })


  function checkRXQueue() {
    if (!rxQueue.isEmpty()) {
      var incoming = rxQueue.dequeue();

      handleSerialDataIn(incoming);
    }
  }

  /*
   * Serial port data handler
   *
   */
  function handleSerialDataIn(incoming) {
    // Remove newline
    if (incoming.charAt(incoming.length - 1) === "\r") incoming = incoming.slice(0, -1);
      let obj = JSON.parse(incoming);

      var now = Math.round(Date.now()/1000);
      obj.timestamp = lib.convertTimestamp(now);
      
      logToConsole(JSON.stringify(obj));
      if (obj.source === "tx" && obj.data){
        database.log(obj);
        forwardToEndpoints(obj);
      }  
  }

  function forwardToEndpoints(obj){
    
    mqttclient.forwardToMQTT(obj);

    obj.channel = "LOG_DATA";
    websocket.wsBroadcast(JSON.stringify(obj));
  }

  /*
   * Log sensor data to console, and web
   *
   */
  function logToConsole(str) {
    console.log(str);
  }
}


