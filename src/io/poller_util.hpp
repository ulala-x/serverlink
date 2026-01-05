/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_POLLER_UTIL_HPP_INCLUDED
#define SERVERLINK_POLLER_UTIL_HPP_INCLUDED

#include <string>
#include "../transport/address.hpp"

namespace slk
{
// Protocol compatibility check for IOCP poller
// IOCP only works with socket-based protocols (TCP, IPC on Windows)
// inproc uses ypipe and doesn't need IOCP
inline bool is_iocp_compatible (const std::string &protocol_)
{
#if defined SL_USE_IOCP
    // Only TCP and TCP6 are IOCP-compatible
    // inproc uses ypipe (zero-copy shared memory) - not a socket
    // IPC on Windows could use IOCP, but currently uses select
    return protocol_ == protocol_name::tcp;
#else
    (void) protocol_;
    return false;
#endif
}

// Check if a protocol requires signaler-based notification
// inproc doesn't use sockets, so it needs traditional signaler
inline bool needs_signaler (const std::string &protocol_)
{
    return protocol_ == protocol_name::inproc;
}

}  // namespace slk

#endif
