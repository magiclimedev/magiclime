const { log } = require("console");
const express = require("express");
var path = require('path');
const { publishAllSensors, publishSensor } = require("../../../mqttclient.js");
const { getAllSettings } = require("./../../../db.js");
const database = require("./../../../db.js");

const router = express.Router();


router.get('/', function(req, res) {
        var sensors = database.getAllSensors();
        res.render('index', {
                sensors : sensors
              });
});

router.get('/settings', function(req, res) {
        const settings = getAllSettings()
        res.render('settings',{
                settings
        });
});

router.get('/mqtt', function(req, res) {
        res.render('mqtt');
});

router.get('/sensors/log', function(req, res) {
        const id = req.params.id
        const sensor = database.getSensor(id);
        res.render('log', {
                name : sensor.name,
                type : sensor.type,
                id: id
              });
});

router.get('/sensors/:uid', function(req, res) {
        const uid = req.params.uid
        const sensor = database.getSensor(uid);
        const sensorLog = database.getSensorLog(uid);
        
        res.render('details', {
                name : sensor.name,
                type : sensor.type,
                uid: uid,
                log: sensorLog
              });
});

router.post('/sensors/:uid', function(req, res) {        
        const uid = req.params.uid
        const name = req.body.name;
        const sensor = database.updateSensorName(uid, name);
        publishAllSensors()
        publishSensor(uid)
        res.json({ name: name })
});

router.post("/settings",async (req,res)=>{
        const {setting,value} = req.body;
        await database.updateSetting(setting,value)
})

module.exports = router;