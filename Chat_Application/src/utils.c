/*
 * File: utils.c
 * Author: Chau Bui
 * Description: This file contains utility functions for IP address handling and
 *              command parsing, used throughout the chat application.
 */

 #include "utils.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <ifaddrs.h>      /* For network interface info */
 #include <netinet/in.h>   /* For sockaddr_in */
 #include <arpa/inet.h>    /* For inet_ntop and INADDR_LOOPBACK */
 
 /* Function to get the local IP address (not loopback) */
 /* Allocates and returns the first non-loopback IPv4 address found */
 char* get_local_ip() {
     struct ifaddrs *ifaddr, *ifa;
     /* Get list of network interfaces */
     if (getifaddrs(&ifaddr) == -1) {
         return strdup("127.0.0.1"); /* Fallback to loopback on error */
     }
     /* Iterate through interfaces */
     for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
         if (ifa->ifa_addr == NULL) continue;
         if (ifa->ifa_addr->sa_family == AF_INET) {
             struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
             /* Check if it's not the loopback address */
             if (addr->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                 char* ip = malloc(INET_ADDRSTRLEN);
                 inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
                 freeifaddrs(ifaddr);
                 return ip; /* Return the first valid IP found */
             }
         }
     }
     freeifaddrs(ifaddr);
     return strdup("127.0.0.1"); /* Fallback if no valid IP is found */
 }
 
 /* Function to check if an IP address is valid */
 /* Uses inet_pton to validate IPv4 address format */
 bool is_valid_ip(const char* ip) {
     struct sockaddr_in sa;
     return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
 }
 
 /* Function to split a command line into tokens */
 /* Splits on spaces and returns a dynamically allocated array of tokens */
 char** split_command(const char* line, int* num_tokens) {
     char* line_copy = strdup(line); /* Duplicate to avoid modifying original */
     char* token;
     char* saveptr;
     int count = 0;
     char** tokens = NULL;
 
     /* Tokenize the line using spaces as delimiters */
     token = strtok_r(line_copy, " ", &saveptr);
     while (token != NULL) {
         tokens = realloc(tokens, sizeof(char*) * (count + 1));
         tokens[count] = strdup(token); /* Duplicate each token */
         count++;
         token = strtok_r(NULL, " ", &saveptr);
     }
     free(line_copy);
     *num_tokens = count; /* Set the number of tokens */
     return tokens;
 }
 
 /* Function to free the allocated tokens */
 /* Cleans up memory allocated by split_command */
 void free_tokens(char** tokens, int num_tokens) {
     for (int i = 0; i < num_tokens; i++) {
         free(tokens[i]); /* Free each token string */
     }
     free(tokens); /* Free the token array */
 }