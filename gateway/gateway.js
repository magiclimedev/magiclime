#!/usr/bin/env node

/**
 * MagicLime Gateway
 * 
 * IoT data collector that receives sensor data via serial port
 * and forwards it to web server via HTTP API
 */

const os = require('os');
const fs = require('fs');
const path = require('path');

// Load configuration first
const config = require('./config');
const logger = require('./logger');
const { errorHandler } = require('./errors');
const { validateSensorData } = require('./validation');

// Services
const database = require('./db');
const httpClient = require('./http-client');
const QueueManager = require('./queue-manager');
const GatewayAPI = require('./api-server');

// Gateway components  
const lib = require('./lib');

// Gateway state
class Gateway {
  constructor() {
    this.serialNum = null;
    this.queueManager = new QueueManager();
    this.isRunning = false;
    this.shutdownInProgress = false;
    this.apiServer = new GatewayAPI();
    
    // Processing intervals
    this.intervals = {
      retry: null,
      cleanup: null,
      stats: null
    };
  }
  
  /**
   * Initialize gateway
   */
  async initialize() {
    try {
      logger.info('Initializing MagicLime Gateway', {
        version: config.app.version,
        mode: config.app.mode
      });
      
      // Check permissions on Linux
      this.checkPermissions();
      
      // Initialize database
      database.initialize();
      
      // Start API server
      this.apiServer.initialize();
      
      // Get or create gateway serial number
      this.serialNum = await this.getGatewaySerial();
      
      // Setup queue event handlers
      this.setupQueueHandlers();
      
      // Setup global handlers for serial module
      this.setupGlobalHandlers();
      
      logger.info('Gateway initialized', {
        serial: this.serialNum,
        mode: config.app.mode,
        serverUrl: config.getServerUrl(),
        dashboardUrl: `${config.getServerUrl()}/${this.serialNum}`
      });
      
      return true;
      
    } catch (error) {
      logger.error('Failed to initialize gateway', { error: error.message });
      throw error;
    }
  }
  
  /**
   * Check system permissions
   */
  checkPermissions() {
    if (os.platform() === 'linux' && process.getuid() !== 0) {
      logger.warn('Serial port access may require permissions', {
        suggestion: 'Add user to dialout group: sudo usermod -a -G dialout $USER'
      });
    }
  }
  
  /**
   * Get or create gateway serial number
   */
  async getGatewaySerial() {
    try {
      const serialPath = path.resolve(config.app.serialFilePath);
      
      if (fs.existsSync(serialPath)) {
        const serial = fs.readFileSync(serialPath, 'utf8').trim();
        if (serial && /^[A-Z0-9]{6}$/.test(serial)) {
          return serial;
        }
      }
      
      // Generate new serial
      const serial = lib.uid();
      fs.writeFileSync(serialPath, serial);
      logger.info('Generated new gateway serial', { serial });
      
      return serial;
      
    } catch (error) {
      logger.error('Error handling serial file', { error: error.message });
      // Generate runtime serial as fallback
      return lib.uid();
    }
  }
  
  /**
   * Setup queue event handlers
   */
  setupQueueHandlers() {
    // Process incoming data immediately
    this.queueManager.on('incoming:ready', () => {
      setImmediate(() => this.processIncomingQueue());
    });
    
    // Process retry queue
    this.queueManager.on('retry:ready', () => {
      if (!this.intervals.retry) {
        this.processRetryQueue();
      }
    });
  }
  
  /**
   * Setup global handlers for serial module
   */
  setupGlobalHandlers() {
    // Serial data handler
    global.rxQueue = {
      isEmpty: () => this.queueManager.isEmpty('incoming'),
      dequeue: () => this.queueManager.dequeue('incoming'),
      enqueue: (data) => this.queueManager.enqueue(data, 'incoming')
    };
    
    // Receiver status handler
    global.forwardReceiverStatus = (status) => {
      this.forwardReceiverStatus(status).catch(error => {
        logger.error('Failed to forward receiver status', {
          error: error.message,
          status
        });
      });
    };
  }
  
