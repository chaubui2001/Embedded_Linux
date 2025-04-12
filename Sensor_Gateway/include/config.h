#ifndef CONFIG_H
#define CONFIG_H

/* -- Network Configuration -- */

/* Maximum number of pending connections in the listen queue */
#define TCP_BACKLOG 10          

/* Timeout duration in seconds for inactive sensors */
#define SENSOR_TIMEOUT_SEC 5   

/* -- Shared Buffer Configuration -- */

/* Size of the shared buffer (number of sensor_data_t elements) */
#define SBUFFER_SIZE 15         

/* -- Database Configuration -- */

/* Filename for the SQLite database */
#define DB_NAME "sensordata.db" 
/* Name of the table within the database */
#define DB_TABLE_NAME "SensorData" 
/* Number of retry attempts if DB connection fails */
#define DB_CONNECT_RETRY_ATTEMPTS 3 
/* Delay in seconds between DB connection retry attempts */
#define DB_CONNECT_RETRY_DELAY_SEC 5 

/* -- Logging Configuration -- */

/* Name of the FIFO used for logging events */
#define LOG_FIFO_NAME "logFifo" 
/* Name of the output log file */
#define LOG_FILE_NAME "gateway.log" 

/* -- Data Manager Configuration -- */
#define MAP_FILE_NAME "room_sensor.map"

/* -- Command Interface Configuration -- */
#define CMD_SOCKET_PATH "/tmp/sensor_gateway_cmd.sock"

/* Max connections from the same IP */
#define MAX_CONNECTIONS_PER_IP 5 

/* Temperature thresholds for 'too hot'/'too cold' alerts */
#define TEMP_TOO_HOT_THRESHOLD 30.0
#define TEMP_TOO_COLD_THRESHOLD 15.0

#endif /* CONFIG_H */