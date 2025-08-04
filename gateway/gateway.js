#!/usr/bin/env node

/**
 * MagicLime Gateway
 * 
 * Pure data collector that receives sensor data from serial port
 * and forwards it to web server via HTTP API
 */

const os = require("os");
const axios = require("axios");

/* Local imports */
const lib = require("./lib.js");
const serial = require("./serial.js");
const database = require("./db.js"); // Legacy database for local storage

// Configuration
const args = process.argv.slice(2);
const isLocalMode = args.includes('--local');
const isCloudMode = args.includes('--cloud');

// Default to local mode if no flags specified
const mode = isCloudMode ? 'cloud' : 'local';

const WEB_SERVER_URLS = {
  local: 'http://localhost:3000',
  cloud: 'https://magiclime.io'
};

const webServerUrl = WEB_SERVER_URLS[mode];

// Gateway state
let gatewaySerialNum = null;
let dataQueue = [];
let isProcessingQueue = false;

// Check permissions on Linux
if (os.platform() === "linux") {
  if (process.getuid() !== 0){
    console.log("\n⚠️  Serial port access requires appropriate permissions.");
    console.log("   Option 1: Add your user to the 'dialout' group:");
    console.log("   sudo usermod -a -G dialout $USER");
    console.log("   Then logout and login again.");
    console.log("\n   Option 2: Run with sudo (not recommended for production)\n");
  }
}

/**
 * Generate unique gateway serial number
 */
function generateUniqueSerialNum() {
  return Math.random().toString(36).substring(2, 8).toUpperCase();
}

/**
 * Initialize gateway
 */
async function initializeGateway() {
  try {
    console.log('🔧 Initializing MagicLime Gateway...');
    
    // Initialize legacy database for local storage
    database.initialize("gateway.db");
    
    // Get or create gateway serial number from file
    const fs = require('fs');
    const serialFilePath = './gateway-serial.txt';
    
    try {
      if (fs.existsSync(serialFilePath)) {
        gatewaySerialNum = fs.readFileSync(serialFilePath, 'utf8').trim();
        console.log(`🆔 Gateway Serial: ${gatewaySerialNum}`);
      } else {
        gatewaySerialNum = generateUniqueSerialNum();
        fs.writeFileSync(serialFilePath, gatewaySerialNum);
        console.log(`🆔 Generated new Gateway Serial: ${gatewaySerialNum}`);
      }
    } catch (fileError) {
      console.error("Error handling serial file:", fileError);
      gatewaySerialNum = generateUniqueSerialNum();
    }
    
    console.log(`📊 Mode: ${mode.toUpperCase()}`);
    console.log(`🌐 Web Server: ${webServerUrl}`);
    console.log(`🎯 Dashboard URL: ${webServerUrl}/${gatewaySerialNum}`);
    console.log('');
    
    return true;
  } catch (error) {
    console.error("❌ Failed to initialize gateway:", error.message);
    return false;
  }
}

/**
 * Forward sensor data to web server
 */
async function forwardSensorData(sensorData) {
  try {
    const url = `${webServerUrl}/api/data/${gatewaySerialNum}`;
    
    const response = await axios.post(url, sensorData, {
      timeout: 10000,
      headers: {
        'Content-Type': 'application/json',
        'User-Agent': `MagicLime-Gateway/${gatewaySerialNum}`
      }
    });
    
    if (response.data.success) {
      console.log(`✅ Data forwarded: ${sensorData.uid} -> ${sensorData.data}`);
      return true;
    } else {
      console.error(`❌ Server rejected data:`, response.data);
      return false;
    }
    
  } catch (error) {
    if (error.code === 'ECONNREFUSED') {
      console.error(`🔌 Cannot connect to web server at ${webServerUrl}`);
      console.error(`   Make sure the web server is running!`);
    } else if (error.code === 'ETIMEDOUT') {
      console.error(`⏱️  Timeout forwarding data to ${webServerUrl}`);
    } else {
      console.error(`❌ Error forwarding data:`, error.message);
    }
    
    // Add to retry queue
    addToRetryQueue(sensorData);
    return false;
  }
}

/**
 * Forward receiver status to web server
 */
async function forwardReceiverStatus(status) {
  try {
    const url = `${webServerUrl}/api/receiver-status/${gatewaySerialNum}`;
    
    const statusData = {
      status: status, // 'connected' or 'disconnected'
      timestamp: Date.now()
    };
    
    const response = await axios.post(url, statusData, {
      timeout: 5000,
      headers: {
        'Content-Type': 'application/json',
        'User-Agent': `MagicLime-Gateway/${gatewaySerialNum}`
      }
    });
    
    console.log(`📡 Receiver status forwarded: ${status}`);
    return true;
    
  } catch (error) {
    console.error(`❌ Error forwarding receiver status:`, error.message);
    return false;
  }
}

/**
 * Add failed data to retry queue
 */
