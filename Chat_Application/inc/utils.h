/*
 * File: utils.h
 * Author: Chau Bui
 * Description: This header file declares utility functions for IP address handling
 *              and command parsing, used across the chat application.
 */

 #ifndef UTILS_H
 #define UTILS_H
 #include <stdbool.h>
 
 /* Function to get the local IP address (excluding loopback) */
 /* Returns a dynamically allocated string containing the IP address */
 char* get_local_ip();
 
 /* Function to validate an IPv4 address */
 /* Returns true if the IP is valid, false otherwise */
 bool is_valid_ip(const char* ip);
 
 /* Function to split a command line into tokens */
 /* Allocates and returns an array of tokens; sets num_tokens to the count */
 char** split_command(const char* line, int* num_tokens);
 
 /* Function to free memory allocated for tokens */
 /* Frees the token array and its contents */
 void free_tokens(char** tokens, int num_tokens);
 
 #endif /* UTILS_H */