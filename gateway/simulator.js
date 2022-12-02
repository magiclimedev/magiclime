const ipc = require('node-ipc').default;	   
var menu = require("node-menu");

ipc.config.id = "thingbits";
ipc.config.retry = 1000;
ipc.config.silent = "true";
ipc.config.socketRoot = "./";
ipc.config.appspace = "ipc.";

var rss = "087";

const button = `{"source":"tx","rss":80,"uid":"XDCF98","sensor":"BUTTON","bat":3.0,"data":"PUSH"}`;
const tiltOpen = `{"source":"tx","rss":80,"uid":"92H3SY","sensor":"TILT","bat":2.7,"data":"HIGH"}`;
const tiltClosed = `{"source":"tx","rss":81,"uid":"92H3SY","sensor":"TILT","bat":2.7,"data":"LOW"}`;
const motion = `{"source":"tx","rss":82,"uid":"S8H59S","sensor":"MOTION","bat":2.6,"data":"MOTION"}`;
const temp = `{"source":"tx","rss":54,"uid":"3Y4D84","sensor":"TH","bat":2.7,"data":"72.5:60"}`;

const sensorActions = [
  { 
    name: "Push Button", 
    payload: button, 
    message: "Button pushed" 
  },
  { 
    name: "Tilt Open", 
    payload: tiltOpen, 
    message: "Tilt is in OPEN position" 
  },
  {
    name: "Tilt Closed",
    payload: tiltClosed,
    message: "Tilt is in CLOSED position",
  },
  { 
    name: "Temp/Humidity", 
    payload: temp, 
    message: "Temp/Hum sent" 
  },
  { 
    name: "Motion", 
    payload: motion, 
    message: "Motion sent" },
];

ipc.connectTo("thingbits");

ipc.of.thingbits.on("message", function (data) {
  console.log(data);
});

ipc.of.thingbits.on("disconnect", function () {
  const emptyLine = ''.padEnd(process.stdout.columns, ' ');
  process.stdout.write(emptyLine + "\r");
  process.stdout.write("Disconnected from thingbits\r");
});

ipc.of.thingbits.on("connect", function () {
  const emptyLine = ''.padEnd(process.stdout.columns, ' ');
  process.stdout.write(emptyLine + "\r");
  process.stdout.write("Connected to thingbits\r");
  //ipc.of.thingbits.emit('message', data);
});

menu.addDelimiter("-", 80, "");

sensorActions.forEach((action) => {
  menu.addItem(action.name, () => {
    const fullSensorData = `${action.payload}`;
    ipc.of.thingbits.emit("message", fullSensorData);
  });
});
menu
  .customHeader(function () {
    process.stdout.write("Simulator to send dummy sensor data to ThingBits Gateway\n");
  })
  .customPrompt(function () {
    process.stdout.write("\nSelect a sensor action:\n");
  })
  .start();