  /**
   * Start gateway
   */
  async start() {
    try {
      this.isRunning = true;
      
      // Start serial module (it manages its own connection)
      require('./serial');
      
      // Setup processing intervals
      this.setupIntervals();
      
      // In local mode, start test data simulation
      if (process.argv.includes('--local')) {
        this.startTestDataSimulation();
      }
      
      // Setup shutdown handlers
      this.setupShutdownHandlers();
      
      // Send initial connected status to web server
      await this.forwardReceiverStatus('connected');
      
      logger.info('Gateway started successfully');
      
    } catch (error) {
      logger.error('Failed to start gateway', { error: error.message });
      throw error;
    }
  }
  
  /**
   * Setup processing intervals
   */
  setupIntervals() {
    // Process retry queue periodically
    this.intervals.retry = setInterval(() => {
      this.processRetryQueue();
    }, config.queue.retryDelay);
    
    // Cleanup old items periodically
    this.intervals.cleanup = setInterval(() => {
      const cleaned = this.queueManager.cleanup();
      if (cleaned > 0) {
        logger.debug('Queue cleanup completed', { removed: cleaned });
      }
    }, 300000); // 5 minutes
    
    // Log statistics periodically
    this.intervals.stats = setInterval(() => {
      this.logStatistics();
    }, 600000); // 10 minutes
  }
  
  /**
   * Process incoming data queue
   */
  async processIncomingQueue() {
    if (!this.isRunning || this.queueManager.processing.incoming) {
      return;
    }
    
    this.queueManager.processing.incoming = true;
    const startTime = Date.now();
    let processedCount = 0;
    
    try {
      while (!this.queueManager.isEmpty('incoming') && this.isRunning) {
        const data = this.queueManager.dequeue('incoming');
        if (data) {
          await this.handleSerialData(data);
          this.queueManager.stats.processed++;
          processedCount++;
        }
      }
      
      // Log processing statistics for potential data bursts
      if (processedCount > 5) {
        const processingTime = Date.now() - startTime;
        logger.info(`📊 Data burst processed: ${processedCount} packets in ${processingTime}ms`, {
          packetsProcessed: processedCount,
          processingTimeMs: processingTime,
          averageTimePerPacket: Math.round(processingTime / processedCount)
        });
      }
      
    } finally {
      this.queueManager.processing.incoming = false;
    }
  }
  
  /**
   * Process retry queue
   */
  async processRetryQueue() {
    if (!this.isRunning || this.queueManager.processing.retry) {
      return;
    }
    
    this.queueManager.processing.retry = true;
    
    try {
      const items = this.queueManager.getRetryItems();
      
      for (const item of items) {
        if (!this.isRunning) break;
        
        try {
          await this.forwardSensorData(item);
          this.queueManager.stats.retried++;
        } catch (error) {
          // Will be re-added to retry queue by forwardSensorData
        }
      }
      
      // Log queue status if items pending
      const retrySize = this.queueManager.size('retry');
      if (retrySize > 0) {
        logger.debug('Retry queue status', { pending: retrySize });
      }
      
    } finally {
      this.queueManager.processing.retry = false;
    }
  }
  
  /**
   * Handle incoming serial data
   */
  async handleSerialData(queueItem) {
    try {
      // Extract the actual data from queue item
      const rawData = queueItem.data || queueItem;
      
      // Parse JSON
      let obj;
      try {
        const jsonStr = rawData.toString().replace(/\r?\n$/, '');
        obj = JSON.parse(jsonStr);
        
        // Debug log to see exact data received
        logger.debug('Raw sensor data received', { 
          raw: jsonStr,
          parsed: obj 
        });
      } catch (error) {
        logger.error('Invalid JSON received', { data: rawData.toString() });
        return;
      }
      
      // Handle different data types
      if (obj.source === 'tx' && obj.uid) {
        // Sensor data
        await this.processSensorData(obj);
        
      } else if (obj.source === 'rx') {
        // Receiver status or info
        logger.info('Receiver info', obj);
        
      } else if (obj.source === 'rp') {
        // Repeater data - process as sensor data
        await this.processSensorData(obj);
        
      } else {
        logger.debug('Unknown data type', obj);
      }
      
    } catch (error) {
      errorHandler.handle(error, { context: 'handleSerialData' });
    }
  }
  