function addToRetryQueue(sensorData) {
  dataQueue.push({
    ...sensorData,
    retryCount: (sensorData.retryCount || 0) + 1,
    timestamp: Date.now()
  });
  
  // Limit queue size
  if (dataQueue.length > 1000) {
    dataQueue.shift(); // Remove oldest
  }
}

/**
 * Process retry queue
 */
async function processRetryQueue() {
  if (isProcessingQueue || dataQueue.length === 0) {
    return;
  }
  
  isProcessingQueue = true;
  
  const now = Date.now();
  const retryData = [];
  
  // Find items ready for retry (older than 30 seconds)
  for (let i = dataQueue.length - 1; i >= 0; i--) {
    const item = dataQueue[i];
    
    // Remove items older than 1 hour or with too many retries
    if (now - item.timestamp > 3600000 || item.retryCount > 5) {
      dataQueue.splice(i, 1);
      continue;
    }
    
    // Retry items older than 30 seconds
    if (now - item.timestamp > 30000) {
      retryData.push(item);
      dataQueue.splice(i, 1);
    }
  }
  
  // Process retry items
  for (const item of retryData) {
    const success = await forwardSensorData(item);
    if (!success) {
      console.log(`🔄 Retry ${item.retryCount}/5 failed for sensor ${item.uid}`);
    }
  }
  
  if (dataQueue.length > 0) {
    console.log(`📋 Queue: ${dataQueue.length} items pending retry`);
  }
  
  isProcessingQueue = false;
}

/**
 * Handle incoming serial data
 */
async function handleSerialDataIn(incoming) {
  try {
    // Remove newline
    if (incoming.charAt(incoming.length - 1) === "\r") {
      incoming = incoming.slice(0, -1);
    }
      
    // Parse JSON
    let obj;
    try {
      obj = JSON.parse(incoming);
    } catch(e) {
      console.error("❌ Invalid JSON received:", incoming);
      return;
    }
    
    // Validate data
    if (obj.source !== "tx" || !obj.uid) {
      console.log("ℹ️  Ignoring non-sensor data:", obj);
      return;
    }
    
    // Transform data format - combine data1, data2 into data object
    let sensorData = obj.data;
    if (obj.data1 || obj.data2) {
      sensorData = {
        value1: obj.data1,
        value2: obj.data2,
        temperature: obj.data1, // Assume data1 is temperature for T-H sensors
        humidity: obj.data2     // Assume data2 is humidity for T-H sensors
      };
    }
    
    // Add timestamp
    const now = Math.round(Date.now() / 1000);
    obj.timestamp = lib.convertTimestamp(now);
    
    console.log(`📡 Received: ${obj.uid} (${obj.label || obj.sensor || 'unknown'}) -> ${JSON.stringify(sensorData)}`);
    
    // Forward to web server
    const forwardData = {
      uid: obj.uid,
      type: obj.label || obj.sensor || 'unknown',
      rss: obj.rss,
      bat: obj.bat,
      data: sensorData
    };
    
    await forwardSensorData(forwardData);
    
    // Also store locally for backup/debugging
    try {
      database.log(obj);
    } catch (dbError) {
      console.error("⚠️  Local database error:", dbError.message);
    }
    
  } catch (error) {
    console.error("❌ Error processing sensor data:", error);
  }
}

/**
 * Main function
 */
async function main() {
  try {
    // Initialize gateway
    const ready = await initializeGateway();
    if (!ready) {
      console.error("❌ Gateway initialization failed. Exiting.");
      process.exit(1);
    }
    
    // Set up data queue
    var Queue = require("queue-fifo");
    global.rxQueue = new Queue();
    
    // Make receiver status forwarding available globally
    global.forwardReceiverStatus = forwardReceiverStatus;
    
    // Process incoming data
    setInterval(() => {
      if (!global.rxQueue.isEmpty()) {
        const incoming = global.rxQueue.dequeue();
        handleSerialDataIn(incoming);
      }
    }, 100);
    
    // Process retry queue
    setInterval(processRetryQueue, 30000); // Every 30 seconds
    
    console.log('🚀 Gateway started successfully!');
    console.log('📡 Listening for sensor data...');
    console.log('');
    
  } catch (error) {
    console.error("❌ Error in main:", error);
    process.exit(1);
  }
}

// Graceful shutdown
process.on('SIGTERM', () => {
  console.log('🛑 Received SIGTERM, shutting down gracefully...');
  console.log('📦 Gateway shutdown complete');
  process.exit(0);
});

process.on('SIGINT', () => {
  console.log('🛑 Received SIGINT, shutting down gracefully...');
  console.log('📦 Gateway shutdown complete');
  process.exit(0);
});

// Handle uncaught exceptions
process.on('uncaughtException', (error) => {
  console.error('💥 Uncaught Exception:', error);
  process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
  console.error('💥 Unhandled Rejection at:', promise, 'reason:', reason);
  process.exit(1);
});

// Start the gateway
main();