# Sensor Gateway

**Author:** Chau Minh Bui

## Introduction

**Sensor Gateway** is a server-side application written in C designed to collect data from multiple sensor nodes via TCP connections, process this data, and store it in an SQLite database. The system is built to handle concurrent connections from sensors, manage connection states, log activities, and provide a Command Line Interface (CLI) for interaction and monitoring.

This project was developed as a programming assignment, focusing on concepts like network programming, concurrency, memory management, and database interaction within a C environment.

## Key Features

* **Connection Management:**
    * Accepts and handles multiple concurrent TCP connections from sensor nodes.
    * Uses efficient I/O mechanisms (like `select`, `poll`, or `epoll`) or multi-threading to manage connections.
    * Tracks the status of each connection (active, inactive, timeout).
    * Automatically disconnects sensors that are inactive for a specified timeout period.
* **Data Management:**
    * Receives data (sensor ID, temperature value, timestamp) from sensor nodes.
    * Utilizes a thread-safe shared buffer to pass data between processing threads (e.g., connection handling thread and database writing thread).
    * Parses and processes sensor data.
* **Storage Management:**
    * Interacts with an SQLite database to store processed sensor data.
    * Creates the necessary database table(s) if they don't exist.
    * Performs data insertion operations into the database.
* **Logging:**
    * Logs important system events (new connections, disconnections, errors, data received/written) to a log file (`gateway.log`).
    * Uses a separate process or a queue mechanism to handle logging without impacting the main gateway performance.
* **Command Interface:**
    * Provides an interface via a FIFO (Named Pipe) allowing external clients (`cmd_client`) to send commands to the gateway (e.g., request server shutdown).
* **System Monitoring (Optional/Potential):**
    * Monitors and reports system resource usage (CPU, RAM) - based on the presence of `sysmon.c`.

## Project Structure
```
Sensor_Gateway/
├── include/        # Contains header files (.h) defining interfaces and data structures
│   ├── common.h
│   ├── config.h      # General configurations (port, timeout, DB path, log path...)
│   ├── conmgt.h      # Connection management header
│   ├── datamgt.h     # Data management header
│   ├── db_handler.h  # Database handler header
│   ├── logger.h      # Logger header
│   ├── sbuffer.h     # Shared buffer header (for inter-thread/process communication)
│   ├── storagemgt.h  # Storage management header
│   ├── cmdif.h       # Command interface header
│   └── sysmon.h      # System monitoring header
├── src/            # Contains C source files (.c) implementing the functionality
│   ├── main.c        # Main entry point for the gateway program
│   ├── conmgt.c      # Connection management implementation
│   ├── datamgt.c     # Data management implementation
│   ├── db_handler.c  # Database handler implementation (SQLite)
│   ├── logger.c      # Logger implementation
│   ├── log_process.c # Possibly used for log processing (e.g., sending logs via pipe)
│   ├── sbuffer.c     # Shared buffer implementation
│   ├── storagemgt.c  # Storage management implementation
│   ├── cmdif.c       # Command interface implementation
│   └── sysmon.c      # System monitoring implementation
├── test/           # Contains code for testing and simulation
│   ├── sensor_sim.c          # Sensor node simulator program
│   ├── cmd_client.c          # Client to send commands to the gateway's command interface
├── gateway.log     # Default log file for the gateway
└── Makefile        # (Assumed) File used to build the project
```
## Dependencies

* **GCC Compiler:** Required to compile the C source code.
* **Make:** Required to use the `Makefile` for building the project.
* **SQLite3:** Library and command-line tools for SQLite (requires `libsqlite3-dev` or equivalent for compilation).
* **pthread:** POSIX Threads library (usually available on Linux/macOS), needed if the project uses multi-threading. Linked with the `-pthread` flag.

## Compilation

This project uses a `Makefile` to simplify the compilation process. Open your terminal in the `Sensor_Gateway` root directory and use the following commands:

1.  **Compile the Sensor Gateway:**
    ```bash
    make
    ```
    This command will compile source files and place the executables `sensor_gateway` in the `./build/out/` directory.

