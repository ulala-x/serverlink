/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_SIGNALER_HPP_INCLUDED
#define SERVERLINK_SIGNALER_HPP_INCLUDED

#include "../util/config.hpp"

#ifdef HAVE_FORK
#include <unistd.h>
#endif

#include "fd.hpp"
#include "../util/macros.hpp"

namespace slk
{
#ifdef SL_USE_IOCP
class iocp_t;  // Forward declaration
#endif

// Cross-platform equivalent to signal_fd. As opposed to signal_fd,
// there can be at most one signal in the signaler at any given moment.
// Attempt to send a signal before receiving the previous one will
// result in undefined behaviour.

class signaler_t
{
  public:
    signaler_t ();
    ~signaler_t ();

#ifdef SL_USE_IOCP
    // Set IOCP poller for signaling (optional, for IOCP-based wakeup)
    void set_iocp (iocp_t *iocp_);
#endif

    // Returns the socket/file descriptor
    // May return retired_fd if the signaler could not be initialized
    fd_t get_fd () const;

    // Send a signal
    void send ();

    // Wait for signal with timeout (in milliseconds)
    // Returns 0 on success, -1 on timeout or error
    int wait (int timeout_) const;

    // Receive signal (blocking)
    void recv ();

    // Receive signal (non-blocking, returns -1 if no signal available)
    int recv_failable ();

    // Check if signaler is valid
    bool valid () const;

#ifdef HAVE_FORK
    // Close file descriptors in forked child process
    void forked ();
#endif

  private:
    // Underlying write & read file descriptors
    // Will be retired_fd if initialization failed
    fd_t _w;
    fd_t _r;

#ifdef SL_USE_IOCP
    // IOCP poller for PostQueuedCompletionStatus-based signaling (non-owning)
    iocp_t *_iocp;
#endif

#ifdef HAVE_FORK
    // Process that created this signaler (to detect forking)
    pid_t pid;

    // Idempotent close for destructor and forked()
    void close_internal ();
#endif

    SL_NON_COPYABLE_NOR_MOVABLE (signaler_t)
};
}

#endif
