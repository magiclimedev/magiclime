const ddl = require('./ddl.js');
const defaultSettings = require('./defaultSettings');
var db;

module.exports = {
  initialize: async function(filename){

      db = require("better-sqlite3")(filename);
      const info = db.exec(ddl.schema);

      this.initializeSettings(db)
  },
  initializeSettings:function(db){
    let checkStmt = db.prepare(
      "SELECT * FROM settings"
    );
    let previousSettings = checkStmt.all();

    if (previousSettings.length>0) {
      return
    }

    for (let settingKey in defaultSettings){

      const setting = defaultSettings[settingKey];

      const updateStmt = db.prepare(
        `INSERT INTO settings(id,name,value) VALUES (null,'${settingKey}','${String(setting.value)}')`
      )
      let info = updateStmt.run()
    }

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
      (NULL, 
      '${obj.uid}',
      '${obj.uid}',
      '${obj.sensor}',
      strftime('%s', 'now'),
      strftime('%s', 'now')
      )     
      `
    
    let stmt = db.prepare(sql);
    var info = stmt.run();
    //return rowID from newly inserted sensor
    return info.lastInsertRowid;
  },

  updateLastSeen: function(obj, id){
    var sql = `
              UPDATE sensor
              SET last_seen = strftime('%s', 'now')
              WHERE
                id = '${id}'
    `
    let stmt = db.prepare(sql);
    stmt.run();

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
    (NULL, 
      '${record.sensor_id}',
      '${record.rss}',
      '${record.bat}',
      '${record.data}',
      strftime('%s', 'now')
      )     
      `

    let stmt = db.prepare(sql);
    stmt.run();
  },

  checkUid: function (id) {
    let stmt = db.prepare(
      "SELECT id, name FROM sensor WHERE  uid = '" + id + "'"
    );
    let result = stmt.get();

    if (result === undefined) {
      return false;
    } else {
      //return true;
      return result.id;
    }
  },

  getSensorName: function (id) {
    let stmt = db.prepare(
      "SELECT name FROM sensor WHERE uid = '" + id + "'"
    );
    let sensorName = stmt.get();

    if (sensorName === undefined) {
      return id;
    } else {
      return sensorName;
    }
  },

  updateSensorName: function (uid, name) {
    var sql = `
              UPDATE 
                    sensor
              SET 
                    name = '${name}'
              WHERE
                    uid = '${uid}'
    `
    let stmt = db.prepare(sql);
    stmt.run();
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
              uid = '${uid}'
      `
    );
    let sensor = stmt.get();

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
                  sensor.uid =  '${uid}'
                ORDER BY 
                  log.id 
                DESC 
                  limit 10`);

    let sensorLog = stmt.all();

    if (sensorLog === undefined) {
      return "error";
    } else {
      return sensorLog;
    }
  },
  updateSetting: function(name,value){
    value = String(value)
    const sql = db.prepare(`
      UPDATE settings
      SET value = '${value}'
      WHERE 
        name = '${name}';
    `)
    sql.run()
    const getStmt = db.prepare(`
    SELECT * from settings;
    `)
    const res = getStmt.all()
    defaultSettings[name].value = value

  }
  ,
  getAllSettings: function () {
    let sql = db.prepare(`
    SELECT * from settings
    `)
    const dbSettingsList = sql.all();
    const settingsArray = []
    for (let settingName in defaultSettings){
      defaultSettings[settingName].loadDBValue(dbSettingsList)
      settingsArray.push(defaultSettings[settingName])
    }
    return settingsArray;
  }
,
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
