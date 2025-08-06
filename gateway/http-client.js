const axios = require('axios');
const config = require('./config');
const logger = require('./logger');
const { NetworkError, errorHandler } = require('./errors');

/**
 * HTTP client with circuit breaker and retry logic
 */
class HttpClient {
  constructor() {
    this.client = null;
    this.circuitBreaker = {
      failures: 0,
      lastFailure: 0,
      state: 'closed', // closed, open, half-open
      threshold: 5,
      timeout: 60000 // 1 minute
    };
    
    this.stats = {
      requests: 0,
      successes: 0,
      failures: 0,
      timeouts: 0,
      circuitBreaks: 0
    };
    
    this.initialize();
  }
  
  /**
   * Initialize axios client with defaults
   */
  initialize() {
    this.client = axios.create({
      timeout: config.server.timeout,
      headers: {
        'User-Agent': `MagicLime-Gateway/${config.app.version}`,
        'Content-Type': 'application/json'
      },
      validateStatus: (status) => status < 500, // Don't throw on 4xx
      maxRedirects: 5
    });
    
    // Add request interceptor for auth
    if (config.security.requireAuth && config.security.apiKey) {
      this.client.interceptors.request.use(config => {
        config.headers['X-API-Key'] = config.security.apiKey;
        return config;
      });
    }
    
    // Add response interceptor for logging
    this.client.interceptors.response.use(
      response => {
        this.stats.requests++;
        this.stats.successes++;
        this.onSuccess();
        return response;
      },
      error => {
        this.stats.requests++;
        this.stats.failures++;
        this.onFailure(error);
        return Promise.reject(error);
      }
    );
  }
  
  /**
   * Check if circuit breaker allows request
   */
  canMakeRequest() {
    const breaker = this.circuitBreaker;
    
    if (breaker.state === 'closed') {
      return true;
    }
    
    if (breaker.state === 'open') {
      const timeSinceFailure = Date.now() - breaker.lastFailure;
      if (timeSinceFailure > breaker.timeout) {
        // Try half-open
        breaker.state = 'half-open';
        logger.info('Circuit breaker half-open, trying request');
        return true;
      }
      return false;
    }
    
    // Half-open state - allow one request
    return true;
  }
  
  /**
   * Handle successful request
   */
  onSuccess() {
    const breaker = this.circuitBreaker;
    
    if (breaker.state === 'half-open') {
      // Reset circuit breaker
      breaker.state = 'closed';
      breaker.failures = 0;
      logger.info('Circuit breaker reset to closed');
    }
  }
  
  /**
   * Handle failed request
   */
  onFailure(error) {
    const breaker = this.circuitBreaker;
    
    // Count timeouts separately
    if (error.code === 'ECONNABORTED' || error.code === 'ETIMEDOUT') {
      this.stats.timeouts++;
    }
    
    breaker.failures++;
    breaker.lastFailure = Date.now();
    
    if (breaker.failures >= breaker.threshold && breaker.state === 'closed') {
      breaker.state = 'open';
      this.stats.circuitBreaks++;
      logger.error('Circuit breaker opened', {
        failures: breaker.failures,
        timeout: breaker.timeout + 'ms'
      });
    }
  }
  
  /**
   * Make HTTP request with circuit breaker
   */
  async request(options) {
    if (!this.canMakeRequest()) {
      const error = new NetworkError('Circuit breaker is open', {
        state: this.circuitBreaker.state,
        failures: this.circuitBreaker.failures
      });
      throw error;
    }
    
    try {
      const response = await this.client.request(options);
      
      // Check for application-level errors
      if (response.status >= 400) {
        throw new NetworkError(`HTTP ${response.status}: ${response.statusText}`, {
          status: response.status,
          data: response.data
        });
      }
      
      return response;
      
    } catch (error) {
      // Convert axios errors to our error types
      if (error.code === 'ECONNREFUSED') {
        throw new NetworkError('Connection refused - is the server running?', {
          url: options.url,
          code: error.code
        });
      }
      
      if (error.code === 'ETIMEDOUT' || error.code === 'ECONNABORTED') {
        throw new NetworkError('Request timeout', {
          url: options.url,
          timeout: options.timeout || config.server.timeout
        });
      }
      
      if (error.response) {
        throw new NetworkError(`HTTP ${error.response.status}: ${error.response.statusText}`, {
          status: error.response.status,
          data: error.response.data,
          url: options.url
        });
      }
      
      // Re-throw if already our error type
      if (error instanceof NetworkError) {
        throw error;
      }
      
      // Unknown error
      throw new NetworkError(error.message || 'Unknown network error', {
        url: options.url,
        originalError: error.message
      });
    }
  }
  
  /**
   * POST request helper
   */
  async post(url, data, options = {}) {
    return this.request({
      method: 'POST',
      url,
      data,
      ...options
    });
  }
  
  /**
   * GET request helper
   */
  async get(url, options = {}) {
    return this.request({
      method: 'GET',
      url,
      ...options
    });
  }
  
  /**
   * Get client statistics
   */
  getStats() {
    return {
      ...this.stats,
      circuitBreaker: {
        state: this.circuitBreaker.state,
        failures: this.circuitBreaker.failures
      }
    };
  }
  
  /**
   * Reset statistics
   */
  resetStats() {
    this.stats = {
      requests: 0,
      successes: 0,
      failures: 0,
      timeouts: 0,
      circuitBreaks: 0
    };
  }
  
  /**
   * Reset circuit breaker
   */
  resetCircuitBreaker() {
    this.circuitBreaker = {
      failures: 0,
      lastFailure: 0,
      state: 'closed',
      threshold: 5,
      timeout: 60000
    };
    logger.info('Circuit breaker reset');
  }
}

// Export singleton instance
module.exports = new HttpClient();