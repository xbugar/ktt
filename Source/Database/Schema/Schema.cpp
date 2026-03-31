#include <stdexcept>
#include <string>

#include <Database/Schema/Schema.h>

namespace ktt {

void Schema::CreateIfNotExists(sqlite3 *connection) {
        const auto createSchemaSQL = R"(
CREATE TABLE IF NOT EXISTS tuning_source
(
    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
    parameter_fingerprint INTEGER NOT NULL,
    source_fingerprint    INTEGER NOT NULL,
    created_at            TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_tuning_source_fingerprints ON tuning_source (parameter_fingerprint, source_fingerprint);

CREATE TABLE IF NOT EXISTS tuning_space
(
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id         INTEGER NOT NULL REFERENCES tuning_source (id),
    space_fingerprint INTEGER NOT NULL,
    created_at        TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_tuning_space_fingerprint ON tuning_space (space_fingerprint, source_id);

CREATE TABLE IF NOT EXISTS tuning_run
(
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    uuid       TEXT NOT NULL DEFAULT (lower(hex(randomblob(16)))),
    created_at TIMESTAMP     DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS tuning_result
(
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id    TEXT    NOT NULL REFERENCES tuning_run (id),
    space_id  INTEGER NOT NULL REFERENCES tuning_space (id),
    duration  INTEGER NOT NULL,
    result    TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_tuning_result_space_id ON tuning_result (space_id);
    )";

        char *errorMsg = nullptr;

        if (const int result = sqlite3_exec(connection, createSchemaSQL, nullptr, nullptr, &errorMsg);
            result != SQLITE_OK) {
            std::string error = errorMsg ? errorMsg : "Unknown error";
            sqlite3_free(errorMsg);
            throw std::runtime_error("Failed to create table: " + error);
        }
    }
} // namespace ktt
