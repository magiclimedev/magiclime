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
const MQTTClientList = require("./mqttclient.js");
const {Settings} = require("./Settings")
const websocket = require("./web/websocket.js");
const server = require("./web/webserver.js");
const serial = require("./serial.js");
const ipc = require("./ipc.js");
const { Console, log } = require("console");
const db = require("better-sqlite3");
const express = require("express");
const ejs = require("ejs");
//const open = require('open');
var ip = require("ip");
const { subscribeToMQTT, publishAllSensors,publishSensor, publishSensorLog } = require("./mqttclient.js");
const { getAllSensors,getAllSettings } = require("./db.js");

if (os.platform() === "linux") {
  if (process.getuid()){
    console.log("Please run as sudo. Exiting.");
    process.exit(1);
  }
}

database.initialize("gateway.db");
// Launch default browser
//open(`http://${ip.address()}`);
// This is an object because it will be exported and will need to be a reference, not a value, for imports.
const clientObj = {mqttClientList:null}
// Main loop
main()

async function main() {
  clientObj.mqttClientList = new MQTTClientList()
  const {mqttClientList} = clientObj;
  var Queue = require("queue-fifo");
  global.rxQueue = new Queue();
  setInterval(checkRXQueue, 100);
  addMQTTEndpoints()
  mqttClientList.checkMQTTServers();
  //publishing sensors and sensor details to MQTT
  mqttClientList.publishAllSensors()
  const allSensors = getAllSensors()
  allSensors.forEach(({uid})=>{
    mqttClientList.publishSensor(uid);
    mqttClientList.publishSensorLog(uid)
  })
  // this sets default rows within the settings tables if they haven't been initialized
  Settings.ensureInitialized();

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
    var obj;

    // Remove newline
    if (incoming.charAt(incoming.length - 1) === "\r")
      incoming = incoming.slice(0, -1);

    try {
      obj = JSON.parse(incoming);
    }  catch(e) {
      logToConsole("Invalid JSON received: " + incoming);
      return;
    }

    var now = Math.round(Date.now()/1000);
    obj.timestamp = lib.convertTimestamp(now);

    logToConsole(JSON.stringify(obj));
    if (obj.source === "tx" && obj.data){
      database.log(obj);
      forwardToEndpoints(obj);
    }
  }

  function forwardToEndpoints(obj){
    
    mqttClientList.forwardToMQTT(obj);

    obj.channel = "LOG_DATA";
    websocket.wsBroadcast(JSON.stringify(obj));
  }

  //adds external mqtt if it has been set in the settings
  async function addMQTTEndpoints(){
    const settings = await Settings.getAll();
    if (settings.externalMqtt!=="true"){
      return
    }
    mqttClientList.add(settings.externalMqttIp);
  }
  /*
   * Log sensor data to console, and web
   *
   */
  function logToConsole(str) {
    console.log(str);
  }
}

module.exports = {clientObj}