  /**
   * Process sensor data
   */
  async processSensorData(obj) {
    try {
      // Validate data
      const validated = validateSensorData(obj);
      
      // Add path information to all packets
      const enhancedPacket = {
        ...validated,
        _path: validated.source === 'rp' ? 'repeater' : 'direct',
        _pathIndicator: validated.source === 'rp' ? 'R' : 'D'
      };
      
      // Build forwarding object with path information
      const sensorData = this.buildSensorData(enhancedPacket);
      
      // Add the original raw packet for raw logs display
      sensorData.rawPacket = enhancedPacket;
      
      // Enhanced logging for sensor reboot data bursts
      const dataStr = this.formatDataString(enhancedPacket);
      const pathIndicator = enhancedPacket._pathIndicator || '';
      const pathInfo = enhancedPacket._path || 'unknown';
      
      // Check if this is a heartbeat packet (high RSS, typical values)
      const isHeartbeat = enhancedPacket.rss && Math.abs(enhancedPacket.rss) > 90;
      if (isHeartbeat) {
        // Log heartbeats at debug level to reduce console spam
        logger.debug(`💓 Heartbeat: ${enhancedPacket.uid} (${enhancedPacket.label || 'Unknown'}) [${pathIndicator}] RSS=${enhancedPacket.rss} -> ${dataStr}`);
      } else {
        logger.info(`📡 Received: ${enhancedPacket.uid} (${enhancedPacket.label || 'Unknown'}) [${pathIndicator}] -> ${dataStr}`);
      }
      
      // Log complete packet for debugging data bursts
      logger.debug('Complete packet data', { 
        uid: enhancedPacket.uid,
        rss: enhancedPacket.rss,
        path: pathInfo,
        rawPacket: enhancedPacket
      });
      
      // Forward ALL packets to server (no deduplication)
      await this.forwardSensorData(sensorData);
      
      // Log to local database with path information (raw JSON for raw logs)
      try {
        const dbData = {
          ...enhancedPacket,
          _lastPath: pathInfo
        };
        database.log(dbData);
      } catch (dbError) {
        logger.error('Local database error', { 
          error: dbError.message,
          uid: enhancedPacket.uid 
        });
      }
      
    } catch (error) {
      errorHandler.handle(error, { context: 'processSensorData', uid: obj.uid });
    }
  }
  
  /**
   * Build sensor data for forwarding
   */
  buildSensorData(obj) {
    const data = {
      uid: obj.uid,
      timestamp: Date.now(),
      data: {}
    };
    
    // Add data fields
    if (obj.data1 !== undefined) data.data.value1 = obj.data1;
    if (obj.data2 !== undefined) data.data.value2 = obj.data2;
    
    // Add metadata
    if (obj.rss !== undefined) data.rss = obj.rss;
    if (obj.bat !== undefined) data.bat = obj.bat;
    
    // Handle both 'label' and 'sensor' fields for type
    if (obj.label) {
      data.type = obj.label;
    } else if (obj.sensor) {
      data.type = obj.sensor;
      // Also set label for consistency
      obj.label = obj.sensor;
    }
    
    // Add path information
    if (obj._path) {
      data.path = obj._path;
      data.pathIndicator = obj._pathIndicator;
    }
    
    // Add semantic labels for known sensor types
    if (obj.label === 'T-H' && obj.data1 && obj.data2) {
      data.data.temperature = obj.data1;
      data.data.humidity = obj.data2;
    }
    
    return data;
  }
  
