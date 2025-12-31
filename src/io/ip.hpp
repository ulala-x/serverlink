/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_IP_HPP_INCLUDED
#define SERVERLINK_IP_HPP_INCLUDED

#include "fd.hpp"
#include <string>

namespace slk
{
// Opens a socket with the given parameters
fd_t open_socket (int domain_, int type_, int protocol_);

// Sets the socket into non-blocking mode
void unblock_socket (fd_t s_);

// Creates a pair of sockets for signaling
// Returns 0 on success, -1 on error
int make_fdpair (fd_t *r_, fd_t *w_);

// Makes a socket non-inheritable to child processes
void make_socket_noninheritable (fd_t sock_);

// Initialize/shutdown network subsystem (mainly for Windows)
bool initialize_network ();
void shutdown_network ();

// Sets the SO_NOSIGPIPE option for the underlying socket
// Return 0 on success, -1 if the connection has been closed by the peer
int set_nosigpipe (fd_t s_);

// Enables IPv4 mapping for IPv6 sockets
void enable_ipv4_mapping (fd_t s_);

// Sets the IP Type-Of-Service field for the socket
void set_ip_type_of_service (fd_t s_, int iptos_);

// Sets the socket priority
void set_socket_priority (fd_t s_, int priority_);

// Binds the socket to a specific network device
int bind_to_device (fd_t s_, const std::string &bound_device_);

// Asserts that a socket operation succeeded or failed with a recoverable error
void assert_success_or_recoverable (fd_t s_, int rc_);
}

#endif
