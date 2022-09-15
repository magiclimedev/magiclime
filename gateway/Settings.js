const {initialize} = require("./db");

const Settings = {
    async ensureInitialized(){
        const settings = await this.getAll();
        if(settings.externalMqtt){
            return
        }
        await this.update("externalMqtt","false"),
        await this.update("externalMqttIp","localhost")
    },
    async getAll(){
        const db = await initialize("gateway.db")
        let sql = db.prepare(`
        SELECT * from settings
        `);
        const allSettingsArray = sql.all();
        const allSettings = {}
        allSettingsArray.forEach(setting=>{
            allSettings[setting.name] = setting.value
        })
        return allSettings
    },
    async get(settingName){
        const db = await initialize("gateway.db")
        let stmt = `
        SELECT * from settings
        WHERE name = '${settingName}'
        `
        let sql = db.prepare(stmt);
        const setting = sql.get();
        return setting
    },
    async update(settingName,settingValue){
        const db = await initialize("gateway.db")
        let result;
        const existingSettingValue = await this.get(settingName)
        if (existingSettingValue){
            const sql = db.prepare(`
            UPDATE settings
            SET value = '${settingValue}'
            WHERE 
            name = '${settingName}';
            `);
            result = sql.run()
        }else{
            const sql = db.prepare(
                `
                INSERT INTO settings(name,value)
                VALUES ('${settingName}','${settingValue}')
                `)
                result = sql.run()
        }
        return result
    }
}

module.exports = {Settings}