const { updateSetting } = require("../db");
const db = require("../db")

const translations = {
    "Mqtt":"MQTT",
    "Ip":"IP"
}

class Setting{
    constructor(parent,name,type,options){
        this.name = name;
        this.value = options?.defaultValue!==undefined?options.defaultValue:null;
        this.type = type;
        this.condition = options?.condition; 
        this.parent = parent
        this.dependents = [];
        parent[name] = this;
        if(this.condition){
            parent[this.condition].dependents.push(name)
        }
    }
    update(value){
        updateSetting(value)
    }
    htmlClassAdd(){
        if(!this.condition){
            return ""
        }
        else if (this.parent[this.condition].convertFromString()){
            return "switch on"
        }
        return "switch"
    }
    loadDBValue(rows){
        const row = rows.find(row=>row.name===this.name);
        this.value = row.value
    }
    convertFromString(){
        if(this.type===String){
            return this.value
        }
        else if(this.type===Number){
            return Number(this.value)
        }
        else if (this.type ===Boolean){
            return this.value==="false"?false:true
        }
    }
    htmlInputType(){
        switch (this.type){
            case Boolean:
                return "checkbox"
            case Number:
                return "number"
            default:
                return "text"
        }
    }
    formatName(){
        let added = "";
        let remaining = this.name;
        let arr = []
        for(let i=0;i<this.name.length;i++){
            const isUpperCase = (this.name[i] >= 'A') && (this.name[i] <= 'Z');
            if(!isUpperCase){
                continue
            }
            const before = remaining.substring(0,remaining.indexOf(this.name[i]));
            arr.push(before)
            remaining = remaining.replace(before,"")
        }
        remaining.length>0 && arr.push(remaining)
        arr[0] = arr[0][0].toUpperCase().concat(arr[0].substring(1))
        arr = arr.map(word=>translations?.[word]||word)
        return arr.join(" ")
    }

}

module.exports = Setting