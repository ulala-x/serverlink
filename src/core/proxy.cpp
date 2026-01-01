/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "proxy.hpp"
#include "socket_base.hpp"
#include "../msg/msg.hpp"
#include "../io/socket_poller.hpp"
#include "../util/err.hpp"
#include "../util/constants.hpp"
#include "../util/likely.hpp"

#include <stddef.h>
#include <string.h>
#include <new>

namespace slk
{

// Burst size for forwarding messages in a single iteration
// This improves throughput by batching message forwarding
static const unsigned int proxy_burst_size = 1000;

// Statistics structures
struct stats_socket
{
    uint64_t count;  // Number of messages
    uint64_t bytes;  // Total bytes
};

struct stats_endpoint
{
    stats_socket send;
    stats_socket recv;
};

struct stats_proxy
{
    stats_endpoint frontend;
    stats_endpoint backend;
};

// Proxy state machine
enum proxy_state_t
{
    active,      // Forwarding messages normally
    paused,      // Not forwarding (control command)
    terminated   // Shutting down
};

// Helper function to capture message to capture socket
static int capture_message (socket_base_t *capture_,
                           msg_t *msg_,
                           int more_ = 0)
{
    if (capture_) {
        msg_t ctrl;
        int rc = ctrl.init ();
        if (unlikely (rc < 0))
            return -1;

        rc = ctrl.copy (*msg_);
        if (unlikely (rc < 0))
            return -1;

        rc = capture_->send (&ctrl, more_ ? SL_SNDMORE : 0);
        if (unlikely (rc < 0))
            return -1;
    }
    return 0;
}

// Note: close_and_return is already defined in msg.hpp, we'll use that

// Forward messages from one socket to another
// Returns 0 on success, -1 on error
static int forward (socket_base_t *from_,
                   socket_base_t *to_,
                   socket_base_t *capture_,
                   msg_t *msg_,
                   stats_socket &recving,
                   stats_socket &sending)
{
    // Forward a burst of messages
    for (unsigned int i = 0; i < proxy_burst_size; i++) {
        int more;
        size_t moresz;

        // Forward all the parts of one message
        while (true) {
            int rc = from_->recv (msg_, SL_DONTWAIT);
            if (rc < 0) {
                if (likely (errno == EAGAIN && i > 0))
                    return 0; // End of burst

                return -1;
            }

            size_t nbytes = msg_->size ();
            recving.count += 1;
            recving.bytes += nbytes;

            moresz = sizeof (more);
            rc = from_->getsockopt (SL_RCVMORE, &more, &moresz);
            if (unlikely (rc < 0))
                return -1;

            // Copy message to capture socket if any
            rc = capture_message (capture_, msg_, more);
            if (unlikely (rc < 0))
                return -1;

            rc = to_->send (msg_, more ? SL_SNDMORE : 0);
            if (unlikely (rc < 0))
                return -1;

            sending.count += 1;
            sending.bytes += nbytes;

            if (more == 0)
                break;
        }
    }

    return 0;
}

// Handle control socket commands
// Commands: PAUSE, RESUME, TERMINATE, STATISTICS
// Returns 0 on success, -1 on error
static int handle_control (socket_base_t *control_,
                          proxy_state_t &state_,
                          const stats_proxy &stats_)
{
    msg_t cmsg;
    int rc = cmsg.init ();
    if (rc != 0) {
        return -1;
    }

    rc = control_->recv (&cmsg, SL_DONTWAIT);
    if (rc < 0) {
        return -1;
    }

    const uint8_t *command = static_cast<const uint8_t *> (cmsg.data ());
    const size_t msiz = cmsg.size ();

    // STATISTICS command - return 8-part message with stats
    if (msiz == 10 && 0 == memcmp (command, "STATISTICS", 10)) {
        // The stats are a cross product:
        // (Front,Back) X (Recv,Sent) X (Number,Bytes)
        // flattened into 8 message parts:
        // (frn, frb, fsn, fsb, brn, brb, bsn, bsb)
        // f=front/b=back, r=recv/s=send, n=number/b=bytes
        const uint64_t stat_vals[8] = {
            stats_.frontend.recv.count, stats_.frontend.recv.bytes,
            stats_.frontend.send.count, stats_.frontend.send.bytes,
            stats_.backend.recv.count,  stats_.backend.recv.bytes,
            stats_.backend.send.count,  stats_.backend.send.bytes
        };

        for (size_t ind = 0; ind < 8; ++ind) {
            cmsg.init_size (sizeof (uint64_t));
            memcpy (cmsg.data (), stat_vals + ind, sizeof (uint64_t));
            rc = control_->send (&cmsg, ind < 7 ? SL_SNDMORE : 0);
            if (unlikely (rc < 0)) {
                return -1;
            }
        }
        return 0;
    }

    // State change commands
    if (msiz == 5 && 0 == memcmp (command, "PAUSE", 5)) {
        state_ = paused;
    } else if (msiz == 6 && 0 == memcmp (command, "RESUME", 6)) {
        state_ = active;
    } else if (msiz == 9 && 0 == memcmp (command, "TERMINATE", 9)) {
        state_ = terminated;
    }

    // Satisfy REP duty and reply no matter what
    cmsg.init_size (0);
    rc = control_->send (&cmsg, 0);
    if (unlikely (rc < 0)) {
        return -1;
    }

    return 0;
}

// Macro to clean up pollers on error
#define PROXY_CLEANUP()                                                        \
    do {                                                                       \
        delete poller_all;                                                     \
        delete poller_in;                                                      \
        delete poller_receive_blocked;                                         \
        delete poller_send_blocked;                                            \
        delete poller_both_blocked;                                            \
        delete poller_frontend_only;                                           \
        delete poller_backend_only;                                            \
    } while (false)

#define CHECK_RC_EXIT_ON_FAILURE()                                             \
    do {                                                                       \
        if (rc < 0) {                                                          \
            PROXY_CLEANUP ();                                                  \
            return close_and_return (&msg, -1);                                \
        }                                                                      \
    } while (false)

// Simple proxy implementation
int proxy (socket_base_t *frontend_,
          socket_base_t *backend_,
          socket_base_t *capture_)
{
    return proxy_steerable (frontend_, backend_, capture_, NULL);
}

// Steerable proxy implementation with control socket
int proxy_steerable (socket_base_t *frontend_,
                    socket_base_t *backend_,
                    socket_base_t *capture_,
                    socket_base_t *control_)
{
    msg_t msg;
    int rc = msg.init ();
    if (rc != 0)
        return -1;

    // Proxy can be in these three states
    proxy_state_t state = active;

    bool frontend_equal_to_backend;
    bool frontend_in = false;
    bool frontend_out = false;
    bool backend_in = false;
    bool backend_out = false;
    socket_poller_t::event_t events[4];
    int nevents = 3; // increase to 4 if we have control_

    stats_proxy stats = {{{0, 0}, {0, 0}}, {{0, 0}, {0, 0}}};

    // Don't allocate these pollers from stack because they will take more than 900 kB of stack!
    // On Windows this blows up default stack of 1 MB and aborts the program.
    socket_poller_t *poller_all =
        new (std::nothrow) socket_poller_t; // Poll for everything
    socket_poller_t *poller_in =
        new (std::nothrow) socket_poller_t; // Poll only 'SL_POLLIN' on all sockets
    socket_poller_t *poller_receive_blocked =
        new (std::nothrow) socket_poller_t; // All except 'SL_POLLIN' on 'frontend_'

    // If frontend_==backend_ 'poller_send_blocked' and 'poller_receive_blocked' are the same
    socket_poller_t *poller_send_blocked = NULL;
    socket_poller_t *poller_both_blocked = NULL;
    socket_poller_t *poller_frontend_only = NULL;
    socket_poller_t *poller_backend_only = NULL;

    if (frontend_ != backend_) {
        poller_send_blocked = new (std::nothrow) socket_poller_t;
        poller_both_blocked = new (std::nothrow) socket_poller_t;
        poller_frontend_only = new (std::nothrow) socket_poller_t;
        poller_backend_only = new (std::nothrow) socket_poller_t;
        frontend_equal_to_backend = false;
    } else {
        frontend_equal_to_backend = true;
    }

    if (poller_all == NULL || poller_in == NULL
        || poller_receive_blocked == NULL
        || ((poller_send_blocked == NULL || poller_both_blocked == NULL)
            && !frontend_equal_to_backend)) {
        PROXY_CLEANUP ();
        return close_and_return (&msg, -1);
    }

    socket_poller_t *poller_wait = poller_in; // Poller for blocking wait

    // Register 'frontend_' and 'backend_' with pollers
    rc = poller_all->add (frontend_, NULL, SL_POLLIN | SL_POLLOUT);
    CHECK_RC_EXIT_ON_FAILURE ();
    rc = poller_in->add (frontend_, NULL, SL_POLLIN);
    CHECK_RC_EXIT_ON_FAILURE ();

    if (frontend_equal_to_backend) {
        rc = poller_receive_blocked->add (frontend_, NULL, SL_POLLOUT);
        CHECK_RC_EXIT_ON_FAILURE ();
    } else {
        rc = poller_all->add (backend_, NULL, SL_POLLIN | SL_POLLOUT);
        CHECK_RC_EXIT_ON_FAILURE ();
        rc = poller_in->add (backend_, NULL, SL_POLLIN);
        CHECK_RC_EXIT_ON_FAILURE ();
        rc = poller_both_blocked->add (frontend_, NULL, SL_POLLOUT);
        CHECK_RC_EXIT_ON_FAILURE ();
        rc = poller_both_blocked->add (backend_, NULL, SL_POLLOUT);
        CHECK_RC_EXIT_ON_FAILURE ();
        rc = poller_send_blocked->add (backend_, NULL, SL_POLLOUT);
        CHECK_RC_EXIT_ON_FAILURE ();
        rc = poller_send_blocked->add (frontend_, NULL, SL_POLLIN | SL_POLLOUT);
        CHECK_RC_EXIT_ON_FAILURE ();
        rc = poller_receive_blocked->add (frontend_, NULL, SL_POLLOUT);
        CHECK_RC_EXIT_ON_FAILURE ();
        rc = poller_receive_blocked->add (backend_, NULL, SL_POLLIN | SL_POLLOUT);
        CHECK_RC_EXIT_ON_FAILURE ();
        rc = poller_frontend_only->add (frontend_, NULL, SL_POLLIN | SL_POLLOUT);
        CHECK_RC_EXIT_ON_FAILURE ();
        rc = poller_backend_only->add (backend_, NULL, SL_POLLIN | SL_POLLOUT);
        CHECK_RC_EXIT_ON_FAILURE ();
    }

    if (control_) {
        ++nevents;

        rc = poller_all->add (control_, NULL, SL_POLLIN);
        CHECK_RC_EXIT_ON_FAILURE ();

        rc = poller_in->add (control_, NULL, SL_POLLIN);
        CHECK_RC_EXIT_ON_FAILURE ();

        rc = poller_receive_blocked->add (control_, NULL, SL_POLLIN);
        CHECK_RC_EXIT_ON_FAILURE ();

        if (!frontend_equal_to_backend) {
            rc = poller_send_blocked->add (control_, NULL, SL_POLLIN);
            CHECK_RC_EXIT_ON_FAILURE ();

            rc = poller_both_blocked->add (control_, NULL, SL_POLLIN);
            CHECK_RC_EXIT_ON_FAILURE ();

            rc = poller_frontend_only->add (control_, NULL, SL_POLLIN);
            CHECK_RC_EXIT_ON_FAILURE ();

            rc = poller_backend_only->add (control_, NULL, SL_POLLIN);
            CHECK_RC_EXIT_ON_FAILURE ();
        }
    }

    bool request_processed = false;
    bool reply_processed = false;

    while (state != terminated) {
        // Blocking wait initially only for 'SL_POLLIN'
        rc = poller_wait->wait (events, nevents, -1);
        if (rc < 0 && errno == EAGAIN)
            rc = 0;
        CHECK_RC_EXIT_ON_FAILURE ();

        // Some events have arrived, now poll for everything without blocking
        rc = poller_all->wait (events, nevents, 0);
        if (rc < 0 && errno == EAGAIN)
            rc = 0;
        CHECK_RC_EXIT_ON_FAILURE ();

        // Process events
        for (int i = 0; i < rc; i++) {
            if (control_ && events[i].socket == control_) {
                rc = handle_control (control_, state, stats);
                CHECK_RC_EXIT_ON_FAILURE ();
                continue;
            }

            if (events[i].socket == frontend_) {
                frontend_in = (events[i].events & SL_POLLIN) != 0;
                frontend_out = (events[i].events & SL_POLLOUT) != 0;
            } else if (events[i].socket == backend_) {
                backend_in = (events[i].events & SL_POLLIN) != 0;
                backend_out = (events[i].events & SL_POLLOUT) != 0;
            }
        }

        if (state == active) {
            // Process a request: 'SL_POLLIN' on 'frontend_' and 'SL_POLLOUT' on 'backend_'
            if (frontend_in && (backend_out || frontend_equal_to_backend)) {
                rc = forward (frontend_, backend_, capture_, &msg,
                            stats.frontend.recv, stats.backend.send);
                CHECK_RC_EXIT_ON_FAILURE ();
                request_processed = true;
                frontend_in = backend_out = false;
            } else {
                request_processed = false;
            }

            // Process a reply: 'SL_POLLIN' on 'backend_' and 'SL_POLLOUT' on 'frontend_'
            if (backend_in && frontend_out) {
                rc = forward (backend_, frontend_, capture_, &msg,
                            stats.backend.recv, stats.frontend.send);
                CHECK_RC_EXIT_ON_FAILURE ();
                reply_processed = true;
                backend_in = frontend_out = false;
            } else {
                reply_processed = false;
            }

            if (request_processed || reply_processed) {
                // If request/reply is processed, enable corresponding 'SL_POLLIN' for blocking wait
                if (poller_wait != poller_in) {
                    if (request_processed) {
                        if (poller_wait == poller_both_blocked)
                            poller_wait = poller_send_blocked;
                        else if (poller_wait == poller_receive_blocked
                                || poller_wait == poller_frontend_only)
                            poller_wait = poller_in;
                    }
                    if (reply_processed) {
                        if (poller_wait == poller_both_blocked)
                            poller_wait = poller_receive_blocked;
                        else if (poller_wait == poller_send_blocked
                                || poller_wait == poller_backend_only)
                            poller_wait = poller_in;
                    }
                }
            } else {
                // No requests have been processed
                // Disable receiving 'SL_POLLIN' for sockets for which there's no 'SL_POLLOUT'
                if (frontend_in) {
                    if (frontend_out)
                        poller_wait = poller_backend_only;
                    else {
                        if (poller_wait == poller_send_blocked)
                            poller_wait = poller_both_blocked;
                        else if (poller_wait == poller_in)
                            poller_wait = poller_receive_blocked;
                    }
                }
                if (backend_in) {
                    if (backend_out)
                        poller_wait = poller_frontend_only;
                    else {
                        if (poller_wait == poller_receive_blocked)
                            poller_wait = poller_both_blocked;
                        else if (poller_wait == poller_in)
                            poller_wait = poller_send_blocked;
                    }
                }
            }
        }
    }

    PROXY_CLEANUP ();
    return close_and_return (&msg, 0);
}

} // namespace slk
