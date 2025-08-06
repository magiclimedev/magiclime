// Custom error classes for better error handling

class GatewayError extends Error {
  constructor(message, code, details = {}) {
    super(message);
    this.name = this.constructor.name;
    this.code = code;
    this.details = details;
    this.timestamp = new Date().toISOString();
    Error.captureStackTrace(this, this.constructor);
  }
  
  toJSON() {
    return {
      name: this.name,
      message: this.message,
      code: this.code,
      details: this.details,
      timestamp: this.timestamp,
      stack: this.stack
    };
  }
}

class DatabaseError extends GatewayError {
  constructor(message, details = {}) {
    super(message, 'DATABASE_ERROR', details);
  }
}

class SerialPortError extends GatewayError {
  constructor(message, details = {}) {
    super(message, 'SERIAL_PORT_ERROR', details);
  }
}

class NetworkError extends GatewayError {
  constructor(message, details = {}) {
    super(message, 'NETWORK_ERROR', details);
  }
}

class ValidationError extends GatewayError {
  constructor(message, details = {}) {
    super(message, 'VALIDATION_ERROR', details);
  }
}

class ConfigurationError extends GatewayError {
  constructor(message, details = {}) {
    super(message, 'CONFIGURATION_ERROR', details);
  }
}

// Error handler utility functions
const errorHandler = {
  // Handle different error types appropriately
  handle(error, context = {}) {
    const logger = require('./logger');
    
    // Add context to error
    if (error instanceof GatewayError) {
      error.details = { ...error.details, ...context };
    }
    
    // Log based on error type
    if (error instanceof ValidationError) {
      logger.warn(error.message, { error: error.toJSON() });
    } else if (error instanceof NetworkError) {
      logger.error('Network error occurred', { error: error.toJSON() });
    } else if (error instanceof DatabaseError) {
      logger.error('Database error occurred', { error: error.toJSON() });
    } else if (error instanceof SerialPortError) {
      logger.error('Serial port error occurred', { error: error.toJSON() });
    } else {
      // Unknown errors
      logger.error('Unexpected error occurred', {
        error: {
          name: error.name,
          message: error.message,
          stack: error.stack,
          ...context
        }
      });
    }
    
    return error;
  },
  
  // Wrap async functions with error handling
  asyncWrapper(fn, context = {}) {
    return async (...args) => {
      try {
        return await fn(...args);
      } catch (error) {
        this.handle(error, context);
        throw error;
      }
    };
  },
  
  // Create error from various sources
  from(error, ErrorClass = GatewayError, details = {}) {
    if (error instanceof ErrorClass) {
      return error;
    }
    
    // Handle specific error patterns
    if (error.code === 'SQLITE_CONSTRAINT') {
      return new DatabaseError(error.message, { sqliteCode: error.code, ...details });
    }
    
    if (error.code === 'ECONNREFUSED' || error.code === 'ETIMEDOUT') {
      return new NetworkError(error.message, { 
        code: error.code,
        address: error.address,
        port: error.port,
        ...details 
      });
    }
    
    if (error.message?.includes('Serial port')) {
      return new SerialPortError(error.message, details);
    }
    
    // Default conversion
    return new ErrorClass(error.message || 'Unknown error', details);
  }
};

module.exports = {
  GatewayError,
  DatabaseError,
  SerialPortError,
  NetworkError,
  ValidationError,
  ConfigurationError,
  errorHandler
};