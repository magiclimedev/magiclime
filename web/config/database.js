const path = require('path');

module.exports = {
  local: {
    dialect: 'sqlite',
    storage: path.join(__dirname, '..', 'web.db'),
    logging: false,
    define: {
      underscored: true,
      freezeTableName: true
    }
  },
  cloud: {
    dialect: 'sqlite',
    storage: path.join(__dirname, '..', 'cloud.db'),
    logging: false,
    define: {
      underscored: true,
      freezeTableName: true
    }
    // For production cloud deployment, could switch to PostgreSQL:
    // dialect: 'postgres',
    // host: process.env.DB_HOST,
    // username: process.env.DB_USER,
    // password: process.env.DB_PASSWORD,
    // database: process.env.DB_NAME,
    // port: process.env.DB_PORT || 5432,
  },
  development: {
    dialect: 'sqlite',
    storage: path.join(__dirname, '..', 'development.db'),
    logging: console.log,
    define: {
      underscored: true,
      freezeTableName: true
    }
  },
  test: {
    dialect: 'sqlite',
    storage: ':memory:',
    logging: false,
    define: {
      underscored: true,
      freezeTableName: true
    }
  }
};