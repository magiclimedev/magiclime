const express = require('express');
const router = express.Router();
const models = require('../models');
const { Op } = require('sequelize');

// Cloud dashboard route - accessible at https://magiclime.io/{serial_num}
router.get('/:serial_num', async (req, res) => {
  try {
    const { serial_num } = req.params;
    
    // Find gateway by serial number
    const gateway = await models.Gateway.findOne({
      where: { serial_num: serial_num },
      include: [{
        model: models.Sensor,
        as: 'sensors',
        required: false,
        include: [{
          model: models.Log,
          as: 'logs',
          limit: 1,
          order: [['created_at', 'DESC']]
        }]
      }]
    });
    
    if (!gateway) {
      return res.status(404).render('404', { 
        serial_num: serial_num 
      });
    }
    
    // Get sensors with status and recent logs
    const sensorsBasic = await models.Sensor.getAllByGateway(gateway.id);
    
    // Add recent logs to each sensor
    const sensors = await Promise.all(sensorsBasic.map(async (sensor) => {
      // Get the most recent log
      const recentLogs = await models.Log.findAll({
        where: { sensor_id: sensor.id },
        order: [['created_at', 'DESC']],
        limit: 1
      });
      
      // If the most recent log shows repeater, check if sensor has direct connectivity
      if (recentLogs.length > 0 && 
          (recentLogs[0].pathIndicator === 'R' || recentLogs[0].path === 'repeater')) {
        // Look back through recent logs to see if any came direct
        const checkLogs = await models.Log.findAll({
          where: { sensor_id: sensor.id },
          order: [['created_at', 'DESC']],
          limit: 20 // Check last 20 logs
        });
        
        // Find the first direct packet if any
        const directLog = checkLogs.find(log => 
          log.pathIndicator === 'D' || log.path === 'direct'
        );
        
        if (directLog) {
          // Use the direct log's path indicator but keep the latest data
          recentLogs[0].pathIndicator = 'D';
          recentLogs[0].path = 'direct';
        }
      }
      
      return {
        ...sensor,
        logs: recentLogs
      };
    }));
    
    // Check gateway status (connected if last_seen < 5 minutes ago)
    const fiveMinutesAgo = new Date(Date.now() - 5 * 60 * 1000);
    const gatewayStatus = gateway.last_seen > fiveMinutesAgo ? 'connected' : 'disconnected';
    
    res.render('dashboard', {
      gateway: gateway,
      sensors: sensors,
      gatewayStatus: gatewayStatus,
      cloudUrl: models.Gateway.getCloudUrl(serial_num)
    });
    
  } catch (error) {
    console.error('Error loading dashboard:', error);
    res.status(500).render('error', { 
      message: 'Internal server error',
      error: error.message 
    });
  }
});

// Sensor detail page
router.get('/:serial_num/sensor/:sensor_id', async (req, res) => {
  try {
    const { serial_num, sensor_id } = req.params;
    
    // Find gateway and sensor
    const gateway = await models.Gateway.findOne({
      where: { serial_num: serial_num }
    });
    
    if (!gateway) {
      return res.status(404).render('404', { 
        serial_num: serial_num 
      });
    }
    
    const sensor = await models.Sensor.findOne({
      where: { 
        id: sensor_id,
        gateway_id: gateway.id 
      }
    });
    
    if (!sensor) {
      return res.status(404).render('404', { 
        serial_num: serial_num 
      });
    }
    
    // Get deduplicated recent logs for display
    const logs = await models.Log.getDeduplicatedRecentBySensor(sensor_id, 24);
    const chartData = await models.Log.getChartData(sensor_id, 24);
    
    res.render('sensor-detail', {
      gateway: gateway,
      sensor: sensor,
      logs: logs,
      chartData: JSON.stringify(chartData)
    });
    
  } catch (error) {
    console.error('Error loading sensor detail:', error);
    res.status(500).render('error', { 
      message: 'Internal server error',
      error: error.message 
    });
  }
});

// Raw data view
router.get('/:serial_num/raw', async (req, res) => {
  try {
    const { serial_num } = req.params;
    
    const gateway = await models.Gateway.findOne({
      where: { serial_num: serial_num }
    });
    
    if (!gateway) {
      return res.status(404).render('404', { 
        serial_num: serial_num 
      });
    }
    
    // Get recent logs from all sensors
    const recentLogs = await models.Log.findAll({
      include: [{
        model: models.Sensor,
        as: 'sensor',
        where: { gateway_id: gateway.id },
        include: [{
          model: models.Gateway,
          as: 'gateway'
        }]
      }],
      order: [['created_at', 'DESC']],
      limit: 100
    });
    
    res.render('raw-data', {
      gateway: gateway,
      logs: recentLogs
    });
    
  } catch (error) {
    console.error('Error loading raw data:', error);
    res.status(500).render('error', { 
      message: 'Internal server error',
      error: error.message 
    });
  }
});

module.exports = router;