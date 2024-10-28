const mqtt = require('mqtt');
const { getAllSensors, getSensor, getSensorLog, getSensorName } = require('./db.js');
const lib = require('./lib.js');

class MQTTClient{
    
    constructor(host){
        this.client = mqtt.connect(`mqtt://${host}`);
        this.options = {
            qos: 0
        }
        this.host = host
    }
    
    checkMQTTServer() {   
        const formatter = lib.createFormatter(20);
        var client  = mqtt.connect(`mqtt://${this.host}`);
        formatter("MQTT", `Broker found on ${this.host}:1883`)
    }
    

    publishAllSensors(){
        const client = mqtt.connect(`mqtt://${this.host}`);
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
    
    publishSensor(uid){
        try{
            const client = mqtt.connect(`mqtt://${this.host}`);
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

async publishSensorLog(uid){
    const client = mqtt.connect(`mqtt://${this.host}`)
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
};
forwardToMQTT(obj){
    const host = this.host

    const mqttMessage = {
        "uid": obj.uid,
        "rss": obj.rss,
        "bat": obj.bat,
        "type": obj.type,
        "data": obj.data,
        "name": getSensorName(obj.uid).name
    }
    
    const mqttPacket = {
        "topic": "magiclime",
        "message": mqttMessage
    }

    const client = mqtt.connect("mqtt://"+this.host);

    client.on("error", function(error) {
        console.log("Error: Could not connect or publish to MQTT server at "+host);
        client.end();
    });
    
    client.on("connect", ()=> {
        
        client.publish(
            String(mqttPacket.topic),
            JSON.stringify(mqttPacket.message),
            this.options);
            const eventPacket = {...mqttPacket,topic:`magiclime/sensors/${obj.uid}/event`,eventType:obj.eventType||"pulse"}
            client.publish(String(eventPacket.topic),JSON.stringify(eventPacket.message),{qos:0,retain:eventPacket.eventType==="pulse"?false:true})
            this.publishSensorLog(mqttMessage.uid)
            this.publishSensor(mqttMessage.uid)
            this.publishAllSensors()
            client.end();
        });
    }
}

class MQTTClientList{
    constructor(){
        this.clientList = [new MQTTClient("localhost")];
    }
    add(host){
        this.clientList.push(new MQTTClient(host))
    }
    clear(){
        this.clientList = [this.clientList.find(client=>client.host==="localhost")];
    }
    // this method can take a single ip address as a string or an array of ip address strings
    replace(hosts){
        this.clientList = [this.clientList.find(client=>client.host==="localhost")];
        if (typeof hosts === "string"){
            this.add(hosts)
        }
        else{
            hosts.forEach(host=>{
                this.add(host)
            })
        }
    }
    checkMQTTServers(){
        this.clientList.forEach(client=>client.checkMQTTServer())
    }
    publishAllSensors(){
        this.clientList.forEach(client=>client.publishAllSensors()) 
    }
    publishSensor(uid){
        this.clientList.forEach(client=>client.publishSensor(uid)) 
    }
    publishSensorLog(uid){
        this.clientList.forEach(client=>client.publishSensorLog(uid)) 
    }
    forwardToMQTT(obj){
        this.clientList.forEach(client=>client.forwardToMQTT(obj)) 
    }
}

module.exports = MQTTClientList