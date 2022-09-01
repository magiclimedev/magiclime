const mqtt = require('mqtt');
const lib = require('./lib.js');

module.exports = {

    /*
     * checkMQTTServer()
     * TODO Implement
     */
    
    checkMQTTServer: function() {   
        var client  = mqtt.connect("mqtt://localhost");
        console.log("MQTT Broker:     found on localhost:1883")
    },

    /*
     * Publish data to MQTT server
     *
     */
    forwardToMQTT: async function(obj) {
        var mqttPacket = {};

        var mqttMessage = {
            "uid": obj.uid,
            "rss": obj.rss,
            "bat": obj.bat,
            "type": obj.type,
            "data": obj.data
        }

        var mqttPacket = {
            "topic": "magiclime",
            "message": mqttMessage
        }

        var client = mqtt.connect("mqtt://localhost");

        /* MQTT options */
        var options = {
            qos: 0
        };

        client.on("error", function(error) {
            console.log("Error: Could not connect or publish to MQTT server");
            client.end();
        });

        client.on("connect", function() {
            client.publish(
                String(mqttPacket.topic),
                JSON.stringify(mqttPacket.message),
                options);
            client.end();
        });
    }
};
