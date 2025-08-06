/**
 * Simple HTTP API server for gateway data
 * Serves raw logs to web interface
 */

const http = require('http');
const url = require('url');
const config = require('./config');
const database = require('./db');
const logger = require('./logger');

class GatewayAPI {
  constructor() {
    this.server = null;
    this.port = process.env.GATEWAY_API_PORT || 3001;
  }

  initialize() {
    this.server = http.createServer((req, res) => {
      this.handleRequest(req, res);
    });

    this.server.listen(this.port, () => {
      logger.info('Gateway API server started', { port: this.port });
    });

    this.server.on('error', (error) => {
      logger.error('Gateway API server error', { error: error.message });
    });
  }

  handleRequest(req, res) {
    const parsedUrl = url.parse(req.url, true);
    const path = parsedUrl.pathname;
    const method = req.method;

    // Set CORS headers
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
    res.setHeader('Content-Type', 'application/json');

    // Handle preflight requests
    if (method === 'OPTIONS') {
      res.writeHead(200);
      res.end();
      return;
    }

    try {
      if (method === 'GET' && path === '/raw-logs') {
        this.handleRawLogs(req, res, parsedUrl.query);
      } else if (method === 'GET' && path === '/health') {
        this.handleHealth(req, res);
      } else {
        res.writeHead(404);
        res.end(JSON.stringify({ error: 'Not found' }));
      }
    } catch (error) {
      logger.error('API request error', { error: error.message, path, method });
      res.writeHead(500);
      res.end(JSON.stringify({ error: 'Internal server error' }));
    }
  }

  handleRawLogs(req, res, query) {
    try {
      const limit = parseInt(query.limit) || 20;
      const logs = database.getRawLogs(limit);
      
      res.writeHead(200);
      res.end(JSON.stringify(logs));
      
      logger.debug('Raw logs requested', { count: logs.length, limit });
    } catch (error) {
      logger.error('Failed to get raw logs', { error: error.message });
      res.writeHead(500);
      res.end(JSON.stringify({ error: 'Failed to get raw logs' }));
    }
  }

  handleHealth(req, res) {
    res.writeHead(200);
    res.end(JSON.stringify({ 
      status: 'healthy',
      timestamp: new Date().toISOString() 
    }));
  }

  close() {
    if (this.server) {
      this.server.close(() => {
        logger.info('Gateway API server closed');
      });
    }
  }
}

module.exports = GatewayAPI;