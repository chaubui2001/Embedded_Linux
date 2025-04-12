#ifndef COMMON_H
#define COMMON_H

#include <time.h> 
#include <stdint.h> 
#include <stdbool.h>
/* -- Sensor Data Types -- */

typedef uint16_t sensor_id_t;    /* Type for sensor ID */
typedef double sensor_value_t;   /* Type for sensor value (temperature) */
typedef time_t sensor_ts_t;      /* Type for timestamp (seconds since epoch) */

/* Structure to hold a single sensor data reading */
typedef struct {
    sensor_id_t id;
    sensor_value_t value;
    sensor_ts_t ts;
} sensor_data_t;

/* -- General Error Codes -- */
/* Using negative values for errors, 0 for success */
typedef enum {
    GATEWAY_SUCCESS = 0,          /* Operation successful */

    /* Generic Errors */
    GATEWAY_ERROR = -1,           /* Generic unspecified error */
    GATEWAY_ERROR_TIMEOUT = -2,   /* Operation timed out */
    GATEWAY_ERROR_NOMEM = -3,     /* Memory allocation failed */
    GATEWAY_ERROR_INVALID_ARG = -4, /* Invalid argument provided to function */

    /* SBuffer Errors */
    SBUFFER_ERROR = -10,          /* Generic sbuffer error */
    SBUFFER_FULL = -11,           /* Buffer is full (might not be returned if blocking) */
    SBUFFER_EMPTY = -12,          /* Buffer is empty (might not be returned if blocking) */
    SBUFFER_INIT_ERR = -13,       /* Sbuffer initialization failed */
    SBUFFER_FREE_ERR = -14,       /* Sbuffer free failed */
    SBUFFER_SHUTDOWN = -15,       /* Sbuffer shutdown requested */

    /* DB Handler Errors */
    DB_HANDLER_ERROR = -20,       /* Generic database handler error */
    DB_CONNECT_ERROR = -21,       /* Failed to connect to database */
    DB_TABLE_CREATE_ERROR = -22,  /* Failed to create database table */
    DB_INSERT_ERROR = -23,        /* Failed to insert data into database */
    DB_DISCONNECT_ERROR = -24,    /* Failed to disconnect database */

    /* Connection Manager Errors */
    CONNMGR_ERROR = -30,          /* Generic connection manager error */
    CONNMGR_SOCKET_CREATE_ERR = -31,/* Failed to create server socket */
    CONNMGR_SOCKET_BIND_ERR = -32,  /* Failed to bind server socket */
    CONNMGR_SOCKET_LISTEN_ERR = -33,/* Failed to listen on server socket */
    CONNMGR_POLL_ERR = -34,       /* Error in select/poll/epoll */
    CONNMGR_ACCEPT_ERR = -35,     /* Failed to accept new connection */
    CONNMGR_CLIENT_READ_ERR = -36,/* Failed to read from client socket */
    CONNMGR_CLIENT_CLOSE_ERR = -37,/* Error closing client socket */

    /* Logger/FIFO Errors */
    LOGGER_ERROR = -40,           /* Generic logger error */
    LOGGER_FIFO_CREATE_ERR = -41, /* Failed to create FIFO */
    LOGGER_FIFO_OPEN_ERR = -42,   /* Failed to open FIFO */
    LOGGER_FIFO_WRITE_ERR = -43,  /* Failed to write to FIFO */
    LOGGER_FIFO_READ_ERR = -44,   /* Failed to read from FIFO */
    LOGGER_FILE_OPEN_ERR = -45,   /* Failed to open log file */
    LOGGER_FILE_WRITE_ERR = -46,  /* Failed to write to log file */
    
    /* Threading Errors */
    THREAD_ERROR = -50,           /* Generic thread error */
    THREAD_CREATE_ERR = -51,      /* Failed to create thread */
    THREAD_JOIN_ERR = -52,        /* Failed to join thread */
    THREAD_MUTEX_INIT_ERR = -53,  /* Failed to initialize mutex */
    THREAD_MUTEX_LOCK_ERR = -54,  /* Failed to lock mutex */
    THREAD_MUTEX_UNLOCK_ERR = -55,/* Failed to unlock mutex */
    THREAD_COND_INIT_ERR = -56,   /* Failed to initialize condition variable */
    THREAD_COND_WAIT_ERR = -57,   /* Error waiting on condition variable */
    THREAD_COND_SIGNAL_ERR = -58, /* Error signaling condition variable */

} gateway_error_t;


#endif /* COMMON_H */