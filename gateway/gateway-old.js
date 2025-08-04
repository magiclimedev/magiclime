/*
 * gateway.js
 */
const os = require("os");

const { mainModule } = require("process");
const axios = require("axios");

/* Local imports */
const lib = require("./lib.js");
const models = require("./models");
const database = require("./db.js"); // Keep for backward compatibility
const mqttbroker = require("./mqttbroker.js");
const MQTTClientList = require("./mqttclient.js");
const {Settings} = require("./Settings") // Keep for backward compatibility
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
const { getAllSensors,getAllSettings } = require("./db.js"); // Keep for backward compatibility

if (os.platform() === "linux") {
  if (process.getuid() !== 0){
    console.log("\n⚠️  Serial port access requires appropriate permissions.");
    console.log("   Option 1: Add your user to the 'dialout' group:");
    console.log("   sudo usermod -a -G dialout $USER");
    console.log("   Then logout and login again.");
    console.log("\n   Option 2: Run with sudo (not recommended for production)\n");
  }
}

// Initialize Sequelize models
let gatewaySerialNum = null;
let currentGateway = null;

async function initializeSequelize() {
  try {
    // Sync database schema
    await models.sequelize.sync({ alter: true });
    
    // Ensure default settings
    await models.Setting.ensureDefaults();
    
    // Get or create gateway serial number
    gatewaySerialNum = await models.Setting.getGlobal('gateway_serial_num');
    if (!gatewaySerialNum) {
      gatewaySerialNum = await models.Gateway.generateUniqueSerialNum();
      await models.Setting.setGlobal('gateway_serial_num', gatewaySerialNum);
      currentGateway = await models.Gateway.create({ 
        serial_num: gatewaySerialNum,
        name: `Gateway ${gatewaySerialNum}`,
        last_seen: new Date()
      });
      console.log(`\n🆔 Generated new Gateway Serial: ${gatewaySerialNum}`);
    } else {
      currentGateway = await models.Gateway.findOrCreateBySerialNum(gatewaySerialNum);
      console.log(`\n🆔 Gateway Serial: ${gatewaySerialNum}`);
    }
    
    console.log(`🌐 Cloud Dashboard: ${models.Gateway.getCloudUrl(gatewaySerialNum)}`);
    console.log(`🏠 Local Dashboard: http://${ip.address()}/\n`);
    
    return true;
  } catch (error) {
    console.error("Failed to initialize Sequelize:", error.message);
    return false;
  }
}

// Keep backward compatibility
try {
  database.initialize("gateway.db");
} catch (error) {
  console.error("Warning: Legacy database initialization failed:", error.message);
}
// Launch default browser
//open(`http://${ip.address()}`);
// This is an object because it will be exported and will need to be a reference, not a value, for imports.
const clientObj = {mqttClientList:null}
// Main loop
main()

async function main() {
  try {
    // Initialize Sequelize first
    const sequelizeReady = await initializeSequelize();
    if (!sequelizeReady) {
      console.error("Failed to initialize database. Exiting.");
      process.exit(1);
    }
    
    clientObj.mqttClientList = new MQTTClientList()
    const {mqttClientList} = clientObj;
    var Queue = require("queue-fifo");
    global.rxQueue = new Queue();
    setInterval(checkRXQueue, 100);
    
    await addMQTTEndpoints();
    await mqttClientList.checkMQTTServers();
    
    //publishing sensors and sensor details to MQTT
    await mqttClientList.publishAllSensors();
    
    // Get sensors using Sequelize if gateway exists
    if (currentGateway) {
      const allSensors = await models.Sensor.getAllByGateway(currentGateway.id);
      allSensors.forEach((sensor) => {
        mqttClientList.publishSensor(sensor.uid);
        mqttClientList.publishSensorLog(sensor.uid);
      });
    }
    
    // Keep backward compatibility
    const allSensorsLegacy = getAllSensors();
    if (allSensorsLegacy) {
      allSensorsLegacy.forEach(({uid})=>{
        mqttClientList.publishSensor(uid);
        mqttClientList.publishSensorLog(uid);
      });
    }
    
    // this sets default rows within the settings tables if they haven't been initialized
    await Settings.ensureInitialized();
  } catch (error) {
    console.error("Error in main initialization:", error);
    process.exit(1);
  }

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
  async function handleSerialDataIn(incoming) {
    var obj;

    // Remove newline
    if (incoming.charAt(incoming.length - 1) === "\r")
      incoming = incoming.slice(0, -1);

    try {
      obj = JSON.parse(incoming);
    } catch(e) {
      logToConsole("Invalid JSON received: " + incoming + " - Error: " + e.message);
      return;
    }

    var now = Math.round(Date.now()/1000);
    obj.timestamp = lib.convertTimestamp(now);

    logToConsole(JSON.stringify(obj));
    if (obj.source === "tx" && obj.data){
      try {
        // Log using both Sequelize and legacy database for compatibility
        database.log(obj);
        
        // Also log using Sequelize if we have a gateway
        if (currentGateway && obj.uid) {
          const sensor = await models.Sensor.findOrCreateByUid(
            currentGateway.id, 
            obj.uid, 
            obj.sensor || 'unknown'
          );
          
          await models.Sensor.logData(sensor.id, {
            rss: obj.rss,
            bat: obj.bat,
            data: obj.data
          });
          
          // Update gateway last seen
          await models.Gateway.updateLastSeen(gatewaySerialNum);
        }
        
        forwardToEndpoints(obj);
      } catch (error) {
        console.error("Error processing sensor data:", error);
      }
    }
  }

  function forwardToEndpoints(obj){
    try {
      mqttClientList.forwardToMQTT(obj);
    } catch (error) {
      console.error("Error forwarding to MQTT:", error);
    }

    try {
      obj.channel = "LOG_DATA";
      websocket.wsBroadcast(JSON.stringify(obj));
    } catch (error) {
      console.error("Error broadcasting to websocket:", error);
    }
  }

  //adds external mqtt if it has been set in the settings
  async function addMQTTEndpoints(){
    try {
      const settings = await Settings.getAll();
      if (settings.externalMqtt!=="true"){
        return;
      }
      
      // Validate MQTT IP before adding
      if (!settings.externalMqttIp || settings.externalMqttIp.trim() === '') {
        console.error("External MQTT enabled but no IP configured");
        return;
      }
      
      mqttClientList.add(settings.externalMqttIp);
    } catch (error) {
      console.error("Error adding MQTT endpoints:", error);
    }
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