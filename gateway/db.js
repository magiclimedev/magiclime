const Database = require('better-sqlite3');
const config = require('./config');
const logger = require('./logger');
const { DatabaseError, errorHandler } = require('./errors');
const { validateSensorData, sanitizeSensorData } = require('./validation');
const DatabaseAdapter = require('./db-adapter');

class SensorDatabase {
  constructor() {
    this.db = null;
    this.adapter = null;
    this.statements = {};
  }
  
  /**
   * Initialize database connection
   */
  initialize(filename = config.database.filename) {
    try {
      logger.info('Initializing database', { filename });
      
      this.db = new Database(filename, {
        verbose: config.debug.enabled ? logger.debug : null,
        timeout: config.database.busyTimeout
      });
      
      // Set pragma options for better performance and integrity
      this.db.pragma('journal_mode = WAL');
      this.db.pragma('synchronous = NORMAL');
      this.db.pragma('foreign_keys = ON');
      
      // Run migrations if needed
      this.checkAndMigrate();
      
      // Initialize adapter
      this.adapter = new DatabaseAdapter(this.db);
      
      // Prepare statements
      this.prepareStatements();
      
      logger.info('Database initialized successfully');
      return this.db;
      
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { filename });
    }
  }
  
  /**
   * Check schema version and run migrations if needed
   */
  checkAndMigrate() {
    try {
      // Check if sensor table exists
      const tableExists = this.db.prepare(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='sensor'"
      ).get();
      
      if (!tableExists) {
        logger.info('Creating initial database schema');
        this.createInitialSchema();
      } else {
        // Check schema version
        let currentVersion = 1.0;
        
        try {
          const versionRow = this.db.prepare(
            "SELECT value FROM settings WHERE name = 'schema_version'"
          ).get();
          
          currentVersion = versionRow ? parseFloat(versionRow.value) : 1.0;
        } catch (e) {
          // Settings table might not exist in old schema
          logger.debug('Settings table not found, assuming schema v1.0');
        }
        
        if (currentVersion < 2.1) {
          logger.info('Running database migration', { fromVersion: currentVersion });
          this.migrateToV21();
        }
      }
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'migration' });
    }
  }
  
  /**
   * Create initial database schema
   */
  createInitialSchema() {
    const schema = `
      CREATE TABLE IF NOT EXISTS sensor (
        id INTEGER PRIMARY KEY,
        uid TEXT NOT NULL UNIQUE,
        name TEXT,
        type TEXT NOT NULL,
        first_seen INTEGER NOT NULL,
        last_seen INTEGER NOT NULL,
        created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
        updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
      );
      
      CREATE INDEX IF NOT EXISTS sensor_uid ON sensor (uid);
      CREATE INDEX IF NOT EXISTS sensor_last_seen ON sensor (last_seen);
      
      CREATE TABLE IF NOT EXISTS log (
        id INTEGER PRIMARY KEY,
        sensor_id INTEGER NOT NULL,
        rss INTEGER NOT NULL,
        bat REAL NOT NULL,
        data TEXT NOT NULL,
        date_created INTEGER NOT NULL,
        FOREIGN KEY (sensor_id) REFERENCES sensor(id)
      );
      
      CREATE INDEX IF NOT EXISTS log_sensor_id ON log (sensor_id);
      CREATE INDEX IF NOT EXISTS log_date_created ON log (date_created);
      
      CREATE TABLE IF NOT EXISTS raw_log (
        id INTEGER PRIMARY KEY,
        uid TEXT NOT NULL,
        source TEXT NOT NULL,
        rss INTEGER,
        bat REAL,
        sensor_type TEXT,
        data TEXT,
        path TEXT,
        path_indicator TEXT,
        created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
      );
      
      CREATE INDEX IF NOT EXISTS raw_log_created_at ON raw_log (created_at DESC);
      CREATE INDEX IF NOT EXISTS raw_log_uid ON raw_log (uid);
      
      CREATE TABLE IF NOT EXISTS settings (
        id INTEGER PRIMARY KEY,
        name VARCHAR(56) UNIQUE NOT NULL,
        value VARCHAR(256),
        created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
        updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
      );
      
      INSERT INTO settings (name, value) VALUES ('schema_version', '2.1');
    `;
    
    this.db.exec(schema);
  }
  
  /**
   * Migrate to schema version 2.1 (add raw_log table)
   */
  migrateToV21() {
    try {
      const migration = `
        CREATE TABLE IF NOT EXISTS raw_log (
          id INTEGER PRIMARY KEY,
          uid TEXT NOT NULL,
          source TEXT NOT NULL,
          rss INTEGER,
          bat REAL,
          sensor_type TEXT,
          data TEXT,
          path TEXT,
          path_indicator TEXT,
          created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE INDEX IF NOT EXISTS raw_log_created_at ON raw_log (created_at DESC);
        CREATE INDEX IF NOT EXISTS raw_log_uid ON raw_log (uid);
        
        UPDATE settings SET value = '2.1' WHERE name = 'schema_version';
      `;
      
      this.db.exec(migration);
      logger.info('Database migrated to version 2.1');
      
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'migrateToV21' });
    }
  }
  
  /**
   * Prepare frequently used statements
   */
  prepareStatements() {
    this.statements = {
      checkUid: this.db.prepare('SELECT id, name FROM sensor WHERE uid = ?'),
      
      insertSensor: this.db.prepare(`
        INSERT INTO sensor (uid, name, type, first_seen, last_seen, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
      `),
      
      updateLastSeen: this.db.prepare(`
        UPDATE sensor 
        SET last_seen = ?, updated_at = ? 
        WHERE id = ?
      `),
      
      // insertLog will be set dynamically based on schema
      
      getSensor: this.db.prepare(`
        SELECT 
          uid,
          name,
          type,
          first_seen,
          last_seen,
          created_at,
          updated_at
        FROM sensor 
        WHERE uid = ?
      `),
      
      // getSensorLog will be set dynamically based on schema
      
      getAllSensors: this.db.prepare(`
        SELECT 
          uid,
          name,
          type,
          last_seen,
          CASE
            WHEN ? - last_seen > ?
            THEN 'dead'
            ELSE 'active'
          END as status
        FROM sensor
        ORDER BY last_seen DESC
      `),
      
      updateSensorName: this.db.prepare(`
        UPDATE sensor 
        SET name = ?, updated_at = ? 
        WHERE uid = ?
      `),
      
      insertRawLog: this.db.prepare(`
        INSERT INTO raw_log (uid, source, rss, bat, sensor_type, data, path, path_indicator)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
      `),
      
      getRawLogs: this.db.prepare(`
        SELECT * FROM raw_log 
        ORDER BY created_at DESC 
        LIMIT ?
      `),
      
      cleanupRawLogs: this.db.prepare(`
        DELETE FROM raw_log 
        WHERE id NOT IN (
          SELECT id FROM raw_log 
          ORDER BY created_at DESC 
          LIMIT ?
        )
      `)
    };
    
    // Add dynamic statements based on schema
    this.statements.insertLog = this.db.prepare(this.adapter.getInsertLogSQL());
    this.statements.getSensorLog = this.db.prepare(this.adapter.getSensorLogSQL());
  }
  
  /**
   * Log sensor data with transaction support
   */
  log(obj) {
    try {
      // Validate and sanitize input
      const sanitized = sanitizeSensorData(obj);
      const validated = validateSensorData(sanitized);
      
      const logContext = logger.child({ uid: validated.uid });
      
      // Use transaction for consistency
      const result = this.db.transaction(() => {
        const now = Math.floor(Date.now() / 1000);
        
        // Check if sensor exists
        let sensorId = this.checkUid(validated.uid);
        
        // Create sensor if not found
        if (!sensorId) {
          sensorId = this.insertSensor(validated, now);
          logContext.info('New sensor registered', { type: validated.label });
        }
        
        // Update last seen
        this.updateLastSeen(validated, sensorId, now);
        
        // Log sensor data
        this.insertLog(validated, sensorId, now);
        
        // Also log to raw log for web interface
        this.insertRawLog(validated);
        
        return { sensorId, logged: true };
      })();
      
      logContext.debug('Sensor data logged', { sensorId: result.sensorId });
      return result;
      
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'log', uid: obj?.uid });
    }
  }
  
  /**
   * Check if sensor exists by UID
   */
  checkUid(uid) {
    try {
      const result = this.statements.checkUid.get(uid);
      return result ? result.id : false;
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'checkUid', uid });
    }
  }
  
  /**
   * Insert new sensor
   */
  insertSensor(obj, timestamp) {
    try {
      const info = this.statements.insertSensor.run(
        obj.uid,
        obj.uid, // Default name to UID
        obj.label || 'Unknown',
        timestamp,
        timestamp,
        timestamp,
        timestamp
      );
      
      return info.lastInsertRowid;
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'insertSensor', uid: obj.uid });
    }
  }
  
  /**
   * Update sensor last seen timestamp
   */
  updateLastSeen(obj, sensorId, timestamp) {
    try {
      this.statements.updateLastSeen.run(timestamp, timestamp, sensorId);
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'updateLastSeen', sensorId });
    }
  }
  
  /**
   * Insert sensor log entry
   */
  insertLog(obj, sensorId, timestamp) {
    try {
      // Prepare data object
      const dataObj = {};
      if (obj.data1 !== undefined) dataObj.value1 = obj.data1;
      if (obj.data2 !== undefined) dataObj.value2 = obj.data2;
      
      // Add semantic labels based on sensor type
      if (obj.label === 'T-H') {
        dataObj.temperature = obj.data1;
        dataObj.humidity = obj.data2;
      }
      
      // Build parameters array based on schema
      const params = [
        obj.uid,  // Use sensor UID instead of database sensorId
        obj.rss || -100,
        obj.bat || 0,
        JSON.stringify(dataObj)
      ];
      
      // Add path information if schema supports it
      if (this.adapter.schema.log.hasPath) {
        params.push(obj._path || null);
      }
      
      if (this.adapter.schema.log.hasPathIndicator) {
        params.push(obj._pathIndicator || null);
      }
      
      if (this.adapter.schema.log.hasPathIndicatorSnake) {
        params.push(obj._pathIndicator || null); // Same value for snake_case column
      }
      
      // The adapter handles the timestamp automatically based on schema
      this.statements.insertLog.run(...params);
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'insertLog', sensorId });
    }
  }
  
  /**
   * Get sensor details
   */
  getSensor(uid) {
    try {
      const sensor = this.statements.getSensor.get(uid);
      
      if (!sensor) {
        return null;
      }
      
      // Convert timestamps to readable format
      return {
        ...sensor,
        first_seen: new Date(sensor.first_seen * 1000).toLocaleString(),
        last_seen: new Date(sensor.last_seen * 1000).toLocaleString(),
        created_at: new Date(sensor.created_at * 1000).toLocaleString(),
        updated_at: new Date(sensor.updated_at * 1000).toLocaleString()
      };
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'getSensor', uid });
    }
  }
  
  /**
   * Get sensor log entries
   */
  getSensorLog(uid, limit = 10) {
    try {
      const logs = this.statements.getSensorLog.all(uid, limit);
      
      // Parse JSON data and format timestamps using adapter
      return logs.map(log => ({
        ...log,
        date_created: this.adapter.formatDateValue(log.date_value),
        data: JSON.parse(log.data)
      }));
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'getSensorLog', uid });
    }
  }
  
  /**
   * Get all sensors with status
   */
  getAllSensors() {
    try {
      const now = Math.floor(Date.now() / 1000);
      const deadThreshold = 3 * 60 * 60; // 3 hours in seconds
      
      const sensors = this.statements.getAllSensors.all(now, deadThreshold);
      
      // Format timestamps
      return sensors.map(sensor => ({
        ...sensor,
        last_seen: new Date(sensor.last_seen * 1000).toLocaleString()
      }));
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'getAllSensors' });
    }
  }
  
  /**
   * Update sensor name
   */
  updateSensorName(uid, name) {
    try {
      const now = Math.floor(Date.now() / 1000);
      const info = this.statements.updateSensorName.run(name, now, uid);
      
      if (info.changes === 0) {
        throw new DatabaseError('Sensor not found', { uid });
      }
      
      logger.info('Sensor name updated', { uid, name });
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'updateSensorName', uid });
    }
  }
  
  /**
   * Get database statistics
   */
  getStats() {
    try {
      const stats = this.db.prepare(this.adapter.getStatsSQL()).get(
        Math.floor(Date.now() / 1000)
      );
      
      return stats;
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'getStats' });
    }
  }
  
  /**
   * Insert raw log entry (circular buffer)
   */
  insertRawLog(obj) {
    try {
      // Store the complete enhanced packet as JSON (keep _path and _pathIndicator for consistency with WebSocket)
      const rawPacket = { ...obj };
      delete rawPacket._lastPath; // Only remove _lastPath as it's internal
      
      // Insert new raw log entry with complete packet JSON
      this.statements.insertRawLog.run(
        obj.uid,
        obj.source,
        obj.rss || null,
        obj.bat || null,
        obj.label || obj.sensor || null,
        JSON.stringify(rawPacket), // Store complete enhanced packet with _path and _pathIndicator
        obj._path || null,
        obj._pathIndicator || null
      );
      
      // Keep only last 50 entries (circular buffer)
      this.statements.cleanupRawLogs.run(50);
      
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'insertRawLog', uid: obj.uid });
    }
  }
  
  /**
   * Get recent raw logs
   */
  getRawLogs(limit = 20) {
    try {
      const logs = this.statements.getRawLogs.all(limit);
      
      return logs.map(log => ({
        ...log,
        data: log.data ? JSON.parse(log.data) : null,
        created_at: log.created_at
      }));
    } catch (error) {
      throw errorHandler.from(error, DatabaseError, { operation: 'getRawLogs' });
    }
  }
  
  /**
   * Close database connection
   */
  close() {
    if (this.db) {
      logger.info('Closing database connection');
      this.db.close();
      this.db = null;
      this.statements = {};
    }
  }
}

// Export singleton instance
module.exports = new SensorDatabase();