const { SerialPort } = require('serialport')
const { ReadlineParser } = require('@serialport/parser-readline');

const {
    list
} = require('serialport');
const websocket = require("./web/websocket.js");
const { obj } = require('pumpify');

const globals = require('./globals.js');


var serialPortName;
var disconnected = true;
var bootup = true;

/*
 * Set up Serial Port. Reconnect when unplugged/plugged
 *
 */
setInterval(async () => {
    await getSerialPort(function(error, result) {
        if (error) {
            //console.log("Error: " + error);
            //console.log("Exiting!");
            //process.exit(1);
            if (bootup) {
                console.log("Serial server:   Receiver not connected");
                bootup = false;
                globals.receiverStatus = "DIS";
            }

        } else {
            serialPortName = result;

            if (disconnected) {
                console.log("Serial server:   started on " + serialPortName);
                globals.receiverStatus = "CON";
                var obj = new Object();
                obj.channel = "RECEIVER";
                obj.status = "CON";
                websocket.wsBroadcast(JSON.stringify(obj));
                
                var serialPort = new SerialPort({
                    path: serialPortName,
                    baudRate: 57600
                    //autoOpen: false
                })
                const parser = new ReadlineParser();
                serialPort.pipe(parser);
                parser.on('data', function(data) {
                    rxQueue.enqueue(data);
                });

                serialPort.on('close', function() {
                    console.log("Serial server:   receiver disconnected");
                    disconnected = true;
                    globals.receiverStatus = "DIS";
                    var obj = new Object();
                    obj.channel = "RECEIVER";
                    obj.status = "DIS";
                    websocket.wsBroadcast(JSON.stringify(obj));
                });
                disconnected = false;
                bootup = false;
            }
        }
    });
}, 1000);

/*
 * Return  serial port the receiver is plugged in
 *
 */
async function getSerialPort(callback) {
    var path;
    await SerialPort.list().then(function(ports) {
        ports.forEach(function(port) {
            if (port.manufacturer === "FTDI") {
                //console.log(port.path);
                path = port.path;
            }
        });
    });

    if (path)
        callback(undefined, path);
    else
        callback("Receiver not found", undefined);
}
