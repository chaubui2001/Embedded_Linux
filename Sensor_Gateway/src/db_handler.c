#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h> /* SQLite library header */
#include <string.h>  /* For strerror */

/* Include project-specific headers */
#include "config.h"     /* For DB_NAME, DB_TABLE_NAME */
#include "common.h"     /* For sensor_data_t, gateway_error_t */
#include "logger.h"     /* For log_message */
#include "db_handler.h" /* For function declarations */

/* Local use macros */
#define SQL_BUFFER_SIZE_SML 256 /* Small buffer for SQL statements */
#define SQL_BUFFER_SIZE_LRG 512 /* Large buffer for SQL statements */

/* --- Implementation of Database Handler Functions --- */

/**
 * @brief Connects to the SQLite database.
 * Creates the database file and the required table if they don't exist.
 * @param db_name The filename of the database.
 * @param db A pointer to a sqlite3* variable where the database handle will be stored.
 * @return GATEWAY_SUCCESS on success, an error code otherwise.
 */
gateway_error_t db_connect(const char *db_name, sqlite3 **db) {
    char *err_msg = NULL;
    int rc;
    char sql_create_table[SQL_BUFFER_SIZE_SML];

    /* Attempt to open the database file */
    /* sqlite3_open will create the file if it doesn't exist */
    rc = sqlite3_open(db_name, db);
    if (rc != SQLITE_OK) {
        log_message(LOG_LEVEL_ERROR, "Cannot open database %s: %s",
                    db_name, sqlite3_errmsg(*db));
        sqlite3_close(*db); /* Close handle even on error */
        *db = NULL; /* Ensure db pointer is NULL on failure */
        return DB_CONNECT_ERROR;
    }

    /* Log successful connection */
    log_message(LOG_LEVEL_INFO, "Connection to SQL server %s established.", db_name);

    /* Prepare SQL statement to create the table if it doesn't exist */
    snprintf(sql_create_table, sizeof(sql_create_table),
            "CREATE TABLE IF NOT EXISTS %s ("
            "RecordID INTEGER PRIMARY KEY AUTOINCREMENT, " /* Auto-incrementing primary key */
            "SensorID INTEGER NOT NULL, "                /* Sensor ID */
            "Timestamp INTEGER NOT NULL, "               /* Unix timestamp (seconds) */
            "Value REAL NOT NULL"                        /* Temperature value */
            ");",
            DB_TABLE_NAME);

    /* Execute the CREATE TABLE statement */
    rc = sqlite3_exec(*db, sql_create_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
         
        log_message(LOG_LEVEL_ERROR, "Failed to create table %s: %s",
                    DB_TABLE_NAME, err_msg);
        sqlite3_free(err_msg); /* Free error message */
        sqlite3_close(*db);
        *db = NULL;
        return DB_TABLE_CREATE_ERROR;
    } else {
        /* Log table creation/existence check */
        
        log_message(LOG_LEVEL_INFO, "Table %s checked/created successfully.", DB_TABLE_NAME);
    }

    return GATEWAY_SUCCESS;
}

/**
 * @brief Disconnects from the SQLite database.
 * @param db The sqlite3 database handle to disconnect.
 * @return GATEWAY_SUCCESS on success, DB_DISCONNECT_ERROR otherwise.
 */
gateway_error_t db_disconnect(sqlite3 *db) {
    int rc;

    if (db == NULL) {
        return GATEWAY_SUCCESS; /* Nothing to disconnect */
    }

    rc = sqlite3_close(db);
    if (rc != SQLITE_OK) {
        /* Error likely means there are unfinalized statements or active transactions */
        
        log_message(LOG_LEVEL_ERROR, "Failed to close database: %s", sqlite3_errmsg(db));
        /* Depending on the error, the handle might still be partially valid or invalid */
        /* Returning error signifies potential resource leak or issue */
        return DB_DISCONNECT_ERROR; 
    }

    /* Log successful disconnection */
    log_message(LOG_LEVEL_INFO, "Disconnected from SQL server."); // Use new log_message

    return GATEWAY_SUCCESS;
}

/**
 * @brief Inserts sensor data into the specified table in the database.
 * Uses prepared statements for safety and efficiency.
 * @param db The sqlite3 database handle.
 * @param data A pointer to the sensor_data_t struct containing the data to insert.
 * @return GATEWAY_SUCCESS on success, DB_INSERT_ERROR otherwise.
 */
gateway_error_t db_insert_sensor_data(sqlite3 *db, const sensor_data_t *data) {
    sqlite3_stmt *stmt = NULL;
    char sql_insert[SQL_BUFFER_SIZE_SML];
    int rc;
    gateway_error_t result = GATEWAY_SUCCESS;

    if (db == NULL || data == NULL) {
        return GATEWAY_ERROR_INVALID_ARG;
    }

    /* Prepare the SQL INSERT statement */
    snprintf(sql_insert, sizeof(sql_insert),
             "INSERT INTO %s (SensorID, Timestamp, Value) VALUES (?, ?, ?);",
             DB_TABLE_NAME);

    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
         
        log_message(LOG_LEVEL_ERROR, "Failed to prepare insert statement: %s", sqlite3_errmsg(db));
        result = DB_INSERT_ERROR;
        goto cleanup; /* Use goto for single cleanup point */
    }

    /* Bind values to the prepared statement parameters */
    /* Index starts from 1 */
    rc = sqlite3_bind_int(stmt, 1, data->id); /* Bind SensorID as INT */
    if (rc != SQLITE_OK) {
         
        log_message(LOG_LEVEL_ERROR, "Failed to bind SensorID (%d): %s", data->id, sqlite3_errmsg(db));
        result = DB_INSERT_ERROR;
        goto cleanup;
    }

    rc = sqlite3_bind_int64(stmt, 2, data->ts); /* Bind Timestamp as INT64 */
    if (rc != SQLITE_OK) {
         
        log_message(LOG_LEVEL_ERROR, "Failed to bind Timestamp (%ld) for sensor %d: %s", data->ts, data->id, sqlite3_errmsg(db));
        result = DB_INSERT_ERROR;
        goto cleanup;
    }

    rc = sqlite3_bind_double(stmt, 3, data->value); /* Bind Value as DOUBLE */
    if (rc != SQLITE_OK) {
         
        log_message(LOG_LEVEL_ERROR, "Failed to bind Value (%.2f) for sensor %d: %s", data->value, data->id, sqlite3_errmsg(db));
        result = DB_INSERT_ERROR;
        goto cleanup;
    }

    /* Execute the prepared statement */
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
         
        log_message(LOG_LEVEL_ERROR, "Failed to execute insert statement for sensor %d: %s", data->id, sqlite3_errmsg(db));
        result = DB_INSERT_ERROR;
        goto cleanup;
    }

    /* Log successful insertion */
     
    log_message(LOG_LEVEL_DEBUG, "Inserted SensorID %d, TS %ld, Value %.2f into DB",
                data->id, data->ts, data->value);

cleanup:
    /* Finalize the statement to release resources */
    if (stmt != NULL) {
        sqlite3_finalize(stmt);
    }

    return result;
}