'use strict';

module.exports = (sequelize, DataTypes) => {
  const Sensor = sequelize.define('Sensor', {
    uid: { 
      type: DataTypes.STRING, 
      allowNull: false 
    },
    name: {
      type: DataTypes.STRING,
      allowNull: true
    },
    type: { 
      type: DataTypes.STRING, 
      allowNull: false 
    },
    first_seen: {
      type: DataTypes.DATE,
      allowNull: true
    },
    last_seen: {
      type: DataTypes.DATE,
      allowNull: true
    }
  }, {
    tableName: 'sensor',
    underscored: true,
    indexes: [
      {
        unique: true,
        fields: ['gateway_id', 'uid']
      }
    ]
  });

  // Static methods
  Sensor.findOrCreateByUid = async function(gatewayId, uid, type) {
    const [sensor, created] = await this.findOrCreate({
      where: { 
        gateway_id: gatewayId, 
        uid: uid 
      },
      defaults: { 
        type: type,
        name: uid,
        first_seen: new Date(),
        last_seen: new Date()
      }
    });
    
    if (!created) {
      await sensor.update({ last_seen: new Date() });
    }
    
    return sensor;
  };

  Sensor.updateName = async function(sensorId, newName) {
    const [updatedRows] = await this.update(
      { name: newName },
      { where: { id: sensorId } }
    );
    return updatedRows > 0;
  };

  Sensor.getActiveByGateway = async function(gatewayId) {
    const threeHoursAgo = new Date(Date.now() - 3 * 60 * 60 * 1000);
    return await this.findAll({
      where: {
        gateway_id: gatewayId,
        last_seen: { [sequelize.Sequelize.Op.gt]: threeHoursAgo }
      },
      order: [['name', 'ASC']]
    });
  };

  Sensor.getAllByGateway = async function(gatewayId) {
    const threeHoursAgo = new Date(Date.now() - 3 * 60 * 60 * 1000);
    const sensors = await this.findAll({
      where: { gateway_id: gatewayId },
      order: [['name', 'ASC']]
    });
    
    // Add status field
    return sensors.map(sensor => {
      const sensorData = sensor.toJSON();
      sensorData.status = sensor.last_seen > threeHoursAgo ? 'active' : 'dead';
      return sensorData;
    });
  };

  Sensor.logData = async function(sensorId, data) {
    const transaction = await sequelize.transaction();
    try {
      // Update sensor last_seen
      await this.update(
        { last_seen: new Date() },
        { 
          where: { id: sensorId }, 
          transaction 
        }
      );
      
      // Create log entry
      const log = await sequelize.models.Log.create({
        sensor_id: sensorId,
        rss: data.rss,
        bat: data.bat,
        data: data.data,
        path: data.path,
        pathIndicator: data.pathIndicator
      }, { transaction });
      
      await transaction.commit();
      return log;
    } catch (error) {
      await transaction.rollback();
      throw error;
    }
  };

  Sensor.getByUidAndGateway = async function(gatewayId, uid) {
    return await this.findOne({
      where: {
        gateway_id: gatewayId,
        uid: uid
      }
    });
  };

  // Associations
  Sensor.associate = function(models) {
    Sensor.belongsTo(models.Gateway, { 
      foreignKey: 'gateway_id',
      as: 'gateway'
    });
    
    Sensor.hasMany(models.Log, { 
      foreignKey: 'sensor_id',
      as: 'logs'
    });
  };

  return Sensor;
};