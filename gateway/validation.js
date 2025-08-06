const { ValidationError } = require('./errors');

// Validation schemas for different data types
const schemas = {
  sensorData: {
    source: { type: 'string', required: true, enum: ['tx', 'rx', 'rp'] },
    uid: { type: 'string', required: true, pattern: /^[A-Z0-9]{6}$/ },
    rss: { type: 'number', required: false, min: -200, max: 200 },
    bat: { type: 'number', required: false, min: 0, max: 5 },
    ppv: { type: 'string', required: false },
    label: { type: 'string', required: false, maxLength: 50 },
    data1: { type: 'string', required: false, maxLength: 100 },
    data2: { type: 'string', required: false, maxLength: 100 }
  },
  
  receiverStatus: {
    source: { type: 'string', required: true, enum: ['rx'] },
    RX_VER: { type: 'string', required: false },
    KEY_RSS: { type: 'number', required: false },
    KEY_RX: { type: 'string', required: false }
  }
};

// Validation functions
const validators = {
  string: (value, rules) => {
    if (typeof value !== 'string') {
      throw new ValidationError(`Expected string, got ${typeof value}`);
    }
    
    if (rules.enum && !rules.enum.includes(value)) {
      throw new ValidationError(`Value must be one of: ${rules.enum.join(', ')}`);
    }
    
    if (rules.pattern && !rules.pattern.test(value)) {
      throw new ValidationError(`Value does not match pattern: ${rules.pattern}`);
    }
    
    if (rules.minLength && value.length < rules.minLength) {
      throw new ValidationError(`String too short. Min length: ${rules.minLength}`);
    }
    
    if (rules.maxLength && value.length > rules.maxLength) {
      throw new ValidationError(`String too long. Max length: ${rules.maxLength}`);
    }
    
    return value;
  },
  
  number: (value, rules) => {
    const num = Number(value);
    if (isNaN(num)) {
      throw new ValidationError(`Expected number, got ${typeof value}`);
    }
    
    if (rules.min !== undefined && num < rules.min) {
      throw new ValidationError(`Value too small. Min: ${rules.min}`);
    }
    
    if (rules.max !== undefined && num > rules.max) {
      throw new ValidationError(`Value too large. Max: ${rules.max}`);
    }
    
    return num;
  },
  
  boolean: (value) => {
    if (typeof value !== 'boolean') {
      throw new ValidationError(`Expected boolean, got ${typeof value}`);
    }
    return value;
  }
};

// Main validation function
function validate(data, schemaName) {
  const schema = schemas[schemaName];
  if (!schema) {
    throw new ValidationError(`Unknown schema: ${schemaName}`);
  }
  
  const validated = {};
  const errors = [];
  
  // Check required fields
  for (const [field, rules] of Object.entries(schema)) {
    if (rules.required && !(field in data)) {
      errors.push({ field, message: 'Required field missing' });
      continue;
    }
    
    if (field in data && data[field] !== undefined && data[field] !== null) {
      try {
        const validator = validators[rules.type];
        if (!validator) {
          throw new ValidationError(`Unknown type: ${rules.type}`);
        }
        
        validated[field] = validator(data[field], rules);
      } catch (error) {
        errors.push({ field, message: error.message });
      }
    }
  }
  
  // Check for unknown fields
  for (const field of Object.keys(data)) {
    if (!(field in schema)) {
      // Allow unknown fields but log them
      validated[field] = data[field];
    }
  }
  
  if (errors.length > 0) {
    throw new ValidationError('Validation failed', { errors });
  }
  
  return validated;
}

// Specific validators
function validateSensorData(data) {
  return validate(data, 'sensorData');
}

function validateReceiverStatus(data) {
  return validate(data, 'receiverStatus');
}

// Sanitization functions
function sanitizeString(str, maxLength = 255) {
  if (typeof str !== 'string') return '';
  
  // Remove control characters
  str = str.replace(/[\x00-\x1F\x7F]/g, '');
  
  // Trim and limit length
  return str.trim().substring(0, maxLength);
}

function sanitizeSensorData(data) {
  const sanitized = { ...data };
  
  if (sanitized.uid) {
    sanitized.uid = sanitizeString(sanitized.uid, 6).toUpperCase();
  }
  
  if (sanitized.label) {
    sanitized.label = sanitizeString(sanitized.label, 50);
  }
  
  if (sanitized.sensor) {
    sanitized.sensor = sanitizeString(sanitized.sensor, 50);
  }
  
  if (sanitized.data1) {
    sanitized.data1 = sanitizeString(sanitized.data1, 100);
  }
  
  if (sanitized.data2) {
    sanitized.data2 = sanitizeString(sanitized.data2, 100);
  }
  
  return sanitized;
}

module.exports = {
  validate,
  validateSensorData,
  validateReceiverStatus,
  sanitizeString,
  sanitizeSensorData
};