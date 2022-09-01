const express = require("express");
var path = require('path');

const lib = require("./../../../lib");
const database = require("./../../../db.js");

const router = express.Router();

router.get('/', function(req, res) {
        //
        res.render('api', {
                // sensors : sensors
              });
});

router.get('/sensors', function(req, res) {
        res.send(database.getAllSensors());
});

router.get('/sensors/:uid', function(req, res) {
        const uid = req.params.uid
        //res.send('Sensor details: ' + id)
        res.send(database.getSensor(uid));
});

router.get('/sensors/:uid/logs', function(req, res) {
        const uid = req.params.uid
        //res.send('Sensor details: ' + id)
        res.send(database.getSensorLog(uid));
});

module.exports = router;