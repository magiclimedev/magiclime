const express = require('express');
const router = express.Router();
const models = require('../models');
const { Op } = require('sequelize');
const deduplicationService = require('../services/packet-deduplication');

// Receiver status endpoint - for gateways to send receiver connection status
router.post('/receiver-status/:serial_num', async (req, res) => {
  try {
    const { serial_num } = req.params;
    const { status, timestamp } = req.body;
    
    // Validate required fields
    if (!status) {
      return res.status(400).json({ 
        error: 'Missing required field: status' 
      });
    }
    
    // Find or create gateway
    const gateway = await models.Gateway.findOrCreateBySerialNum(serial_num);
    
    // Update gateway last seen
    await models.Gateway.updateLastSeen(serial_num);
    
    // Broadcast receiver status to WebSocket clients
    try {
      if (global.io) {
        const statusData = {
          status: status, // 'connected' or 'disconnected'
          timestamp: timestamp || new Date().toISOString(),
          gateway_serial_num: serial_num
        };
        
        // Broadcast to all clients
        global.io.emit('RECEIVER', statusData);
        
        // Broadcast to specific gateway subscribers
        global.io.to(`gateway_${serial_num}`).emit('receiver_status', statusData);
        
        console.log(`📡 Receiver status broadcast: ${serial_num} -> ${status}`);
      }
    } catch (wsError) {
      console.error('WebSocket broadcast failed:', wsError);
    }
    
    res.json({ 
      success: true,
      status: status,
      gateway_serial_num: serial_num
    });
    
  } catch (error) {
    console.error('Error handling receiver status:', error);
    res.status(500).json({ 
      error: 'Failed to handle receiver status',
      message: error.message 
    });
  }
});

// Data ingestion endpoint - for gateways to send sensor data
router.post('/data/:serial_num', async (req, res) => {
  try {
    const { serial_num } = req.params;
    const { uid, type, rss, bat, data, path, pathIndicator, rawPacket } = req.body;
    
    // Validate required fields
    if (!uid || !data) {
      return res.status(400).json({ 
        error: 'Missing required fields: uid, data' 
      });
    }
    
    // Find or create gateway
    const gateway = await models.Gateway.findOrCreateBySerialNum(serial_num);
    
    // Find or create sensor
    const sensor = await models.Sensor.findOrCreateByUid(
      gateway.id, 
      uid, 
      type || 'unknown'
    );
    
    // First check with deduplication service BEFORE logging
    const packetInfo = {
      uid,
      type,
      rss,
      bat,
      data,
      path,
      pathIndicator
    };
    
    // Temporarily use a placeholder ID for deduplication check
    const tempId = Date.now();
    const deduplicationResult = deduplicationService.processPacket(packetInfo, tempId);
    
    // Check if we should update the main sensor display
    let shouldUpdateMain = deduplicationResult.isFirstPacket;
    
    // Also update if this is a significantly better signal from any path
    if (!shouldUpdateMain && rss) {
      const latestLog = await models.Log.findOne({
        where: { sensor_id: sensor.id },
        order: [['created_at', 'DESC']]
      });
      
      // Update if signal is significantly better (5+ RSS improvement)
      if (latestLog && latestLog.rss && rss > latestLog.rss + 5) {
        shouldUpdateMain = true;
        console.log(`📶 RSS improved from ${latestLog.rss} to ${rss} for ${uid} - updating main display`);
      }
    }
    
    let log;
    if (shouldUpdateMain) {
      // First packet or significantly better signal - update main sensor display
      log = await models.Sensor.logData(sensor.id, {
        rss: rss,
        bat: bat,
        data: data,
        path: path,
        pathIndicator: pathIndicator
      });
    } else {
      // Duplicate packet - create log entry but don't update sensor last_seen
      log = await models.Log.create({
        sensor_id: sensor.id,
        rss: rss,
        bat: bat,
        data: data,
        path: path,
        pathIndicator: pathIndicator
      });
    }
    
    // Update gateway last seen
    await models.Gateway.updateLastSeen(serial_num);
    
    // Always broadcast ALL packets to raw logs WebSocket (no deduplication)
    try {
      if (global.io) {
        // Use raw packet data if available, otherwise fall back to processed data
        const rawLogData = rawPacket || {
          uid: uid,
          sensor: type,
          rss: rss,
          bat: bat,
          data: data,
          path: path,
          pathIndicator: pathIndicator
        };
        
        const broadcastData = {
          ...rawLogData,
          timestamp: new Date().toISOString(),
          gateway_serial_num: serial_num,
          log_id: log.id,
          is_duplicate: !deduplicationResult.isFirstPacket,
          duplicate_info: deduplicationResult.duplicateInfo
        };
        
        // Always broadcast to raw logs (shows all packets)
        global.io.emit('LOG_DATA', broadcastData);
        
        // Only broadcast to main dashboard if this is the first packet (deduplication)
        // Use processed format for dashboard
        if (deduplicationResult.shouldUpdate) {
          const dashboardData = {
            uid: uid,
            sensor: type,
            rss: rss,
            bat: bat,
            data: data,
            path: path,
            pathIndicator: pathIndicator,
            timestamp: new Date().toISOString(),
            gateway_serial_num: serial_num
          };
          global.io.to(`gateway_${serial_num}`).emit('sensor_data', dashboardData);
        }
      }
    } catch (wsError) {
      console.error('WebSocket broadcast failed:', wsError);
    }
    
    res.json({ 
      success: true, 
      log_id: log.id,
      sensor_id: sensor.id,
      should_update_display: deduplicationResult.shouldUpdate,
      is_first_packet: deduplicationResult.isFirstPacket
    });
    
  } catch (error) {
    console.error('Error ingesting data:', error);
    res.status(500).json({ 
      error: 'Failed to ingest data',
      message: error.message 
    });
  }
});

