/*
 * File: client.h
 * Author: Chau Bui
 * Description: This header file declares functions for client-side operations,
 *              including connecting to peers and sending messages.
 */

 #ifndef CLIENT_H
 #define CLIENT_H
 
 #include <stdbool.h>
 
 /* Function to establish a TCP connection to a peer */
 /* Returns true on success, false on failure with error message */
 bool connect_to_peer(const char* ip, int port);
 
 /* Function to send a message to a connection */
 /* Sends the message via the specified connection ID */
 void send_message(int id, const char* message);
 
 #endif /* CLIENT_H */