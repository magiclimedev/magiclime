const globals = require('../globals.js');

const WebSocketServer = require('ws');
const WS_PORT = 8080;

const server = require('http').createServer();
const app = require('./webserver');

const wss = new WebSocketServer.Server({
  // port: WS_PORT,
  // Create web socket server on top of a regular http server
  server: server,
});

// Also mount the app here
server.on('request', app);

const connections = new Array();

wss.on('open', () => {
  console.log('WebSocket server started ');
});

wss.on('connection', handleWSConnection);

function handleWSConnection(client) {
  // console.log('New WS Connection');
  connections.push(client);
  
  var obj = new Object();
  obj.channel = "RECEIVER";
  obj.status = globals.receiverStatus;

  if (connections.length > 0) {
    for (myConnection in connections) {
      connections[myConnection].send(JSON.stringify(obj));
    }
  }

  client.on('close', function () {
    // console.log('connection closed');
    let position = connections.indexOf(client);
    connections.splice(position, 1);
  });
}

server.listen(WS_PORT, () => {
  console.log(`http/ws server listening on port: ${WS_PORT}`);
});

module.exports = {
  wsBroadcast: function (msg) {
    if (connections.length > 0) {
      for (myConnection in connections) {
        connections[myConnection].send(msg);
      }
    }
  },
};
