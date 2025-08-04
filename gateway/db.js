const ddl = require('./ddl.js');
var db;



module.exports = {
  db,
  initialize: async function(filename){

      db = require("better-sqlite3")(filename);
      const info = db.exec(ddl.schema);
      return db
  },
  log: function (obj) {
    
    
    const uid = obj.uid;

    // Call this.checkUid(uid) and get the returned rowID if the record exists
    var id = this.checkUid(uid);
    
    // if sensor not found, create it
    if (id == false) {
      id = this.insertSensor(obj);
    }
    // log sensor event
      this.updateLastSeen(obj, id);
      this.insertLog(obj, id);
  },

  insertSensor: function (obj) {
    let sql = `INSERT INTO sensor(id, uid, name, type, first_seen, last_seen) VALUES 
      (NULL, ?, ?, ?, strftime('%s', 'now'), strftime('%s', 'now'))`
    
    let stmt = db.prepare(sql);
    var info = stmt.run(obj.uid, obj.uid, obj.sensor);
    //return rowID from newly inserted sensor
    return info.lastInsertRowid;
  },

  updateLastSeen: function(obj, id){
    var sql = `UPDATE sensor SET last_seen = strftime('%s', 'now') WHERE id = ?`
    let stmt = db.prepare(sql);
    stmt.run(id);
  },

  insertLog: function (obj, id) {
    let record = {
      sensor_id : id,
      rss : obj.rss,
      sensor : obj.sensor,
      bat : obj.bat,
      data : obj.data
      // date_created: obj.timestamp
    }

    var sql = `INSERT INTO log(id, sensor_id, rss, bat, data, date_created) VALUES 
    (NULL, ?, ?, ?, ?, strftime('%s', 'now'))`

    let stmt = db.prepare(sql);
    stmt.run(record.sensor_id, record.rss, record.bat, record.data);
  },

  checkUid: function (id) {
    let stmt = db.prepare("SELECT id, name FROM sensor WHERE uid = ?");
    let result = stmt.get(id);

    if (result === undefined) {
      return false;
    } else {
      //return true;
      return result.id;
    }
  },

  getSensorName: function (id) {
    let stmt = db.prepare("SELECT name FROM sensor WHERE uid = ?");
    let sensorName = stmt.get(id);

    if (sensorName === undefined) {
      return id;
    } else {
      return sensorName;
    }
  },

  updateSensorName: function (uid, name) {
    var sql = `UPDATE sensor SET name = ? WHERE uid = ?`
    let stmt = db.prepare(sql);
    stmt.run(name, uid);
  },

  getSensor: function (uid) {
    let stmt = db.prepare(
      `SELECT 
              uid,
              name,
              type,
              datetime(first_seen, 'unixepoch', 'localtime') as first_seen,
              datetime(last_seen, 'unixepoch', 'localtime') as last_seen 
      FROM 
              sensor 
      WHERE 
              uid = ?`
    );
    let sensor = stmt.get(uid);

    if (sensor === undefined) {
      return uid;
    } else {
      return sensor;
    }
  },

  getSensorLog: function (uid) {
    let stmt = db.prepare(
                `SELECT 
                    datetime(log.date_created, 'unixepoch', 'localtime') as date_created,
                    --log.id, 
                    --log.sensor_id, 
                    log.rss, 
                    log.bat, 
                    log.data,
                    sensor.name, 
                    sensor.type  
                FROM log 
                    left join sensor on log.sensor_id = sensor.id 
                WHERE 
                  sensor.uid = ?
                ORDER BY 
                  log.id 
                DESC 
                  limit 10`);

    let sensorLog = stmt.all(uid);

    if (sensorLog === undefined) {
      return "error";
    } else {
      return sensorLog;
    }
  },
  getAllSensors: function () {
    // Mark sensor dead if not heard from in 3 hours
    let sql = db.prepare(`
    SELECT  uid, 
            name, 
            type, 
            datetime(last_seen, 'unixepoch', 'localtime') as last_seen,
    CASE	
            WHEN strftime('%s','now') - last_seen > 10800
              THEN 'dead'
            ELSE
              'active'
            END	
              status
    FROM 
            sensor
    `);
    const sensorList = sql.all();

    if (sensorList === undefined) {
      return undefined;
    } else {
      return sensorList;  
    }
  },
};

