#ifndef DATAMGT_H
#define DATAMGT_H

#include "sbuffer.h" /* Required for sbuffer_t */
#include "common.h"  /* Required for gateway_error_t, sensor_id_t */

/* --- Room Sensor Map Structures --- */

/* Structure for a single Room <-> Sensor mapping entry */
typedef struct {
    int room_id;
    sensor_id_t sensor_id;
} room_sensor_entry_t;

/* Structure to hold the entire map (dynamic array) */
typedef struct {
    room_sensor_entry_t *entries; /* Pointer to the array of entries */
    int count;                    /* Number of valid entries */
    int capacity;                 /* Allocated capacity */
} room_sensor_map_t;

/* --- Thread Arguments --- */

/* Structure to pass arguments to the data manager thread */
typedef struct {
    sbuffer_t *buffer;           /* Pointer to the shared buffer */
    room_sensor_map_t *map;      /* Pointer to the loaded room-sensor map */
} datamgt_args_t;

/* --- Thread Function --- */

/**
 * The main function for the Data Manager thread.
 * Reads sensor data from the shared buffer, performs calculations
 * (e.g., running average), checks for conditions (e.g., temperature thresholds),
 * potentially using the room_sensor_map, and logs relevant events.
 * @param arg A pointer to a datamgt_args_t struct containing thread arguments.
 * @return Always returns NULL. Errors should be handled internally or logged.
 */
void *datamgt_run(void *arg);

/* --- Shutdown Function --- */
/**
 * @brief Signals the Data Manager thread to stop gracefully.
 */
 void datamgt_stop(void);

/* --- Map Loading/Freeing Functions --- */

/**
* @brief Loads the room-to-sensor mapping from a file.
* Assumes CSV format: room_id,sensor_id per line. Ignores empty lines and lines starting with #.
* Allocates memory for the map structure and its entries.
* @param filename The path to the map file.
* @param map A pointer to a room_sensor_map_t* variable where the pointer to the loaded map will be stored.
* @return GATEWAY_SUCCESS on success, an error code otherwise (e.g., GATEWAY_ERROR_NOMEM, file errors).
*/
gateway_error_t datamgt_load_room_sensor_map(const char* filename, room_sensor_map_t** map);

/**
* @brief Frees the memory allocated for the room-sensor map.
* @param map A pointer to the room_sensor_map_t* variable holding the map pointer.
* Sets the map pointer to NULL after freeing.
*/
void datamgt_free_room_sensor_map(room_sensor_map_t** map);


#endif /* DATAMGT_H */