// Get gateway information
router.get('/gateway/:serial_num', async (req, res) => {
  try {
    const { serial_num } = req.params;
    
    const gateway = await models.Gateway.findOne({
      where: { serial_num: serial_num },
      include: [{
        model: models.Sensor,
        as: 'sensors'
      }]
    });
    
    if (!gateway) {
      return res.status(404).json({ error: 'Gateway not found' });
    }
    
    res.json(gateway);
    
  } catch (error) {
    console.error('Error fetching gateway:', error);
    res.status(500).json({ 
      error: 'Failed to fetch gateway',
      message: error.message 
    });
  }
});

// Update gateway information
router.put('/gateway/:serial_num', async (req, res) => {
  try {
    const { serial_num } = req.params;
    const { name } = req.body;
    
    const gateway = await models.Gateway.findOne({
      where: { serial_num: serial_num }
    });
    
    if (!gateway) {
      return res.status(404).json({ error: 'Gateway not found' });
    }
    
    if (name) {
      await gateway.update({ name: name });
    }
    
    res.json({ success: true, gateway: gateway });
    
  } catch (error) {
    console.error('Error updating gateway:', error);
    res.status(500).json({ 
      error: 'Failed to update gateway',
      message: error.message 
    });
  }
});

// Get sensors for a gateway
router.get('/gateway/:serial_num/sensors', async (req, res) => {
  try {
    const { serial_num } = req.params;
    
    const gateway = await models.Gateway.findOne({
      where: { serial_num: serial_num }
    });
    
    if (!gateway) {
      return res.status(404).json({ error: 'Gateway not found' });
    }
    
    const sensors = await models.Sensor.getAllByGateway(gateway.id);
    
    res.json(sensors);
    
  } catch (error) {
    console.error('Error fetching sensors:', error);
    res.status(500).json({ 
      error: 'Failed to fetch sensors',
      message: error.message 
    });
  }
});

// Get sensor details
router.get('/sensor/:sensor_id', async (req, res) => {
  try {
    const { sensor_id } = req.params;
    
    const sensor = await models.Sensor.findByPk(sensor_id, {
      include: [{
        model: models.Gateway,
        as: 'gateway'
      }]
    });
    
    if (!sensor) {
      return res.status(404).json({ error: 'Sensor not found' });
    }
    
    res.json(sensor);
    
  } catch (error) {
    console.error('Error fetching sensor:', error);
    res.status(500).json({ 
      error: 'Failed to fetch sensor',
      message: error.message 
    });
  }
});

// Update sensor information
router.put('/sensor/:sensor_id', async (req, res) => {
  try {
    const { sensor_id } = req.params;
    const { name } = req.body;
    
    const sensor = await models.Sensor.findByPk(sensor_id);
    
    if (!sensor) {
      return res.status(404).json({ error: 'Sensor not found' });
    }
    
    if (name) {
      await sensor.update({ name: name });
    }
    
    res.json({ success: true, sensor: sensor });
    
  } catch (error) {
    console.error('Error updating sensor:', error);
    res.status(500).json({ 
      error: 'Failed to update sensor',
      message: error.message 
    });
  }
});