  /**
   * Format data string for logging
   */
  formatDataString(obj) {
    const data = {};
    
    if (obj.data1 !== undefined) data.value1 = obj.data1;
    if (obj.data2 !== undefined) data.value2 = obj.data2;
    
    if (obj.label === 'T-H' && obj.data1 && obj.data2) {
      data.temperature = obj.data1;
      data.humidity = obj.data2;
    }
    
    return JSON.stringify(data);
  }
  
  /**
   * Forward sensor data to server
   */
  async forwardSensorData(sensorData) {
    try {
      const url = `${config.getServerUrl()}/api/data/${this.serialNum}`;
      
      const response = await httpClient.post(url, sensorData);
      
      if (response.data.success) {
        // Only log non-reboot data to reduce console spam during bursts
        const isRebootData = sensorData.rss && Math.abs(sensorData.rss) > 80;
        if (!isRebootData) {
          logger.info(`Data forwarded: ${sensorData.uid} -> ${JSON.stringify(sensorData.data)}`);
        } else {
          logger.debug(`Reboot data forwarded: ${sensorData.uid} RSS=${sensorData.rss}`, {
            uid: sensorData.uid,
            rss: sensorData.rss,
            data: sensorData.data
          });
        }
        return true;
      } else {
        logger.error('Server rejected data', { 
          response: response.data,
          uid: sensorData.uid 
        });
        throw new Error('Server rejected data');
      }
      
    } catch (error) {
      logger.error('Failed to forward data - will retry', {
        error: error.message,
        uid: sensorData.uid,
        rss: sensorData.rss
      });
      
      // Add to retry queue - critical for ensuring no data loss during bursts
      const added = this.queueManager.addToRetry(sensorData);
      if (added) {
        logger.debug(`Added to retry queue: ${sensorData.uid}`, {
          retryCount: sensorData.retryCount || 0,
          queueSize: this.queueManager.size('retry')
        });
      } else {
        logger.warn(`Failed to add to retry queue: ${sensorData.uid} - DATA LOST`, {
          uid: sensorData.uid,
          reason: 'Max retries exceeded or item too old'
        });
      }
      
      this.queueManager.stats.failed++;
      return false;
    }
  }
  
  /**
   * Forward receiver status
   */
  async forwardReceiverStatus(status) {
    try {
      const url = `${config.getServerUrl()}/api/receiver-status/${this.serialNum}`;
      
      const statusData = {
        status: status,
        timestamp: Date.now()
      };
      
      const response = await httpClient.post(url, statusData, {
        timeout: config.server.retryTimeout
      });
      
      logger.info('Receiver status forwarded', { status });
      return response;
      
    } catch (error) {
      // Log different error types appropriately
      if (error.code === 'ECONNREFUSED') {
        logger.debug('Server not available for receiver status', { status });
      } else {
        logger.error('Failed to forward receiver status', {
          error: error.message,
          code: error.code,
          status
        });
      }
      
      // Don't re-throw the error to avoid unhandled rejections
      return null;
    }
  }
  
  /**
   * Log statistics
   */
  logStatistics() {
    const stats = {
      queue: this.queueManager.getStats(),
      http: httpClient.getStats(),
      database: database.getStats(),
      uptime: process.uptime()
    };
    
    logger.info('Gateway statistics', stats);
  }
  
