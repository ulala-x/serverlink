/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_PROXY_HPP_INCLUDED
#define SL_PROXY_HPP_INCLUDED

namespace slk
{
class socket_base_t;

// Simple proxy - forwards messages between frontend and backend
// with optional capture socket for monitoring
// Returns 0 on success, -1 on error
int proxy (socket_base_t *frontend_,
           socket_base_t *backend_,
           socket_base_t *capture_);

// Steerable proxy - adds control socket for runtime control
// Control commands:
//   - "PAUSE": Stop forwarding messages temporarily
//   - "RESUME": Resume forwarding messages
//   - "TERMINATE": Terminate the proxy and return
//   - "STATISTICS": Return statistics (8-part message with counts/bytes)
// Returns 0 on success, -1 on error
int proxy_steerable (socket_base_t *frontend_,
                     socket_base_t *backend_,
                     socket_base_t *capture_,
                     socket_base_t *control_);
}

#endif
