const logger = require('./logger');

/**
 * Database adapter for handling schema differences and SQL generation
 */
class DatabaseAdapter {
  constructor(db) {
    this.db = db;
    this.schema = this.detectSchema();
  }

  /**
   * Detect database schema to handle different column names
   */
  detectSchema() {
    const schema = {
      log: {
        hasPath: false,
        hasPathIndicator: false,
        hasPathIndicatorSnake: false
      }
    };

    try {
      // Check if path-related columns exist
      const columns = this.db.prepare("PRAGMA table_info(logs)").all();
      const columnNames = columns.map(col => col.name);
      
      schema.log.hasPath = columnNames.includes('path');
      schema.log.hasPathIndicator = columnNames.includes('pathIndicator');
      schema.log.hasPathIndicatorSnake = columnNames.includes('path_indicator');
      
      logger.debug('Database schema detected', schema);
    } catch (error) {
      logger.warn('Could not detect database schema', { error: error.message });
    }

    return schema;
  }

  /**
   * Get SQL for inserting log entries
   */
  getInsertLogSQL() {
    // Base parameters from db.js: [sensorId, rss, bat, JSON.stringify(dataObj)]
    let columns = 'uid, rss, bat, data';
    let placeholders = '?, ?, ?, ?';

    if (this.schema.log.hasPath) {
      columns += ', path';
      placeholders += ', ?';
    }

    if (this.schema.log.hasPathIndicator) {
      columns += ', path_indicator';
      placeholders += ', ?';
    } else if (this.schema.log.hasPathIndicatorSnake) {
      columns += ', path_indicator';
      placeholders += ', ?';
    }

    return `INSERT INTO raw_log (${columns}) VALUES (${placeholders})`;
  }

  /**
   * Get SQL for retrieving sensor logs
   */
  getSensorLogSQL() {
    return `
      SELECT * FROM raw_log 
      WHERE uid = ? 
      ORDER BY created_at DESC 
      LIMIT ?
    `;
  }

  /**
   * Get SQL for database statistics
   */
  getStatsSQL() {
    return `
      SELECT 
        COUNT(*) as total_logs,
        COUNT(DISTINCT uid) as unique_sensors,
        MAX(created_at) as latest_log,
        MIN(created_at) as earliest_log
      FROM raw_log
    `;
  }

  /**
   * Format date value for database storage
   */
  formatDateValue(dateValue) {
    if (!dateValue) {
      return new Date().toISOString();
    }
    
    if (dateValue instanceof Date) {
      return dateValue.toISOString();
    }
    
    if (typeof dateValue === 'string') {
      return new Date(dateValue).toISOString();
    }
    
    return new Date().toISOString();
  }
}

module.exports = DatabaseAdapter;