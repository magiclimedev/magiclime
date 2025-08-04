const lib = require("./lib.js");
const aedes = require('aedes')()
const server = require('net').createServer(aedes.handle)
const port = 1883

server.listen(port, function () {
  const formatter = lib.createFormatter(20);
  formatter("MQTT Server", `Started and listening on port ${port}`);
})
