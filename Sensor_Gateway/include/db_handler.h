#ifndef DB_HANDLER_H
#define DB_HANDLER_H

#include <sqlite3.h> /* Required for sqlite3* */
#include "common.h"  /* Required for sensor_data_t and gateway_error_t */

/**
 * Connects to the SQLite database.
 * Creates the database file and the required table if they don't exist.
 * @param db_name The filename of the database.
 * @param db A pointer to a sqlite3* variable where the database handle will be stored.
 * @return GATEWAY_SUCCESS on success, an error code otherwise (e.g., DB_CONNECT_ERROR, DB_TABLE_CREATE_ERROR).
 */
gateway_error_t db_connect(const char *db_name, sqlite3 **db);

/**
 * Disconnects from the SQLite database.
 * @param db The sqlite3 database handle to disconnect.
 * @return GATEWAY_SUCCESS on success, DB_DISCONNECT_ERROR otherwise.
 */
gateway_error_t db_disconnect(sqlite3 *db);

/**
 * Inserts sensor data into the specified table in the database.
 * @param db The sqlite3 database handle.
 * @param data A pointer to the sensor_data_t struct containing the data to insert.
 * @return GATEWAY_SUCCESS on success, DB_INSERT_ERROR otherwise.
 */
gateway_error_t db_insert_sensor_data(sqlite3 *db, const sensor_data_t *data);

#endif /* DB_HANDLER_H */