const dotenv = require('dotenv');
const path = require('path');

// Load environment variables
dotenv.config();

// Configuration object with defaults and environment overrides
const config = {
  // Application settings
  app: {
    name: 'MagicLime Gateway',
    version: '1.0.0',
    mode: process.env.MODE || (process.argv.includes('--cloud') ? 'cloud' : 'local'),
    serialFilePath: process.env.SERIAL_FILE_PATH || './gateway-serial.txt'
  },
  
  // Server URLs
  server: {
    urls: {
      local: process.env.LOCAL_SERVER_URL || 'http://localhost:3000',
      cloud: process.env.CLOUD_SERVER_URL || 'https://magiclime.io'
    },
    timeout: parseInt(process.env.SERVER_TIMEOUT) || 10000,
    retryTimeout: parseInt(process.env.SERVER_RETRY_TIMEOUT) || 5000
  },
  
  // Database configuration
  database: {
    filename: process.env.DATABASE_FILE || 'gateway.db',
    connectionTimeout: parseInt(process.env.DB_CONNECTION_TIMEOUT) || 5000,
    busyTimeout: parseInt(process.env.DB_BUSY_TIMEOUT) || 5000
  },
  
  // Serial port configuration
  serial: {
    baudRate: parseInt(process.env.SERIAL_BAUD_RATE) || 115200,
    autoDetect: process.env.SERIAL_AUTO_DETECT !== 'false',
    portPath: process.env.SERIAL_PORT_PATH || null,
    reconnectDelay: parseInt(process.env.SERIAL_RECONNECT_DELAY) || 5000,
    maxReconnectAttempts: parseInt(process.env.SERIAL_MAX_RECONNECT_ATTEMPTS) || 10
  },
  
  // Queue configuration
  queue: {
    maxSize: parseInt(process.env.QUEUE_MAX_SIZE) || 1000,
    processInterval: parseInt(process.env.QUEUE_PROCESS_INTERVAL) || 100,
    retryDelay: parseInt(process.env.QUEUE_RETRY_DELAY) || 30000,
    maxRetries: parseInt(process.env.QUEUE_MAX_RETRIES) || 5,
    itemTTL: parseInt(process.env.QUEUE_ITEM_TTL) || 3600000 // 1 hour
  },
  
  // Logging configuration
  logging: {
    level: process.env.LOG_LEVEL || 'info',
    format: process.env.LOG_FORMAT || 'json', // 'pretty' or 'json'
    file: process.env.LOG_FILE || null,
    maxFileSize: process.env.LOG_MAX_FILE_SIZE || '10m',
    maxFiles: parseInt(process.env.LOG_MAX_FILES) || 5
  },
  
  // Security configuration
  security: {
    requireAuth: process.env.REQUIRE_AUTH === 'true',
    apiKey: process.env.API_KEY || null,
    allowedOrigins: process.env.ALLOWED_ORIGINS ? process.env.ALLOWED_ORIGINS.split(',') : ['*']
  },
  
  // Development/Debug settings
  debug: {
    enabled: process.env.DEBUG === 'true',
    verboseSerial: process.env.DEBUG_SERIAL === 'true',
    mockSensors: process.env.MOCK_SENSORS === 'true'
  }
};

// Helper function to get current server URL
config.getServerUrl = function() {
  return config.server.urls[config.app.mode];
};

// Validate configuration
function validateConfig() {
  const errors = [];
  
  if (!config.server.urls[config.app.mode]) {
    errors.push(`Invalid mode: ${config.app.mode}. Must be 'local' or 'cloud'`);
  }
  
  if (config.serial.baudRate < 9600 || config.serial.baudRate > 115200) {
    errors.push(`Invalid baud rate: ${config.serial.baudRate}`);
  }
  
  if (config.queue.maxSize < 100) {
    errors.push(`Queue size too small: ${config.queue.maxSize}`);
  }
  
  if (config.security.requireAuth && !config.security.apiKey) {
    errors.push('AUTH required but no API_KEY provided');
  }
  
  return errors;
}

// Export config with validation
const errors = validateConfig();
if (errors.length > 0) {
  console.error('Configuration errors:', errors);
  process.exit(1);
}

module.exports = config;