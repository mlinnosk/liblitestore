/**
 * Copyright (c) 2014 Markku Linnoskivi
 *
 * See the file LICENSE.txt for copying permission.
 */
CREATE TABLE IF NOT EXISTS meta(
       schema_version INTEGER NOT NULL DEFAULT 1
);
CREATE TABLE IF NOT EXISTS objects(
       id INTEGER PRIMARY KEY NOT NULL,
       name TEXT NOT NULL UNIQUE,
       type INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS raw_data(
       id INTEGER NOT NULL,
       raw_value BLOB NOT NULL,
       FOREIGN KEY(id) REFERENCES objects(id)
       ON DELETE CASCADE ON UPDATE RESTRICT
);
CREATE TABLE IF NOT EXISTS kv_data(
       id INTEGER NOT NULL,
       kv_key BLOB NOT NULL,
       kv_value BLOB NOT NULL,
       FOREIGN KEY(id) REFERENCES objects(id)
       ON DELETE CASCADE ON UPDATE RESTRICT
);
CREATE TABLE IF NOT EXISTS array_data(
       id INTEGER NOT NULL,
       array_index INTEGER NOT NULL,
       array_value BLOB NOT NULL,
       FOREIGN KEY(id) REFERENCES objects(id)
       ON DELETE CASCADE ON UPDATE RESTRICT
);