2.  **Compilet Sensor Simulator:**
    ```bash
    make test
    ```

3.  **Compile the Command Client:**
    ```bash
    make client
    ```

4.  **Clean up build files:**
    ```bash
    make clean
    ```
    This removes the `build` directory and any compiled files.

## Usage

Follow these steps to run the system:

1.  **Compile the project:**
    Make sure you have compiled the project using `make`.

2.  **Run the Sensor Gateway:**
    Open a terminal and execute the gateway:
    ```bash
    ./build/out/sensor_gateway <port>
    ```
    * **`<port>`:** The network port number the gateway should listen on for incoming sensor connections.
        * *Example:* `1234`

    *Example Command:*
    ```bash
    ./build/out/sensor_gateway 1234
    ```
    The gateway will start, create the log file (`gateway.log`) and database file (`sensor_data.db` - name might vary) if they don't exist, and create a FIFO (e.g., `gateway_fifo`) for commands. It will then wait for sensor connections on the specified port.

3.  **Run the Sensor Simulator(s):**
    Open one or more *separate* terminals for each sensor you want to simulate. Execute the sensor simulator program:
    ```bash
    ./build/out/sensor_sim <gateway_ip> <gateway_port> <sensor_id> <send_interval_ms>
    ```
    * **`<gateway_ip>`:** The IP address of the machine running the Sensor Gateway. Use `127.0.0.1` if running on the same machine.
    * **`<gateway_port>`:** The port number the Sensor Gateway is listening on (must match the port used in step 2).
    * **`<sensor_id>`:** A unique ID number for this simulated sensor.
    * **`<send_interval_ms>`:** The time interval (in milliseconds) between sending data readings from this sensor.

    *Example Commands (run each in a separate terminal):*
    ```bash
    ./build/out/sensor_sim 127.0.0.1 1234 101 2000
    ./build/out/sensor_sim 127.0.0.1 1234 102 2500
    ./build/out/sensor_sim 127.0.0.1 1234 103 3000
    ```
    Each simulator instance will connect to the gateway and start sending simulated temperature data with its assigned ID at the specified interval.

4.  **Use the Command Client (Optional):**
    Open another terminal to send commands to the running gateway via the FIFO:
    ```bash
    ./build/out/cmd_client <command>
    ```
    * **`<command>`:** The command to send.

    *Example Command:*
    ```bash
    ./build/out/cmd_client status
    ```
    ```bash
    ./build/out/cmd_client stats
    ```

## Testing

Testing involves running the `sensor_gateway` and one or more instances of `sensor_sim` concurrently. You might also use the `make test` target if it includes automated tests.

1.  **Start Gateway:** Run `./build/out/sensor_gateway <port>`. Monitor its terminal output and the contents of `gateway.log` for startup messages and status updates.
2.  **Start Sensor Simulator(s):** Run one or more `./build/out/sensor_sim` instances with appropriate parameters (as shown in the Usage section). Check the gateway's log for messages about new sensor connections.
3.  **Check Data:**
    * Observe the gateway's log for messages indicating data reception and processing.
    * Use the `sqlite3` command-line tool to inspect the contents of the database file (`sensor_data.db`):
        ```bash
        sqlite3 sensor_data.db
        sqlite> SELECT * FROM SensorData;
        sqlite> .quit
        ```
        Verify that data from the sensors is being correctly inserted into the database.
4.  **Test Timeout:** Run `sensor_sim` with some sensors, then stop the `sensor_sim` processes (e.g., using Ctrl+C in their terminals). Observe the gateway's log to see if the inactive connections are disconnected after the configured timeout period.
5.  **Test Command Interface:** Run `./build/out/cmd_client status` (or the relevant command) to check if the gateway report status.
6.  **Test Error Handling:** Abruptly terminate a `sensor_sim` process while it's connected and observe how the gateway handles the disconnection error in its log.

## Notes

* Ensure you have the necessary permissions to create files (log, database, FIFO) and open network ports.
* The names of the log file, database file, and FIFO pipe might be configurable (check `config.h` or source files).
* The `Makefile` should handle linking necessary libraries like `libsqlite3` and `pthread`.

