var ipc = require("node-ipc");
var menu = require("node-menu");

ipc.config.id = "magiclime";
ipc.config.retry = 1000;
ipc.config.silent = "true";
ipc.config.socketRoot = "./";
ipc.config.appspace = "ipc.";

var rss = "087";

//data structure: ss_|id|sensId(6)|sensType|bat|data

const button = "1|AABBCC|1|3.4|PUSH";
const namedButton = "1|XXYYZZ|1|3.7|PUSH";
const tiltOpen = "1|92H3SY|7|4.0|Open";
const tiltClosed = "1|92H3SY|7|4.0|Closed";
const namedMotion = "1|S8H59S|2|3.8|MOTION";
const namedTemp = "1|3Y4D84|3|2.9|74.8";

//remaining 
// 0 : "Presence",
// 4 : "Temp/Hum/Light",
// 5 : "Knock",
// 6 : "Clap",
// 8 : "Light",
// 9 : "Shake",


const sensorActions = [
  { 
    name: "Unnamed Button", 
    payload: button, 
    message: "Unnamed Button pushed" 
  },
  {
    name: "Named Button",
    payload: namedButton,
    message: "Named button pushed",
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
    name: "Named Temp", 
    payload: namedTemp, 
    message: "Named Temp sent" 
  },
  { 
    name: "Named Motion", 
    payload: namedMotion, 
    message: "Named Motion sent" },
];

ipc.connectTo("magiclime");

ipc.of.magiclime.on("message", function (data) {
  console.log(data);
});

ipc.of.magiclime.on("disconnect", function () {
  const emptyLine = ''.padEnd(process.stdout.columns, ' ');
  process.stdout.write(emptyLine + "\r");
  process.stdout.write("Disconnected from magiclime\r");
});

ipc.of.magiclime.on("connect", function () {
  const emptyLine = ''.padEnd(process.stdout.columns, ' ');
  process.stdout.write(emptyLine + "\r");
  process.stdout.write("Connected to magiclime\r");
});

function getChecksum(data) {
  var cs = 0;
  for (let character of data) {
    cs = cs ^ character.charCodeAt(0);
  }

  return String.fromCharCode(cs);
}

menu.addDelimiter("-", 80, "");

sensorActions.forEach((action) => {
  menu.addItem(action.name, () => {
    const checksum = getChecksum(action.payload);
    const fullSensorData = `${rss}${action.payload}${checksum}`;
    ipc.of.magiclime.emit("message", fullSensorData);
  });
});
menu
  .customHeader(function () {
    process.stdout.write("Send dummy sensor data to MagicLime Gateway\n");
  })
  // .disableDefaultHeader()
  .customPrompt(function () {
    process.stdout.write("\nSelect a sensor action:\n");
  })
  // .disableDefaultPrompt()
  .start();
