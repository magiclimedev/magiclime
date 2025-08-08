'use strict';

module.exports = (sequelize, DataTypes) => {
  const Log = sequelize.define('Log', {
    rss: {
      type: DataTypes.INTEGER,
      allowNull: true
    },
    bat: {
      type: DataTypes.FLOAT,
      allowNull: true
    },
    data: {
      type: DataTypes.JSON,
      allowNull: true
    },
    path: {
      type: DataTypes.STRING,
      allowNull: true
    },
    pathIndicator: {
      type: DataTypes.STRING,
      allowNull: true
    }
  }, {
    tableName: 'log',
    underscored: true,
    updatedAt: false // Only need created_at
  });

  // Static methods
  Log.getRecentBySensor = async function(sensorId, hours = 24) {
    const since = new Date(Date.now() - hours * 60 * 60 * 1000);
    return await this.findAll({
      where: {
        sensor_id: sensorId,
        created_at: { [sequelize.Sequelize.Op.gt]: since }
      },
      order: [['created_at', 'DESC']],
      limit: 1000 // Reasonable limit to prevent memory issues
    });
  };

  Log.getDeduplicatedRecentBySensor = async function(sensorId, hours = 24) {
    const since = new Date(Date.now() - hours * 60 * 60 * 1000);
    
    // Get all logs first, then deduplicate in JavaScript to avoid complex SQL locks
    const allLogs = await this.findAll({
      where: {
        sensor_id: sensorId,
        created_at: { [sequelize.Sequelize.Op.gt]: since }
      },
      order: [['created_at', 'DESC']],
      limit: 500
    });

    // Deduplicate in JavaScript by grouping by minute + data
    const deduplicatedLogs = [];
    const seenKeys = new Set();
    
    for (const log of allLogs) {
      // Handle both underscored and camelCase field names
      const dateField = log.created_at || log.createdAt || log.dataValues?.created_at || log.dataValues?.createdAt;
      
      // Validate date before converting to ISO string
      const logDate = new Date(dateField);
      if (isNaN(logDate.getTime())) {
        console.warn('Skipping log with invalid date:', { 
          id: log.id, 
          created_at: log.created_at, 
          createdAt: log.createdAt,
          dateField: dateField 
        });
        continue; // Skip logs with invalid dates
      }
      
      const minute = logDate.toISOString().slice(0, 16); // YYYY-MM-DDTHH:mm
      const dataStr = JSON.stringify(log.data);
      const key = `${minute}-${dataStr}`;
      
      if (!seenKeys.has(key)) {
        seenKeys.add(key);
        deduplicatedLogs.push(log);
        
        // Limit to 50 entries for display
        if (deduplicatedLogs.length >= 50) break;
      }
    }

    return deduplicatedLogs;
  };

  Log.getChartData = async function(sensorId, hours = 24) {
    const since = new Date(Date.now() - hours * 60 * 60 * 1000);
    
    try {
      // Use JavaScript deduplication instead of complex SQL to avoid locks
      const allLogs = await this.findAll({
        where: {
          sensor_id: sensorId,
          created_at: { [sequelize.Sequelize.Op.gt]: since }
        },
        order: [['created_at', 'DESC']],
        limit: 1000
      });

      // Deduplicate by minute + data in JavaScript
      const deduplicatedLogs = [];
      const seenKeys = new Set();
      
      for (const log of allLogs) {
        // Handle both underscored and camelCase field names
        const dateField = log.created_at || log.createdAt || log.dataValues?.created_at || log.dataValues?.createdAt;
        
        // Validate date before processing
        const logDate = new Date(dateField);
        if (isNaN(logDate.getTime())) {
          console.warn('Skipping chart log with invalid date:', { 
            id: log.id, 
            created_at: log.created_at, 
            createdAt: log.createdAt,
            dateField: dateField 
          });
          continue;
        }
        
        const minute = logDate.toISOString().slice(0, 16); // YYYY-MM-DDTHH:mm
        const dataStr = JSON.stringify(log.data);
        const key = `${minute}-${dataStr}`;
        
        if (!seenKeys.has(key)) {
          seenKeys.add(key);
          deduplicatedLogs.push(log);
        }
      }

      const chartData = deduplicatedLogs.map(log => {
        const dateField = log.created_at || log.createdAt || log.dataValues?.created_at || log.dataValues?.createdAt;
        return {
          time: dateField,
          value: this.extractValue(log.data),
          rss: log.rss,
          bat: log.bat
        };
      }).filter(point => point.value !== null); // Remove null values
      
      // Reverse to show oldest to newest (left to right chronologically)
      return chartData.reverse();
    } catch (error) {
      console.error('Error in getChartData:', error);
      return [];
    }
  };

  Log.getLatestBySensor = async function(sensorId) {
    return await this.findOne({
      where: { sensor_id: sensorId },
      order: [['created_at', 'DESC']]
    });
  };

  Log.cleanup = async function(daysToKeep = 30) {
    const cutoffDate = new Date(Date.now() - daysToKeep * 24 * 60 * 60 * 1000);
    const deletedCount = await this.destroy({
      where: {
        created_at: { [sequelize.Sequelize.Op.lt]: cutoffDate }
      }
    });
    return deletedCount;
  };

  Log.getAggregatedData = async function(sensorId, hours = 24, intervalMinutes = 60) {
    // Get data aggregated by time intervals for better chart performance
    const since = new Date(Date.now() - hours * 60 * 60 * 1000);
    
    const logs = await this.findAll({
      where: {
        sensor_id: sensorId,
        created_at: { [sequelize.Sequelize.Op.gt]: since }
      },
      order: [['created_at', 'ASC']]
    });

    // Group by intervals
    const intervalMs = intervalMinutes * 60 * 1000;
    const grouped = {};
    
    logs.forEach(log => {
      const intervalStart = Math.floor(log.created_at.getTime() / intervalMs) * intervalMs;
      if (!grouped[intervalStart]) {
        grouped[intervalStart] = [];
      }
      grouped[intervalStart].push(log);
    });

    // Average values within each interval
    return Object.keys(grouped).map(timestamp => {
      const intervalLogs = grouped[timestamp];
      const values = intervalLogs.map(log => this.extractValue(log.data)).filter(v => v !== null);
      const avgValue = values.length > 0 ? values.reduce((a, b) => a + b) / values.length : null;
      
      return {
        time: new Date(parseInt(timestamp)),
        value: avgValue,
        count: intervalLogs.length
      };
    });
  };

  // Helper method to extract numeric value from data
  Log.extractValue = function(data) {
    if (data === null || data === undefined) return null;
    
    // Handle different data formats
    if (typeof data === 'number') return data;
    if (typeof data === 'string') {
      const num = parseFloat(data);
      return isNaN(num) ? null : num;
    }
    if (typeof data === 'object') {
      // Try common field names
      if (data.value !== undefined) return parseFloat(data.value) || null;
      if (data.value1 !== undefined) return parseFloat(data.value1) || null;
      if (data.value2 !== undefined) return parseFloat(data.value2) || null;
      if (data.temperature !== undefined) return parseFloat(data.temperature) || null;
      if (data.temp !== undefined) return parseFloat(data.temp) || null;
      if (data.humidity !== undefined) return parseFloat(data.humidity) || null;
      if (data.moisture !== undefined) return parseFloat(data.moisture) || null;
      if (data.light !== undefined) return parseFloat(data.light) || null;
      
      // If no known fields, try to extract the first numeric value
      const values = Object.values(data);
      for (const val of values) {
        const num = parseFloat(val);
        if (!isNaN(num)) return num;
      }
    }
    
    return null;
  };

  // Associations
  Log.associate = function(models) {
    Log.belongsTo(models.Sensor, { 
      foreignKey: 'sensor_id',
      as: 'sensor'
    });
  };

  return Log;
};