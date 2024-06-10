/*
 * MIT License
 * Copyright (c) 2024 William Vallet
 */

#ifndef UDP_CLIENT_H_
#define UDP_CLIENT_H_

#include "lwip/sockets.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

// Open an UDP socket to IP:PORT with multicast option.
// Return the client handle, 0 if error.
extern uintptr_t udp_client_open(
    const char * const ip_str, uint16_t port, bool multicast);
// Close the client connection.
// Return 0 on success, -1 if error.
extern int udp_client_close(uintptr_t handle);
// Define receive timeout.
// Return 0 on success, -1 if error.
extern int udp_client_set_timeout(
    uintptr_t handle, const struct timeval * const timeout);
// Check if the client received data.
// Return number of bytes received, -1 if error.
extern int udp_client_poll(uintptr_t handle);
// Receive data from client with sender address.
// Return number of bytes read, -1 if error.
extern int udp_client_read(
    uintptr_t handle, uint8_t * const payload, size_t size,
    struct sockaddr * const from);
// Send data to client.
// Return number of bytes written, -1 if error.
extern int udp_client_write(
    uintptr_t handle, const uint8_t * const payload, size_t size);

#endif  // UDP_CLIENT_H_
