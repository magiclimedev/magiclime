'use strict';

module.exports = (sequelize, DataTypes) => {
  const Gateway = sequelize.define('Gateway', {
    serial_num: { 
      type: DataTypes.STRING(6), 
      unique: true,
      allowNull: false 
    },
    name: {
      type: DataTypes.STRING,
      allowNull: true
    },
    last_seen: {
      type: DataTypes.DATE,
      allowNull: true
    }
  }, {
    tableName: 'gateway',
    underscored: true
  });

  // Static methods
  Gateway.generateUniqueSerialNum = async function() {
    // Generate 6-char alphanumeric serial number
    let serial;
    let exists = true;
    while (exists) {
      serial = Math.random().toString(36).substring(2, 8).toUpperCase();
      const existing = await this.findOne({ where: { serial_num: serial } });
      exists = !!existing;
    }
    return serial;
  };

  Gateway.findOrCreateBySerialNum = async function(serialNum) {
    // Find or create gateway by serial number
    const [gateway, created] = await this.findOrCreate({
      where: { serial_num: serialNum },
      defaults: { 
        name: `Gateway ${serialNum}`,
        last_seen: new Date()
      }
    });
    return gateway;
  };

  Gateway.updateLastSeen = async function(serialNum) {
    // Update last_seen timestamp
    return await this.update(
      { last_seen: new Date() },
      { where: { serial_num: serialNum } }
    );
  };

  Gateway.getCloudUrl = function(serialNum) {
    return `https://magiclime.io/${serialNum}`;
  };

  Gateway.getBySerialNum = async function(serialNum) {
    return await this.findOne({
      where: { serial_num: serialNum }
    });
  };

  // Associations
  Gateway.associate = function(models) {
    Gateway.hasMany(models.Sensor, { 
      foreignKey: 'gateway_id',
      as: 'sensors'
    });
    
    Gateway.hasMany(models.Setting, { 
      foreignKey: 'gateway_id',
      as: 'settings'
    });
    
    // For future user authentication
    // Gateway.belongsToMany(models.User, { 
    //   through: models.UserGateway,
    //   foreignKey: 'gateway_id',
    //   as: 'users'
    // });
  };

  return Gateway;
};