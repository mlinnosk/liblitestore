CREATE TABLE meta(
       schema_version INTEGER NOT NULL DEFAULT 1
);
CREATE TABLE objects(
       id INTEGER PRIMARY KEY NOT NULL,
       name TEXT NOT NULL UNIQUE,
       type INTEGER NOT NULL
);
CREATE TABLE raw_data(
       id INTEGER NOT NULL,
       raw_value BLOB NOT NULL,
       FOREIGN KEY(id) REFERENCES objects(id)
       ON DELETE CASCADE ON UPDATE RESTRICT
);
CREATE TABLE object_data(
       id INTEGER NOT NULL,
       json_key BLOB NOT NULL,
       json_value BLOB NOT NULL,
       FOREIGN KEY(id) REFERENCES objects(id)
       ON DELETE CASCADE ON UPDATE RESTRICT
);
CREATE TABLE array_data(
       id INTEGER NOT NULL,
       array_index INTEGER NOT NULL,
       json_value BLOB NOT NULL,
       FOREIGN KEY(id) REFERENCES objects(id)
       ON DELETE CASCADE ON UPDATE RESTRICT
);