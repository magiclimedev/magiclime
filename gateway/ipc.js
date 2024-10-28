const ipc = require('node-ipc').default;	   
const lib = require("./lib.js");


/* Set up IPC server for dummy data */
ipc.config.id = 'thingbits';
ipc.config.retry = 1500;
ipc.config.silent = 'true';
ipc.config.socketRoot = './';
ipc.config.appspace = 'ipc.';

ipc.serve();
ipc.server.start();


ipc.server.on('start', () => {
    const formatter = lib.createFormatter(20);
    formatter("IPC", `Started on ${ipc.config.socketRoot}${ipc.config.appspace}${ipc.config.id}`);
});

ipc.server.on('message', function(data, socket) {
    rxQueue.enqueue(data);
});

ipc.server.on('connect', function(socket) {
    //console.log('IPC client has connected!');
});

ipc.server.on('socket.disconnected', function(socket, destroyedSocketID) {
    console.log('IPC client has disconnected!');
});