'use strict';

module.exports = {
  up: async (queryInterface, Sequelize) => {
    const tableInfo = await queryInterface.describeTable('log');
    
    // Add path column if it doesn't exist
    if (!tableInfo.path) {
      await queryInterface.addColumn('log', 'path', {
        type: Sequelize.STRING,
        allowNull: true
      });
    }
    
    // Add path_indicator column if it doesn't exist (Sequelize uses snake_case with underscored: true)
    if (!tableInfo.path_indicator) {
      await queryInterface.addColumn('log', 'path_indicator', {
        type: Sequelize.STRING,
        allowNull: true
      });
    }
  },

  down: async (queryInterface, Sequelize) => {
    await queryInterface.removeColumn('log', 'path');
    await queryInterface.removeColumn('log', 'path_indicator');
  }
};