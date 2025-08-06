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

  Log.getChartData = async function(sensorId, hours = 24) {
    const logs = await this.getRecentBySensor(sensorId, hours);
    const chartData = logs.map(log => ({
      time: log.created_at,
      value: this.extractValue(log.data),
      rss: log.rss,
      bat: log.bat
    }));
    
    // Reverse to show oldest to newest (left to right chronologically)
    return chartData.reverse();
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
      if (data.temperature !== undefined) return parseFloat(data.temperature) || null;
      if (data.temp !== undefined) return parseFloat(data.temp) || null;
      if (data.humidity !== undefined) return parseFloat(data.humidity) || null;
      if (data.moisture !== undefined) return parseFloat(data.moisture) || null;
      if (data.light !== undefined) return parseFloat(data.light) || null;
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