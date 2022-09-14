

const schema = 
`  
CREATE TABLE IF NOT EXISTS sensor
(
		id INTEGER PRIMARY KEY,
        	uid TEXT NOT NULL,
		name TEXT, 
		type TEXT NOT NULL,
		first_seen TEXT NOT NULL,
		last_seen TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS log
(
		id INTEGER PRIMARY KEY,
        	sensor_id TEXT NOT NULL,
		rss INTEGER NOT NULL,
		bat REAL NOT NULL, 
		data TEXT NOT NULL,
		date_created TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS settings(
    id INTEGER PRIMARY KEY,
    name VARCHAR(56),
    value VARCHAR(56)
);


`
;

module.exports = {
        
        schema : schema
}

