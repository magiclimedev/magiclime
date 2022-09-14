const { log } = require("console");
const express = require("express");
var path = require('path');
const { publishAllSensors, publishSensor } = require("../../../mqttclient.js");
const {Settings} = require("./../../../Settings")
const database = require("./../../../db.js");
const router = express.Router();
router.get('/', function(req, res) {
        var sensors = database.getAllSensors();
        res.render('index', {
                sensors : sensors
              });
});

router.get('/settings',async function(req, res) {
        const settings = await Settings.getAll()
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
        const {clientObj} = require("../../../gateway.js")
        const {mqttClientList} = clientObj
        const {setting,value} = req.body;
        const resp = await Settings.update(setting,value);
        if(setting!=="externalMqtt"||setting!=="externalMqttIp"){
                return
        }
        else if(setting==="externalMqtt"&&value=="false"){
                mqttClientList.clear()
              }
        else if(setting==="externalMqtt"&&value=="true"){
                mqttClientList.replace(Settings.get("externalMqttIp")) 
              }
        else if(setting==="externalMqttIp"){
                mqttClientList.replace(value)
        }
})

module.exports = router;