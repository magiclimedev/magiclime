const config = require('./config');
const fs = require('fs');
const path = require('path');

// Log levels
const LOG_LEVELS = {
  error: 0,
  warn: 1,
  info: 2,
  debug: 3
};

// ANSI color codes for console output
const COLORS = {
  error: '\x1b[31m', // red
  warn: '\x1b[33m',  // yellow
  info: '\x1b[36m',  // cyan
  debug: '\x1b[90m', // gray
  reset: '\x1b[0m'
};

// Emoji prefixes for pretty format
const EMOJI_PREFIXES = {
  error: '❌',
  warn: '⚠️ ',
  info: 'ℹ️ ',
  debug: '🔍'
};

class Logger {
  constructor() {
    this.level = LOG_LEVELS[config.logging.level] || LOG_LEVELS.info;
    this.format = config.logging.format;
    this.fileStream = null;
    
    // Setup file logging if configured
    if (config.logging.file) {
      this.setupFileLogging();
    }
  }
  
  setupFileLogging() {
    const logDir = path.dirname(config.logging.file);
    if (!fs.existsSync(logDir)) {
      fs.mkdirSync(logDir, { recursive: true });
    }
    
    this.fileStream = fs.createWriteStream(config.logging.file, { flags: 'a' });
  }
  
  log(level, message, meta = {}) {
    if (LOG_LEVELS[level] > this.level) {
      return;
    }
    
    const timestamp = new Date().toISOString();
    const logEntry = {
      timestamp,
      level,
      message,
      ...meta
    };
    
    // Console output
    if (this.format === 'json') {
      console.log(JSON.stringify(logEntry));
    } else {
      this.prettyPrint(level, message, meta, timestamp);
    }
    
    // File output
    if (this.fileStream) {
      this.fileStream.write(JSON.stringify(logEntry) + '\n');
    }
  }
  
  prettyPrint(level, message, meta, timestamp) {
    const color = COLORS[level] || COLORS.reset;
    const emoji = EMOJI_PREFIXES[level] || '';
    
    // Format timestamp for display
    const date = new Date(timestamp);
    const timeStr = date.toLocaleTimeString('en-US', { 
      hour12: false,
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
    
    // Start with timestamp
    let output = `${COLORS.debug}[${timeStr}]${COLORS.reset} `;
    
    // Add level indicator and message
    output += `${color}${emoji} ${message}${COLORS.reset}`;
    
    // Add metadata if present
    if (Object.keys(meta).length > 0) {
      const metaStr = Object.entries(meta)
        .map(([key, value]) => {
          if (typeof value === 'object') {
            return `${key}=${JSON.stringify(value)}`;
          }
          return `${key}=${value}`;
        })
        .join(' ');
      output += ` ${COLORS.debug}[${metaStr}]${COLORS.reset}`;
    }
    
    console.log(output);
  }
  
  error(message, meta = {}) {
    this.log('error', message, meta);
  }
  
  warn(message, meta = {}) {
    this.log('warn', message, meta);
  }
  
  info(message, meta = {}) {
    this.log('info', message, meta);
  }
  
  debug(message, meta = {}) {
    this.log('debug', message, meta);
  }
  
  // Log with structured context
  child(context = {}) {
    const childLogger = Object.create(this);
    childLogger.log = (level, message, meta = {}) => {
      return this.log(level, message, { ...context, ...meta });
    };
    
    // Bind convenience methods
    ['error', 'warn', 'info', 'debug'].forEach(level => {
      childLogger[level] = (message, meta = {}) => {
        return childLogger.log(level, message, meta);
      };
    });
    
    return childLogger;
  }
  
  // Close file stream
  close() {
    if (this.fileStream) {
      this.fileStream.end();
    }
  }
}

// Export singleton instance
module.exports = new Logger();