  /**
   * Setup shutdown handlers
   */
  setupShutdownHandlers() {
    const shutdown = async (signal) => {
      if (this.shutdownInProgress) {
        return;
      }
      
      this.shutdownInProgress = true;
      logger.info(`Shutting down gateway (${signal})`);
      
      try {
        // Send disconnected status to web server before shutdown
        await this.forwardReceiverStatus('disconnected').catch(() => {
          // Ignore errors during shutdown
        });
        
        // Stop processing
        this.isRunning = false;
        
        // Clear intervals
        Object.values(this.intervals).forEach(interval => {
          if (interval) clearInterval(interval);
        });
        
        // Close connections
        database.close();
        logger.close();
        
        // Log final stats
        this.logStatistics();
        
        process.exit(0);
      } catch (error) {
        logger.error('Error during shutdown', { error: error.message });
        process.exit(1);
      }
    };
    
    // Handle signals
    process.on('SIGTERM', () => shutdown('SIGTERM'));
    process.on('SIGINT', () => shutdown('SIGINT'));
    
    // Handle uncaught errors
    process.on('uncaughtException', (error) => {
      logger.error('Uncaught exception', {
        error: error.message,
        stack: error.stack
      });
      shutdown('uncaughtException');
    });
    
    process.on('unhandledRejection', (reason, promise) => {
      logger.error('Unhandled promise rejection', {
        reason: reason?.message || reason,
        stack: reason?.stack,
        promise: promise?.constructor?.name || 'Unknown'
      });
      
      // In development, we can continue, but log it prominently
      if (config.debug.enabled) {
        logger.error('UNHANDLED PROMISE REJECTION (debug mode)', {
          reason: reason?.message || reason,
          stack: reason?.stack,
          promise: promise?.constructor?.name || 'Unknown',
          debugMode: true
        });
      }
    });
  }

  /**
   * Start test data simulation for local development
   */
  startTestDataSimulation() {
    logger.info('Starting test data simulation');
    
    const testSensors = [
      { uid: 'EWNXXR', label: 'Office', baseTemp: 75, baseHum: 45 },
      { uid: 'GE47CJ', label: 'Basement', baseTemp: 68, baseHum: 55 },
      { uid: '8EDFFF', label: 'Basement Garage', baseTemp: 70, baseHum: 80 },
      { uid: 'SNWZ5Y', label: 'Bonus Room', baseTemp: 72, baseHum: 50 }
    ];
    
    // Send test data every 30 seconds
    this.intervals.testData = setInterval(() => {
      testSensors.forEach(sensor => {
        // Generate realistic variations in temperature and humidity
        const tempVariation = (Math.random() - 0.5) * 4; // ±2°F
        const humVariation = (Math.random() - 0.5) * 10; // ±5%
        
        const temperature = (sensor.baseTemp + tempVariation).toFixed(1);
        const humidity = Math.round(sensor.baseHum + humVariation);
        
        // Generate realistic RSS values (30-120)
        const rss = Math.round(80 + (Math.random() - 0.5) * 40);
        
        // Generate realistic battery values (2.8-3.2V)
        const battery = (2.8 + Math.random() * 0.4).toFixed(1);
        
        // Simulate direct vs repeater (80% direct, 20% repeater)
        const isDirect = Math.random() > 0.2;
        
        const testPacket = {
          source: isDirect ? 'tx' : 'rp',
          uid: sensor.uid,
          rss: rss,
          bat: parseFloat(battery),
          ppv: '1',
          label: sensor.label,
          data1: temperature,
          data2: humidity.toString(),
          _path: isDirect ? 'direct' : 'repeater',
          _pathIndicator: isDirect ? 'D' : 'R'
        };
        
        // Process the test packet
        this.processSensorData(testPacket);
      });
    }, 30000); // Every 30 seconds
    
    // Send initial data immediately
    setTimeout(() => {
      testSensors.forEach(sensor => {
        const testPacket = {
          source: 'tx',
          uid: sensor.uid,
          rss: 95,
          bat: 3.0,
          ppv: '1',
          label: sensor.label,
          data1: sensor.baseTemp.toString(),
          data2: sensor.baseHum.toString(),
          _path: 'direct',
          _pathIndicator: 'D'
        };
        this.processSensorData(testPacket);
      });
    }, 2000); // After 2 seconds
  }
}

// Main entry point
async function main() {
  const gateway = new Gateway();
  
  // Make gateway available globally for serial module
  global.gateway = gateway;
  
  try {
    await gateway.initialize();
    await gateway.start();
  } catch (error) {
    logger.error('Failed to start gateway', { error: error.message });
    process.exit(1);
  }
}

// Run if called directly
if (require.main === module) {
  main();
}

module.exports = Gateway;