// Get sensor logs
router.get('/sensor/:sensor_id/logs', async (req, res) => {
  try {
    const { sensor_id } = req.params;
    const { hours = 24, limit = 100 } = req.query;
    
    const sensor = await models.Sensor.findByPk(sensor_id);
    
    if (!sensor) {
      return res.status(404).json({ error: 'Sensor not found' });
    }
    
    const logs = await models.Log.getRecentBySensor(
      sensor_id, 
      parseInt(hours)
    );
    
    // Limit results
    const limitedLogs = logs.slice(0, parseInt(limit));
    
    res.json(limitedLogs);
    
  } catch (error) {
    console.error('Error fetching sensor logs:', error);
    res.status(500).json({ 
      error: 'Failed to fetch sensor logs',
      message: error.message 
    });
  }
});

// Get chart data for sensor
router.get('/sensor/:sensor_id/chart-data', async (req, res) => {
  try {
    const { sensor_id } = req.params;
    const { hours = 24 } = req.query;
    
    const sensor = await models.Sensor.findByPk(sensor_id);
    
    if (!sensor) {
      return res.status(404).json({ error: 'Sensor not found' });
    }
    
    const chartData = await models.Log.getChartData(
      sensor_id, 
      parseInt(hours)
    );
    
    res.json(chartData);
    
  } catch (error) {
    console.error('Error fetching chart data:', error);
    res.status(500).json({ 
      error: 'Failed to fetch chart data',
      message: error.message 
    });
  }
});

// Get aggregated chart data (for performance)
router.get('/sensor/:sensor_id/chart-data/aggregated', async (req, res) => {
  try {
    const { sensor_id } = req.params;
    const { hours = 24, interval = 60 } = req.query;
    
    const sensor = await models.Sensor.findByPk(sensor_id);
    
    if (!sensor) {
      return res.status(404).json({ error: 'Sensor not found' });
    }
    
    const chartData = await models.Log.getAggregatedData(
      sensor_id, 
      parseInt(hours),
      parseInt(interval)
    );
    
    res.json(chartData);
    
  } catch (error) {
    console.error('Error fetching aggregated chart data:', error);
    res.status(500).json({ 
      error: 'Failed to fetch aggregated chart data',
      message: error.message 
    });
  }
});

// Health check endpoint
router.get('/health', async (req, res) => {
  try {
    // Test database connection
    await models.sequelize.authenticate();
    
    res.json({ 
      status: 'healthy',
      timestamp: new Date().toISOString(),
      database: 'connected'
    });
    
  } catch (error) {
    res.status(500).json({ 
      status: 'unhealthy',
      timestamp: new Date().toISOString(),
      database: 'disconnected',
      error: error.message
    });
  }
});

// Get raw logs from gateway database
router.get('/gateway/:serial_num/raw-logs', async (req, res) => {
  try {
    const { serial_num } = req.params;
    const { limit = 20 } = req.query;
    
    // Find gateway
    const gateway = await models.Gateway.findOne({
      where: { serial_num: serial_num }
    });
    
    if (!gateway) {
      return res.status(404).json({ error: 'Gateway not found' });
    }
    
    // Make HTTP request to gateway API to get raw logs
    try {
      const gatewayApiUrl = `http://localhost:3001/raw-logs?limit=${limit}`;
      const response = await fetch(gatewayApiUrl);
      
      if (!response.ok) {
        throw new Error(`Gateway API responded with ${response.status}`);
      }
      
      const rawLogs = await response.json();
      res.json(rawLogs);
      
    } catch (gatewayError) {
      console.error('Failed to fetch from gateway API:', gatewayError);
      // Return empty array if gateway API is not available
      res.json([]);
    }
    
  } catch (error) {
    console.error('Error fetching raw logs:', error);
    res.status(500).json({ 
      error: 'Failed to fetch raw logs',
      message: error.message 
    });
  }
});

// Database stats endpoint
router.get('/stats', async (req, res) => {
  try {
    const gatewayCount = await models.Gateway.count();
    const sensorCount = await models.Sensor.count();
    const logCount = await models.Log.count();
    
    // Get active sensors (last 3 hours)
    const threeHoursAgo = new Date(Date.now() - 3 * 60 * 60 * 1000);
    const activeSensorCount = await models.Sensor.count({
      where: {
        last_seen: { [Op.gt]: threeHoursAgo }
      }
    });
    
    res.json({
      gateways: gatewayCount,
      sensors: sensorCount,
      activeSensors: activeSensorCount,
      totalLogs: logCount,
      timestamp: new Date().toISOString()
    });
    
  } catch (error) {
    console.error('Error fetching stats:', error);
    res.status(500).json({ 
      error: 'Failed to fetch stats',
      message: error.message 
    });
  }
});

module.exports = router;