const DB = require("better-sqlite3");
const fs = require('fs');

const ddl = require('./ddl.js');

var db;
function initializeDB(filename){
        db = new DB(filename , DB.OPEN_READWRITE, (err) => {
                if (err) {
                        console.error(err.message);
                }
                else
        });
        console.log("Initializing Database");                  
        db.exec(ddl.schema);
}

initializeDB("test.db");