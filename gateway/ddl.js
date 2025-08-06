

// DROP TABLE settings;

const schema = 
`  

CREATE TABLE IF NOT EXISTS sensor
(
		id INTEGER PRIMARY KEY,
        	uid TEXT NOT NULL UNIQUE,
		name TEXT, 
		type TEXT NOT NULL,
		first_seen INTEGER NOT NULL,
		last_seen INTEGER NOT NULL,
		created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
		updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

CREATE TABLE IF NOT EXISTS raw_log
(
		id INTEGER PRIMARY KEY,
		uid TEXT NOT NULL,
		source TEXT,
		rss INTEGER,
		bat REAL,
		sensor_type TEXT,
		data TEXT,
		path TEXT,
		path_indicator TEXT,
		created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS settings(
    id INTEGER PRIMARY KEY,
    name VARCHAR(56) UNIQUE NOT NULL,
    value VARCHAR(256),
    created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
);

INSERT OR IGNORE INTO settings (name, value) VALUES ('schema_version', '2.1');


`
;

module.exports = {
        
        schema : schema
}

