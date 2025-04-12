#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     /* For sleep() */
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>      /* For isspace() */
#include <signal.h>     /* For sig_atomic_t */
#include <errno.h>      /* For errno */

/* Include project-specific headers */
#include "config.h"
#include "common.h"
#include "sbuffer.h"
#include "logger.h"
#include "datamgt.h"

/* --- Local Macros --- */

/* Define temperature thresholds if not already in config.h */
#ifndef TEMP_TOO_HOT_THRESHOLD
#define TEMP_TOO_HOT_THRESHOLD 28.0 /* Example threshold */
#endif
#ifndef TEMP_TOO_COLD_THRESHOLD
#define TEMP_TOO_COLD_THRESHOLD 15.0 /* Example threshold */
#endif

#define INITIAL_SENSOR_LIST_CAPACITY 10 /* Initial capacity for sensor statistics list */
#define INVALID_SENSOR_ID 0             /* Define an invalid sensor ID for checks */
#define BUSY_WAIT_SLEEP_SEC 1           /* Sleep time for loop on unexpected error */
#define MAP_INITIAL_CAPACITY 10         /* Initial capacity for room-sensor map */
#define MAP_LINE_BUFFER_SIZE 100        /* Buffer size for reading map file lines */

/* --- Local Structures --- */

/* Enum for sensor temperature state */
typedef enum {
    TEMP_STATE_NORMAL,
    TEMP_STATE_TOO_COLD,
    TEMP_STATE_TOO_HOT
} temp_state_t;

/* Structure to store statistics for each sensor */
typedef struct {
    sensor_id_t id;
    double total_value_sum;
    uint64_t reading_count;
    temp_state_t last_logged_state;
} sensor_stats_t;

/* Dynamic array structure to hold sensor statistics */
typedef struct {
    sensor_stats_t *data;
    int size;
    int capacity;
} sensor_stats_list_t;

/* --- Static Variables --- */

/* Global list to store sensor stats (access is single-threaded within this module) */
static sensor_stats_list_t sensor_list = {NULL, 0, 0};

/* --- External Variables --- */

/* Global flag from main.c to signal termination */
extern volatile sig_atomic_t terminate_flag;

/* --- Forward Declarations (Internal Helper Functions) --- */

static gateway_error_t init_sensor_stats_list(int initial_capacity);
static void free_sensor_stats_list(void);
static sensor_stats_t* find_or_create_sensor(sensor_id_t id);
static void update_sensor_stats(sensor_stats_t *stats, double value);
static void check_temperature_alerts(sensor_stats_t *stats, const room_sensor_map_t *map); // Added map parameter
static int get_room_id(sensor_id_t sensor_id, const room_sensor_map_t *map); // Helper to lookup room

/* --- Main Thread Function Implementation --- */

void *datamgt_run(void *arg) {
    datamgt_args_t *args = (datamgt_args_t *)arg;
    sbuffer_t *buffer = args->buffer;
    room_sensor_map_t *map = args->map; // Get map pointer
    sensor_data_t data;
    gateway_error_t sbuf_ret;

    if (init_sensor_stats_list(INITIAL_SENSOR_LIST_CAPACITY) != GATEWAY_SUCCESS) {
        log_message(LOG_LEVEL_FATAL, "Data manager failed to initialize sensor list. Exiting thread."); 
        return NULL;
    }
    log_message(LOG_LEVEL_INFO, "Data manager thread started."); 

    while (1) {
        /* Check termination flag at the start of the loop */
        if (terminate_flag) {
            log_message(LOG_LEVEL_INFO, "Data manager received termination signal flag."); 
            break;
        }

        /* 1. Read data from the shared buffer (blocking call) */
        sbuf_ret = sbuffer_remove(buffer, &data);

        if (sbuf_ret == SBUFFER_SHUTDOWN) {
            log_message(LOG_LEVEL_INFO, "Data manager received shutdown signal from sbuffer. Exiting loop."); 
            break;
        }
        else if (sbuf_ret != GATEWAY_SUCCESS) {
            if (sbuf_ret == SBUFFER_EMPTY) { /* Assuming buffer might return this on free/shutdown */
                log_message(LOG_LEVEL_INFO, "Data manager buffer remove returned empty/error, likely shutting down."); 
                break; /* Exit loop */
            } else {
                 log_message(LOG_LEVEL_ERROR, "Data manager failed to remove data from buffer (Error %d)", sbuf_ret); 
                sleep(BUSY_WAIT_SLEEP_SEC); /* Avoid busy loop on unexpected error */
                continue;
            }
        }

        /* Data Validation: Check for invalid sensor ID */
        if (data.id == INVALID_SENSOR_ID) {
             log_message(LOG_LEVEL_WARNING, "Received sensor data with invalid sensor node ID %d", data.id); 
            continue;
        }

        /* 2. Find or create statistics entry for this sensor ID */
        sensor_stats_t *stats = find_or_create_sensor(data.id);
        if (stats == NULL) {
             log_message(LOG_LEVEL_ERROR, "Failed to find or create stats for sensor ID %d (Memory issue?)", data.id); 
            continue;
        }

        /* 3. Update statistics */
        update_sensor_stats(stats, data.value);

        /* 4. Calculate running average and check thresholds/log alerts (pass map) */
        check_temperature_alerts(stats, map);

        /* DEBUG Log */
         log_message(LOG_LEVEL_DEBUG, "Processed Sensor ID: %d, Value: %.2f, Count: %lu, Avg: %.2f", 
                     stats->id, data.value, stats->reading_count,
                     stats->reading_count > 0 ? (stats->total_value_sum / stats->reading_count) : 0.0); // Avoid division by zero

    } /* End of main while loop */

    log_message(LOG_LEVEL_INFO, "Data manager thread shutting down..."); 
    free_sensor_stats_list(); /* Clean up sensor stats list */
    log_message(LOG_LEVEL_INFO, "Data manager finished cleanup."); 

    return NULL;
}

