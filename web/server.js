#!/usr/bin/env node

/**
 * MagicLime Web Server
 * 
 * Standalone web server that receives data from MagicLime gateways
 * and serves cloud dashboards accessible at /{serial_num}
 */

const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const cors = require('cors');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const path = require('path');
require('dotenv').config();

// Initialize Express app
const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
  cors: {
    origin: "*",
    methods: ["GET", "POST"]
  }
});

// Configuration
const PORT = process.env.PORT || 3000;
const NODE_ENV = process.env.NODE_ENV || 'local';
const isDev = process.argv.includes('--dev') || NODE_ENV === 'development';

// Import models
const models = require('./models');

// Middleware
app.use(helmet({
  contentSecurityPolicy: false // Allow inline scripts for dashboard
}));

app.use(cors());
app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true, limit: '10mb' }));

// Rate limiting
const limiter = rateLimit({
  windowMs: 15 * 60 * 1000, // 15 minutes
  max: 1000, // Limit each IP to 1000 requests per windowMs
  message: 'Too many requests from this IP, please try again later.'
});
app.use('/api/', limiter);

// Template engine
app.set('view engine', 'ejs');
app.set('views', path.join(__dirname, 'views'));

// Static files
app.use('/static', express.static(path.join(__dirname, 'public')));
app.use('/assets', express.static(path.join(__dirname, 'public')));

// Routes
const apiRoutes = require('./routes/api');
const dashboardRoutes = require('./routes/dashboard');

app.use('/api', apiRoutes);
app.use('/', dashboardRoutes);

// Socket.IO connection handling
io.on('connection', (socket) => {
  console.log(`Client connected: ${socket.id}`);
  
  socket.on('subscribe', (data) => {
    if (data.gateway_serial_num) {
      socket.join(`gateway_${data.gateway_serial_num}`);
      console.log(`Client ${socket.id} subscribed to gateway ${data.gateway_serial_num}`);
    }
  });
  
  socket.on('disconnect', () => {
    console.log(`Client disconnected: ${socket.id}`);
  });
});

// Global Socket.IO instance for broadcasting
global.io = io;

// Health check endpoint
app.get('/health', async (req, res) => {
  try {
    await models.sequelize.authenticate();
    res.json({
      status: 'healthy',
      timestamp: new Date().toISOString(),
      database: 'connected',
      environment: NODE_ENV
    });
  } catch (error) {
    res.status(500).json({
      status: 'unhealthy',
      timestamp: new Date().toISOString(),
      database: 'disconnected',
      error: error.message,
      environment: NODE_ENV
    });
  }
});

// 404 handler
app.use((req, res) => {
  res.status(404).render('404', {
    url: req.originalUrl,
    serial_num: undefined
  });
});

// Error handler
app.use((err, req, res, next) => {
  console.error('Server error:', err);
  
  res.status(err.status || 500).render('error', {
    message: isDev ? err.message : 'Internal server error',
    error: isDev ? err : {}
  });
});

// Initialize database and start server
async function startServer() {
  try {
    console.log('🔧 Initializing MagicLime Web Server...');
    
    // Initialize database
    console.log('📦 Connecting to database...');
    await models.sequelize.sync({ alter: isDev });
    console.log('✅ Database connected and synchronized');
    
    // Ensure default settings
    await models.Setting.ensureDefaults();
    console.log('⚙️  Default settings initialized');
    
    // Start server
    server.listen(PORT, () => {
      console.log('');
      console.log('🚀 MagicLime Web Server started successfully!');
      console.log('');
      console.log(`🌐 Server running on: http://localhost:${PORT}`);
      console.log(`📊 Environment: ${NODE_ENV}`);
      console.log(`🔧 Development mode: ${isDev ? 'ON' : 'OFF'}`);
      console.log('');
      console.log('📋 Available endpoints:');
      console.log(`   • Health check: http://localhost:${PORT}/health`);
      console.log(`   • Dashboard: http://localhost:${PORT}/{gateway_serial_num}`);
      console.log(`   • Data ingestion: POST http://localhost:${PORT}/api/data/{gateway_serial_num}`);
      console.log('');
      console.log('🎯 Ready to receive data from MagicLime gateways!');
      console.log('');
    });
    
  } catch (error) {
    console.error('❌ Failed to start server:', error);
    process.exit(1);
  }
}

// Graceful shutdown
process.on('SIGTERM', async () => {
  console.log('🛑 Received SIGTERM, shutting down gracefully...');
  
  server.close(() => {
    console.log('📴 HTTP server closed');
  });
  
  try {
    await models.sequelize.close();
    console.log('📦 Database connection closed');
    process.exit(0);
  } catch (error) {
    console.error('❌ Error during shutdown:', error);
    process.exit(1);
  }
});

process.on('SIGINT', async () => {
  console.log('🛑 Received SIGINT, shutting down gracefully...');
  
  server.close(() => {
    console.log('📴 HTTP server closed');
  });
  
  try {
    await models.sequelize.close();
    console.log('📦 Database connection closed');
    process.exit(0);
  } catch (error) {
    console.error('❌ Error during shutdown:', error);
    process.exit(1);
  }
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

// Start the server
startServer();