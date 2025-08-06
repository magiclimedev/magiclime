const { SerialPort } = require('serialport')
const { ReadlineParser } = require('@serialport/parser-readline');
const lib = require("./lib.js");
const {
    list
} = require('serialport');


var serialPortName;
var disconnected = true;
var bootup = true;
var currentSerialPort = null;
var currentParser = null;

/*
 * Set up Serial Port. Reconnect when unplugged/plugged
 *
 */
setInterval(async () => {
    await getSerialPort(function(error, result) {
        if (error) {
            if (bootup) {
                const formatter = lib.createFormatter(20);
                formatter("Serial", "Receiver not connected");
                bootup = false;
            }

        } else {
            serialPortName = result;

            if (disconnected) {
                const formatter = lib.createFormatter(20);
                formatter("Serial", `Started on ${serialPortName}`);
                
                // Close existing serial port if it exists
                if (currentSerialPort && currentSerialPort.isOpen) {
                    try {
                        currentSerialPort.close();
                    } catch (error) {
                        console.error("Error closing existing serial port:", error);
                    }
                }

                currentSerialPort = new SerialPort({
                    path: serialPortName,
                    baudRate: 57600,
                    autoOpen: false
                });
                
                currentParser = new ReadlineParser();
                currentSerialPort.pipe(currentParser);
                
                currentParser.on('data', function(data) {
                    if (global.rxQueue) {
                        global.rxQueue.enqueue(data);
                    }
                });

                currentSerialPort.open(function (err){
                    if (err) {
                        console.error("Error opening serial port:", err.message);
                        disconnected = true;
                        return;
                    }
                    
                    // Forward receiver connected status to web server
                    if (global.forwardReceiverStatus) {
                        global.forwardReceiverStatus('connected');
                    }
                });

                currentSerialPort.on('close', function() {
                    console.log("Serial server:   receiver disconnected");
                    disconnected = true;
                    
                    // Forward receiver status to web server
                    if (global.forwardReceiverStatus) {
                        global.forwardReceiverStatus('disconnected');
                    }
                });
                
                currentSerialPort.on('error', function(err) {
                    console.error("Serial port error:", err.message);
                    disconnected = true;
                    
                    // Forward receiver status to web server
                    if (global.forwardReceiverStatus) {
                        global.forwardReceiverStatus('disconnected');
                    }
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
    try {
        const ports = await SerialPort.list();
        const ftdiPort = ports.find(port => port.manufacturer === "FTDI");
        
        if (ftdiPort) {
            callback(undefined, ftdiPort.path);
        } else {
            callback("Receiver not found", undefined);
        }
    } catch (error) {
        console.error("Error listing serial ports:", error);
        callback("Error listing ports: " + error.message, undefined);
    }
}

// Cleanup on process exit
process.on('exit', () => {
    if (currentSerialPort && currentSerialPort.isOpen) {
        currentSerialPort.close();
    }
});

process.on('SIGINT', () => {
    if (currentSerialPort && currentSerialPort.isOpen) {
        currentSerialPort.close(() => {
            process.exit();
        });
    } else {
        process.exit();
    }
});