/* --- Shutdown Function Implementation --- */

/**
 * @brief Signals the Data Manager thread to stop gracefully.
 * (Currently relies on terminate_flag being checked in the run loop).
 */
void datamgt_stop(void) {
    // The actual stop relies on terminate_flag set by the signal handler
    // and checked within the datamgt_run loop.
    log_message(LOG_LEVEL_INFO, "Data Manager stop requested (flag will be checked in loop)."); 
}


/* --- Implementation of Internal Helper Functions --- */

/**
 * @brief Initializes the dynamic array for sensor statistics.
 */
static gateway_error_t init_sensor_stats_list(int initial_capacity) {
    if (initial_capacity <= 0) initial_capacity = INITIAL_SENSOR_LIST_CAPACITY;
    sensor_list.data = malloc(initial_capacity * sizeof(sensor_stats_t));
    if (sensor_list.data == NULL) {
         log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for initial sensor stats list: %s", strerror(errno)); 
        return GATEWAY_ERROR_NOMEM;
    }
    sensor_list.size = 0;
    sensor_list.capacity = initial_capacity;
    log_message(LOG_LEVEL_DEBUG, "Initialized sensor stats list with capacity %d", initial_capacity); 
    return GATEWAY_SUCCESS;
}

/**
 * @brief Frees the memory allocated for the sensor statistics list.
 */
static void free_sensor_stats_list(void) {
    if (sensor_list.data != NULL) {
        free(sensor_list.data);
        sensor_list.data = NULL;
        sensor_list.size = 0;
        sensor_list.capacity = 0;
        log_message(LOG_LEVEL_DEBUG, "Freed sensor stats list memory."); 
    }
}

/**
 * @brief Finds an existing sensor_stats entry by ID or creates a new one if not found.
 */
static sensor_stats_t* find_or_create_sensor(sensor_id_t id) {
    /* Search for existing sensor */
    for (int i = 0; i < sensor_list.size; ++i) {
        if (sensor_list.data[i].id == id) {
            return &sensor_list.data[i];
        }
    }

    /* If not found, check if resizing is needed */
    if (sensor_list.size >= sensor_list.capacity) {
        int new_capacity = sensor_list.capacity == 0 ? INITIAL_SENSOR_LIST_CAPACITY : sensor_list.capacity * 2;
        log_message(LOG_LEVEL_DEBUG, "Resizing sensor stats list from %d to %d", sensor_list.capacity, new_capacity); 
        sensor_stats_t *new_data = realloc(sensor_list.data, new_capacity * sizeof(sensor_stats_t));
        if (new_data == NULL) {
            log_message(LOG_LEVEL_ERROR, "Failed to reallocate memory for sensor stats: %s", strerror(errno)); 
            return NULL;
        }
        sensor_list.data = new_data;
        sensor_list.capacity = new_capacity;
    }

    /* Create new entry */
    log_message(LOG_LEVEL_DEBUG, "Creating new stats entry for sensor ID %d at index %d", id, sensor_list.size); 
    sensor_stats_t *new_sensor = &sensor_list.data[sensor_list.size];
    new_sensor->id = id;
    new_sensor->total_value_sum = 0.0;
    new_sensor->reading_count = 0;
    new_sensor->last_logged_state = TEMP_STATE_NORMAL; // Initial state

    sensor_list.size++;
    return new_sensor;
}

