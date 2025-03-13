# Remote Message Exchange Chat Application

This project is a simple chat application that allows multiple peers (connection points) to exchange messages over the network using TCP sockets. The application integrates both client and server functionalities into a single program, enabling peers to connect and communicate with each other.

## Features

- Command-line interface for user interaction  
- Display local IP address and listening port  
- Establish connections to other peers  
- List active connections  
- Send messages to connected peers  
- Terminate specific connections  
- Exit the application safely with connection cleanup  

## Project tree

├── inc
│   ├── client.h
│   ├── connection_manager.h
│   ├── server.h
│   └── utils.h
├── Makefile
└── src
    ├── chat.c
    ├── client.c
    ├── connection_manager.c
    ├── server.c
    └── utils.c

## How to Build the Project

To build the project, you need a C compiler (e.g., GCC) and the Make utility. Follow these steps:  

1. Open a terminal and navigate to the project directory.  
2. Run the following command to compile the source code:  

   ```
   make
   ```

3. The executable file will be created in the `build/out` directory with the name `chat`.  

## How to Run the Project

To run the chat application, execute the following command in the terminal:  

```
./build/out/chat <port>
```

Replace `<port>` with the port number on which the application will listen for incoming connections.  

## Available Commands

When the application is running, you can use the following commands:  

- `help`: Display information about available commands.  
- `myip`: Display the IP address of this process.  
- `myport`: Display the port this process is listening on.  
- `connect <destination> <port>`: Establish a connection to another peer.  
- `list`: Display a list of all active connections.  
- `terminate <connection id>`: Terminate a specific connection.  
- `send <connection id> <message>`: Send a message to a connected peer.  
- `exit`: Close all connections and exit the application.  

## Usage Example

1. Start the application on two different terminals (or machines):  

   **Terminal 1:**  
   ```
   ./build/out/chat 5000
   ```

   **Terminal 2:**  
   ```
   ./build/out/chat 6000
   ```

2. On Terminal 1, connect to Terminal 2:  
   ```
   > connect <IP_of_Terminal_2> 6000
   ```

3. On Terminal 1, send a message to the connection:  
   ```
   > send 1 Hello, this is a test message!
   ```

4. On Terminal 2, you will see the received message.  

5. Use `list` to view active connections and `terminate` to close them.  

## Notes

- The application supports a maximum of 100 simultaneous connections.  
- Messages are limited to 100 characters.  
- Ensure that the port numbers are unique and not used by other applications.  
- The application uses multi-threading to handle multiple connections concurrently.  

---
