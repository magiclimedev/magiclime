'use strict';

module.exports = (sequelize, DataTypes) => {
  const Setting = sequelize.define('Setting', {
    name: { 
      type: DataTypes.STRING, 
      allowNull: false 
    },
    value: {
      type: DataTypes.TEXT,
      allowNull: true
    }
  }, {
    tableName: 'setting',
    underscored: true,
    indexes: [
      {
        unique: true,
        fields: ['name', 'gateway_id']
      }
    ]
  });

  // Static methods
  Setting.get = async function(name, gatewayId = null) {
    const setting = await this.findOne({
      where: { 
        name: name, 
        gateway_id: gatewayId 
      }
    });
    return setting ? setting.value : null;
  };

  Setting.set = async function(name, value, gatewayId = null) {
    const [setting, created] = await this.findOrCreate({
      where: { 
        name: name, 
        gateway_id: gatewayId 
      },
      defaults: { value: value }
    });
    
    if (!created) {
      await setting.update({ value: value });
    }
    
    return setting;
  };

  Setting.getAll = async function(gatewayId = null) {
    const settings = await this.findAll({
      where: { gateway_id: gatewayId }
    });
    
    return settings.reduce((obj, setting) => {
      obj[setting.name] = setting.value;
      return obj;
    }, {});
  };

  Setting.ensureDefaults = async function() {
    const defaults = {
      'externalMqtt': 'false',
      'externalMqttIp': 'localhost'
    };
    
    for (const [name, value] of Object.entries(defaults)) {
      await this.findOrCreate({
        where: { 
          name: name, 
          gateway_id: null 
        },
        defaults: { value: value }
      });
    }
  };

  Setting.getGlobal = async function(name) {
    return await this.get(name, null);
  };

  Setting.setGlobal = async function(name, value) {
    return await this.set(name, value, null);
  };

  Setting.getForGateway = async function(name, gatewayId) {
    // First try gateway-specific setting, then fall back to global
    let value = await this.get(name, gatewayId);
    if (value === null) {
      value = await this.get(name, null);
    }
    return value;
  };

  Setting.delete = async function(name, gatewayId = null) {
    const deletedCount = await this.destroy({
      where: {
        name: name,
        gateway_id: gatewayId
      }
    });
    return deletedCount > 0;
  };

  Setting.getAllWithFallback = async function(gatewayId) {
    // Get all global settings
    const globalSettings = await this.getAll(null);
    
    // Get gateway-specific settings and override globals
    if (gatewayId) {
      const gatewaySettings = await this.getAll(gatewayId);
      return { ...globalSettings, ...gatewaySettings };
    }
    
    return globalSettings;
  };

  // Associations
  Setting.associate = function(models) {
    Setting.belongsTo(models.Gateway, { 
      foreignKey: 'gateway_id',
      allowNull: true,
      as: 'gateway'
    });
  };

  return Setting;
};