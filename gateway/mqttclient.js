const mqtt = require('mqtt');
const { getAllSensors, getSensor, getSensorLog } = require('./db.js');
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
    publishAllSensors: function(){
        const client = mqtt.connect("mqtt://localhost");
        const mqttPacket = {
            topic:"magiclime/sensors",
            "message":getAllSensors()
        }
        const options = {
            qos: 0,
            retain:true
        };

        client.on("error", function(error) {
            console.error(error)
            console.log("Error: Could not connect or publish to MQTT server");
            client.end();
        });
        client.on("connect",()=>{
            client.publish(mqttPacket.topic,JSON.stringify(mqttPacket.message),options)
            client.end()
        })
    }
    ,
    publishSensor:function(uid){
        try{
            console.log("UID in pub",uid);
            const client = mqtt.connect("mqtt://localhost")
        const mqttPacket = {
            "topic":`magiclime/sensors/${uid}`,
            "message":getSensor(uid),
        }
        const options = {
            qos: 0,
            retain:true
        };
        client.on("error", function(error) {
            console.error(error)
            console.log("Error: Could not connect or publish to MQTT server");
            client.end();
        });
        client.on("connect",()=>{
            client.publish(mqttPacket.topic,JSON.stringify(mqttPacket.message),options)
            client.end()
        })
        return "success"
    }catch(err){
        console.log(err)
    }
    }
    ,
    publishSensorLog:(uid)=>{
        const client = mqtt.connect("mqtt://localhost")
        const mqttPacket = {
            "topic":`magiclime/sensors/${uid}/logs`,
            "message":getSensorLog(uid),
            "retain":true
        }
        const options = {
            qos: 0,
            retain:true
        };
        client.on("error", function(error) {
            console.error(error)
            console.log("Error: Could not connect or publish to MQTT server");
            client.end();
        });
        client.on("connect",()=>{
            client.publish(mqttPacket.topic,JSON.stringify(mqttPacket.message),options)
            client.end()
        })
    }
    ,
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

        client.on("connect", ()=> {

            client.publish(
                String(mqttPacket.topic),
                JSON.stringify(mqttPacket.message),
                options);
            const eventPacket = {...mqttPacket,topic:`magiclime/sensors/${obj.uid}/event`,eventType:obj.eventType||"pulse"}
            console.log(eventPacket);
            client.publish(String(eventPacket.topic),JSON.stringify(eventPacket.message),{qos:0,retain:eventPacket.eventType==="pulse"?false:true})
            console.log(this);
            this.publishSensorLog(mqttMessage.uid)
            this.publishSensor(mqttMessage.uid)
            this.publishAllSensors()
            client.end();
        });
    },
};