/**
 * @brief Updates the statistics for a given sensor with a new reading.
 */
static void update_sensor_stats(sensor_stats_t *stats, double value) {
    stats->total_value_sum += value;
    stats->reading_count++;
    // No logging here, happens in check_temperature_alerts or main loop DEBUG log
}

/**
 * @brief Calculates running average, checks thresholds, and logs alerts if state changes.
 */
static void check_temperature_alerts(sensor_stats_t *stats, const room_sensor_map_t *map) {

    if (stats->reading_count == 0) {
        return; /* Cannot calculate average yet */
    }

    double running_avg = stats->total_value_sum / stats->reading_count;
    temp_state_t current_state = TEMP_STATE_NORMAL;
    int room_id = -1; // Default room ID if not found

    /* Find room ID from map if map exists */
    if (map != NULL) {
        room_id = get_room_id(stats->id, map);
    }


    if (running_avg < TEMP_TOO_COLD_THRESHOLD) {
        current_state = TEMP_STATE_TOO_COLD;
    } else if (running_avg > TEMP_TOO_HOT_THRESHOLD) {
        current_state = TEMP_STATE_TOO_HOT;
    }

    /* Log only if the state has changed */
    if (current_state != stats->last_logged_state) {
        const char* room_info_str = (room_id != -1) ? "in room" : "for sensor";
        int id_to_log = (room_id != -1) ? room_id : stats->id;

        switch (current_state) {
            case TEMP_STATE_TOO_COLD:
                log_message(LOG_LEVEL_WARNING, // Changed from ALERT to WARNING, ALERT is not a defined level
                            "Sensor node %d (%s %d) reports it's too cold (running avg temperature = %.2f)",
                            stats->id, room_info_str, id_to_log, running_avg); 
                break;
            case TEMP_STATE_TOO_HOT:
                log_message(LOG_LEVEL_WARNING, // Changed from ALERT to WARNING
                            "Sensor node %d (%s %d) reports it's too hot (running avg temperature = %.2f)",
                            stats->id, room_info_str, id_to_log, running_avg); 
                break;
            case TEMP_STATE_NORMAL:
                 /* Log when temperature returns to normal */
                log_message(LOG_LEVEL_INFO,
                            "Sensor node %d (%s %d) temperature has returned to normal (running avg temperature = %.2f)",
                            stats->id, room_info_str, id_to_log, running_avg); 
                break;
        }
        stats->last_logged_state = current_state; /* Update the last logged state */
    }
}


/* --- Implementation of Room-Sensor Map Functions --- */

/**
 * @brief Helper function to find room ID for a given sensor ID.
 */
static int get_room_id(sensor_id_t sensor_id, const room_sensor_map_t *map) {
    if (map == NULL || map->entries == NULL) {
        return -1;
    }
    for (int i = 0; i < map->count; ++i) {
        if (map->entries[i].sensor_id == sensor_id) {
            return map->entries[i].room_id;
        }
    }
    return -1; /* Not found */
}


