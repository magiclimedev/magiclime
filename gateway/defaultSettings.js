const Setting = require("./classes/Setting.js")


const defaultSettings = {}
new Setting(defaultSettings,"externalMqtt",Boolean,{defaultValue:false})
new Setting(defaultSettings,"externalMqttIp",String,{condition:"externalMqtt"})

module.exports = defaultSettings