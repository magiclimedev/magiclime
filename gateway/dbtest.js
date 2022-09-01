const database = require("./db.js");

var sensors = database.getAllSensors()
console.log(sensors);
console.log("Print this first");