/**
 * @brief Loads the room-to-sensor mapping from a file.
 */
 gateway_error_t datamgt_load_room_sensor_map(const char* filename, room_sensor_map_t** map) {
    if (filename == NULL || map == NULL) {
        return GATEWAY_ERROR_INVALID_ARG;
    }

    FILE *fp = NULL;
    char line_buffer[MAP_LINE_BUFFER_SIZE];
    room_sensor_map_t *loaded_map = NULL;
    gateway_error_t status = GATEWAY_SUCCESS;
    int line_num = 0;

    /* Allocate map structure */
    loaded_map = malloc(sizeof(room_sensor_map_t));
    if (loaded_map == NULL) {
        // **** Use fprintf instead of log_message ****
        fprintf(stderr, "ERROR: Failed to allocate memory for room sensor map struct: %s\n", strerror(errno));
        return GATEWAY_ERROR_NOMEM;
    }
    loaded_map->entries = NULL;
    loaded_map->count = 0;
    loaded_map->capacity = 0;

    /* Allocate initial entries */
    loaded_map->entries = malloc(MAP_INITIAL_CAPACITY * sizeof(room_sensor_entry_t));
    if (loaded_map->entries == NULL) {
        // **** Use fprintf instead of log_message ****
        fprintf(stderr, "ERROR: Failed to allocate memory for initial map entries: %s\n", strerror(errno));
        free(loaded_map);
        return GATEWAY_ERROR_NOMEM;
    }
    loaded_map->capacity = MAP_INITIAL_CAPACITY;

    /* Open file */
    fp = fopen(filename, "r");
    if (fp == NULL) {
        // **** Use fprintf instead of log_message ****
        fprintf(stderr, "ERROR: Cannot open room_sensor map file '%s': %s\n", filename, strerror(errno));
        free(loaded_map->entries);
        free(loaded_map);
        return GATEWAY_ERROR; // Or specific file error
    }

    /* Read file line by line */
    while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
        line_num++;
        char *line_ptr = line_buffer;
        int room_id_val;
        int sensor_id_val;

        while (isspace((unsigned char)*line_ptr)) line_ptr++;
        if (*line_ptr == '\0' || *line_ptr == '#') continue; // Skip empty lines and comments

        if (sscanf(line_ptr, "%d , %d", &room_id_val, &sensor_id_val) != 2) {
            // **** Use fprintf instead of log_message ****
            fprintf(stderr, "WARN: Invalid format in map file '%s' at line %d: %s", filename, line_num, line_ptr); // Keep newline from fgets
            continue;
        }
        if (sensor_id_val < 0 || sensor_id_val > UINT16_MAX) {
            // **** Use fprintf instead of log_message ****
            fprintf(stderr, "WARN: Invalid sensor_id %d in map file '%s' at line %d. Skipping.\n", sensor_id_val, filename, line_num);
            continue;
        }

        /* Resize if needed */
        if (loaded_map->count >= loaded_map->capacity) {
            int new_capacity = loaded_map->capacity * 2;
            // **** Use fprintf instead of log_message ****
            fprintf(stderr, "DEBUG: Resizing room sensor map from %d to %d\n", loaded_map->capacity, new_capacity);
            room_sensor_entry_t *new_entries = realloc(loaded_map->entries, new_capacity * sizeof(room_sensor_entry_t));
            if (new_entries == NULL) {
                // **** Use fprintf instead of log_message ****
                fprintf(stderr, "ERROR: Failed to reallocate memory for map entries: %s\n", strerror(errno));
                status = GATEWAY_ERROR_NOMEM;
                goto cleanup_map_load;
            }
            loaded_map->entries = new_entries;
            loaded_map->capacity = new_capacity;
        }

        /* Add entry */
        loaded_map->entries[loaded_map->count].room_id = room_id_val;
        loaded_map->entries[loaded_map->count].sensor_id = (sensor_id_t)sensor_id_val;
        loaded_map->count++;
    }

    if (ferror(fp)) {
        // **** Use fprintf instead of log_message ****
        fprintf(stderr, "ERROR: Error reading from map file '%s': %s\n", filename, strerror(errno));
        status = GATEWAY_ERROR; // Or specific file read error
    }

cleanup_map_load:
    if (fp != NULL) fclose(fp);

    if (status == GATEWAY_SUCCESS) {
        *map = loaded_map;
        // **** Use fprintf instead of log_message ****
        // Log success to stderr as well, as logger might not be ready when this is called in main
        fprintf(stderr, "INFO: Loaded %d entries from room sensor map '%s'.\n", loaded_map->count, filename);
    } else {
        if (loaded_map != NULL) {
            free(loaded_map->entries); /* entries might be NULL if initial malloc failed */
            free(loaded_map);
        }
        *map = NULL;
        // Error logged previously
    }
    return status;
}

/**
 * @brief Frees the memory allocated for the room-sensor map. Logs to stderr.
 */
void datamgt_free_room_sensor_map(room_sensor_map_t** map) {
    if (map == NULL || *map == NULL) return;
    if ((*map)->entries != NULL) free((*map)->entries);
    free(*map);
    *map = NULL;
    // **** Use fprintf instead of log_message ****
    fprintf(stderr, "INFO: Room sensor map freed.\n");
}