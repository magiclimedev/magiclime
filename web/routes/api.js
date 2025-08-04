const express = require('express');
const router = express.Router();
const models = require('../models');
const { Op } = require('sequelize');

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
    const { uid, type, rss, bat, data } = req.body;
    
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
    
    // Log the data
    const log = await models.Sensor.logData(sensor.id, {
      rss: rss,
      bat: bat,
      data: data
    });
    
    // Update gateway last seen
    await models.Gateway.updateLastSeen(serial_num);
    
    // Broadcast to WebSocket clients if available
    try {
      if (global.io) {
        const broadcastData = {
          uid: uid,
          sensor: type,
          rss: rss,
          bat: bat,
          data: data,
          timestamp: new Date().toISOString(),
          gateway_serial_num: serial_num
        };
        
        // Broadcast to all clients
        global.io.emit('LOG_DATA', broadcastData);
        
        // Broadcast to specific gateway subscribers
        global.io.to(`gateway_${serial_num}`).emit('sensor_data', broadcastData);
      }
    } catch (wsError) {
      console.error('WebSocket broadcast failed:', wsError);
    }
    
    res.json({ 
      success: true, 
      log_id: log.id,
      sensor_id: sensor.id 
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