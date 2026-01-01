/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Public C API Implementation */

#include "serverlink/serverlink.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <poll.h>
#endif

#include "core/ctx.hpp"
#include "core/socket_base.hpp"
#include "core/router.hpp"
#include "msg/msg.hpp"
#include "util/err.hpp"
#include "util/clock.hpp"
#include "util/constants.hpp"

#ifdef SL_ENABLE_MONITORING
#include "monitor/connection_manager.hpp"
#include "monitor/event_dispatcher.hpp"
#include "monitor/peer_stats.hpp"
#include "msg/blob.hpp"
#include <vector>
#endif

// Thread-local storage for errno
#ifdef _WIN32
static __declspec(thread) int slk_errno_value = 0;
#else
static __thread int slk_errno_value = 0;
#endif

// Helper function to set errno and return error code
static inline int set_errno(int err)
{
    errno = err;
    slk_errno_value = err;
    return -1;
}

// Helper to convert internal error codes to public API error codes
static int map_errno(int internal_errno)
{
    switch (internal_errno) {
        case EINVAL:
            return SLK_EINVAL;
        case ENOMEM:
            return SLK_ENOMEM;
        case EAGAIN:
#if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:
#endif
            return SLK_EAGAIN;
        case ENOTSOCK:
            return SLK_ENOTSOCK;
        case EPROTO:
            return SLK_EPROTO;
        case SL_ETERM:
            return SLK_ETERM;
        case SL_EMTHREAD:
            return SLK_EMTHREAD;
        case EHOSTUNREACH:
            return SLK_EHOSTUNREACH;
        default:
            return internal_errno;
    }
}

// Helper to validate pointers
#define CHECK_PTR(ptr, ret) \
    do { \
        if (!(ptr)) { \
            set_errno(SLK_EINVAL); \
            return ret; \
        } \
    } while(0)

#define CHECK_PTR_VOID(ptr) \
    do { \
        if (!(ptr)) { \
            set_errno(SLK_EINVAL); \
            return; \
        } \
    } while(0)

extern "C" {

/****************************************************************************/
/*  Version Information                                                     */
/****************************************************************************/

void SL_CALL slk_version(int *major, int *minor, int *patch)
{
    if (major) *major = SLK_VERSION_MAJOR;
    if (minor) *minor = SLK_VERSION_MINOR;
    if (patch) *patch = SLK_VERSION_PATCH;
}

/****************************************************************************/
/*  Error Handling                                                          */
/****************************************************************************/

int SL_CALL slk_errno(void)
{
    return slk_errno_value;
}

const char* SL_CALL slk_strerror(int errnum)
{
    switch (errnum) {
        case SLK_EINVAL:
            return "Invalid argument";
        case SLK_ENOMEM:
            return "Out of memory";
        case SLK_EAGAIN:
            return "Resource temporarily unavailable";
        case SLK_ENOTSOCK:
            return "Not a socket";
        case SLK_EPROTO:
            return "Protocol error";
        case SLK_ETERM:
            return "Context terminated";
        case SLK_EMTHREAD:
            return "No I/O thread available";
        case SLK_EHOSTUNREACH:
            return "Host unreachable";
        case SLK_ENOTREADY:
            return "Socket not ready";
        case SLK_EPEERUNREACH:
            return "Peer unreachable";
        case SLK_EAUTH:
            return "Authentication failed";
        default:
            return "Unknown error";
    }
}

/****************************************************************************/
/*  Context Management                                                      */
/****************************************************************************/

slk_ctx_t* SL_CALL slk_ctx_new(void)
{
    try {
        slk::ctx_t *ctx = new slk::ctx_t();
        if (!ctx || !ctx->valid()) {
            delete ctx;
            set_errno(SLK_ENOMEM);
            return nullptr;
        }
        return reinterpret_cast<slk_ctx_t*>(ctx);
    } catch (const std::bad_alloc&) {
        set_errno(SLK_ENOMEM);
        return nullptr;
    } catch (...) {
        set_errno(SLK_EPROTO);
        return nullptr;
    }
}

void SL_CALL slk_ctx_destroy(slk_ctx_t *ctx_)
{
    if (!ctx_) {
        return;
    }

    slk::ctx_t *ctx = reinterpret_cast<slk::ctx_t*>(ctx_);

    try {
        // NOTE: terminate() calls "delete this", so we don't delete ctx here
        ctx->terminate();
    } catch (...) {
        // Best effort cleanup - terminate() should have deleted the object
    }
}

int SL_CALL slk_ctx_set(slk_ctx_t *ctx_, int option, const void *value, size_t len)
{
    CHECK_PTR(ctx_, -1);
    CHECK_PTR(value, -1);

    slk::ctx_t *ctx = reinterpret_cast<slk::ctx_t*>(ctx_);

    try {
        int rc = ctx->set(option, value, len);
        if (rc != 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_ctx_get(slk_ctx_t *ctx_, int option, void *value, size_t *len)
{
    CHECK_PTR(ctx_, -1);
    CHECK_PTR(value, -1);
    CHECK_PTR(len, -1);

    slk::ctx_t *ctx = reinterpret_cast<slk::ctx_t*>(ctx_);

    try {
        int rc = ctx->get(option, value, len);
        if (rc < 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

/****************************************************************************/
/*  Socket Management                                                       */
/****************************************************************************/

slk_socket_t* SL_CALL slk_socket(slk_ctx_t *ctx_, int type)
{
    CHECK_PTR(ctx_, nullptr);

    slk::ctx_t *ctx = reinterpret_cast<slk::ctx_t*>(ctx_);

    try {
        slk::socket_base_t *socket = ctx->create_socket(type);
        if (!socket) {
            set_errno(map_errno(errno));
            return nullptr;
        }
        return reinterpret_cast<slk_socket_t*>(socket);
    } catch (const std::bad_alloc&) {
        set_errno(SLK_ENOMEM);
        return nullptr;
    } catch (...) {
        set_errno(SLK_EPROTO);
        return nullptr;
    }
}

int SL_CALL slk_close(slk_socket_t *socket_)
{
    CHECK_PTR(socket_, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        int rc = socket->close();
        if (rc != 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_bind(slk_socket_t *socket_, const char *endpoint)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(endpoint, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        int rc = socket->bind(endpoint);
        if (rc != 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_connect(slk_socket_t *socket_, const char *endpoint)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(endpoint, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        int rc = socket->connect(endpoint);
        if (rc != 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_disconnect(slk_socket_t *socket_, const char *endpoint)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(endpoint, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        int rc = socket->term_endpoint(endpoint);
        if (rc != 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_unbind(slk_socket_t *socket_, const char *endpoint)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(endpoint, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        int rc = socket->term_endpoint(endpoint);
        if (rc != 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_setsockopt(slk_socket_t *socket_, int option, const void *value, size_t len)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(value, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        int rc = socket->setsockopt(option, value, len);
        if (rc != 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_getsockopt(slk_socket_t *socket_, int option, void *value, size_t *len)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(value, -1);
    CHECK_PTR(len, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        int rc = socket->getsockopt(option, value, len);
        if (rc != 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

/****************************************************************************/
/*  Message API                                                             */
/****************************************************************************/

slk_msg_t* SL_CALL slk_msg_new(void)
{
    try {
        slk::msg_t *msg = new slk::msg_t();
        if (msg->init() != 0) {
            delete msg;
            set_errno(SLK_ENOMEM);
            return nullptr;
        }
        return reinterpret_cast<slk_msg_t*>(msg);
    } catch (const std::bad_alloc&) {
        set_errno(SLK_ENOMEM);
        return nullptr;
    } catch (...) {
        set_errno(SLK_EPROTO);
        return nullptr;
    }
}

slk_msg_t* SL_CALL slk_msg_new_data(const void *data, size_t size)
{
    CHECK_PTR(data, nullptr);

    try {
        slk::msg_t *msg = new slk::msg_t();
        if (msg->init_buffer(data, size) != 0) {
            delete msg;
            set_errno(SLK_ENOMEM);
            return nullptr;
        }
        return reinterpret_cast<slk_msg_t*>(msg);
    } catch (const std::bad_alloc&) {
        set_errno(SLK_ENOMEM);
        return nullptr;
    } catch (...) {
        set_errno(SLK_EPROTO);
        return nullptr;
    }
}

void SL_CALL slk_msg_destroy(slk_msg_t *msg_)
{
    if (!msg_) {
        return;
    }

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);

    try {
        msg->close();
        delete msg;
    } catch (...) {
        // Best effort cleanup
        delete msg;
    }
}

int SL_CALL slk_msg_init(slk_msg_t *msg_)
{
    CHECK_PTR(msg_, -1);

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);

    try {
        int rc = msg->init();
        if (rc != 0) {
            return set_errno(SLK_ENOMEM);
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_msg_init_data(slk_msg_t *msg_, const void *data, size_t size)
{
    CHECK_PTR(msg_, -1);
    CHECK_PTR(data, -1);

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);

    try {
        int rc = msg->init_buffer(data, size);
        if (rc != 0) {
            return set_errno(SLK_ENOMEM);
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_msg_close(slk_msg_t *msg_)
{
    CHECK_PTR(msg_, -1);

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);

    try {
        int rc = msg->close();
        if (rc != 0) {
            return set_errno(SLK_EPROTO);
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

void* SL_CALL slk_msg_data(slk_msg_t *msg_)
{
    CHECK_PTR(msg_, nullptr);

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);

    try {
        return msg->data();
    } catch (...) {
        set_errno(SLK_EPROTO);
        return nullptr;
    }
}

size_t SL_CALL slk_msg_size(slk_msg_t *msg_)
{
    CHECK_PTR(msg_, 0);

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);

    try {
        return msg->size();
    } catch (...) {
        set_errno(SLK_EPROTO);
        return 0;
    }
}

int SL_CALL slk_msg_copy(slk_msg_t *dest_, slk_msg_t *src_)
{
    CHECK_PTR(dest_, -1);
    CHECK_PTR(src_, -1);

    slk::msg_t *dest = reinterpret_cast<slk::msg_t*>(dest_);
    slk::msg_t *src = reinterpret_cast<slk::msg_t*>(src_);

    try {
        int rc = dest->copy(*src);
        if (rc != 0) {
            return set_errno(SLK_EPROTO);
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_msg_move(slk_msg_t *dest_, slk_msg_t *src_)
{
    CHECK_PTR(dest_, -1);
    CHECK_PTR(src_, -1);

    slk::msg_t *dest = reinterpret_cast<slk::msg_t*>(dest_);
    slk::msg_t *src = reinterpret_cast<slk::msg_t*>(src_);

    try {
        int rc = dest->move(*src);
        if (rc != 0) {
            return set_errno(SLK_EPROTO);
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_msg_get(slk_msg_t *msg_, int property, void *value, size_t *len)
{
    CHECK_PTR(msg_, -1);
    CHECK_PTR(value, -1);
    CHECK_PTR(len, -1);

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);

    // Handle specific properties
    // This is a simplified implementation - extend as needed
    try {
        if (property == 0) { // MORE flag
            if (*len < sizeof(int)) {
                return set_errno(SLK_EINVAL);
            }
            *reinterpret_cast<int*>(value) = (msg->flags() & slk::msg_t::more) ? 1 : 0;
            *len = sizeof(int);
            return 0;
        }
        return set_errno(SLK_EINVAL);
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_msg_set(slk_msg_t *msg_, int property, const void *value, size_t len)
{
    CHECK_PTR(msg_, -1);
    CHECK_PTR(value, -1);

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);

    // Handle specific properties
    // This is a simplified implementation - extend as needed
    try {
        if (property == 0) { // MORE flag
            if (len < sizeof(int)) {
                return set_errno(SLK_EINVAL);
            }
            int more = *reinterpret_cast<const int*>(value);
            if (more) {
                msg->set_flags(slk::msg_t::more);
            } else {
                msg->reset_flags(slk::msg_t::more);
            }
            return 0;
        }
        return set_errno(SLK_EINVAL);
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_msg_get_routing_id(slk_msg_t *msg_, void *id, size_t *size)
{
    CHECK_PTR(msg_, -1);
    CHECK_PTR(id, -1);
    CHECK_PTR(size, -1);

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);

    try {
        uint32_t routing_id = msg->get_routing_id();
        if (*size < sizeof(uint32_t)) {
            return set_errno(SLK_EINVAL);
        }
        memcpy(id, &routing_id, sizeof(uint32_t));
        *size = sizeof(uint32_t);
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_msg_set_routing_id(slk_msg_t *msg_, const void *id, size_t size)
{
    CHECK_PTR(msg_, -1);
    CHECK_PTR(id, -1);

    if (size != sizeof(uint32_t)) {
        return set_errno(SLK_EINVAL);
    }

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);

    try {
        uint32_t routing_id;
        memcpy(&routing_id, id, sizeof(uint32_t));
        int rc = msg->set_routing_id(routing_id);
        if (rc != 0) {
            return set_errno(SLK_EPROTO);
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

/****************************************************************************/
/*  Send/Receive API                                                        */
/****************************************************************************/

int SL_CALL slk_send(slk_socket_t *socket_, const void *data, size_t len, int flags)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(data, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        slk::msg_t msg;
        if (msg.init_buffer(data, len) != 0) {
            return set_errno(SLK_ENOMEM);
        }

        int rc = socket->send(&msg, flags);
        msg.close();

        if (rc < 0) {
            return set_errno(map_errno(errno));
        }
        return static_cast<int>(len);
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_recv(slk_socket_t *socket_, void *buf, size_t len, int flags)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(buf, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        slk::msg_t msg;
        if (msg.init() != 0) {
            return set_errno(SLK_ENOMEM);
        }

        int rc = socket->recv(&msg, flags);
        if (rc < 0) {
            msg.close();
            return set_errno(map_errno(errno));
        }

        size_t msg_size = msg.size();
        size_t copy_size = (msg_size < len) ? msg_size : len;
        memcpy(buf, msg.data(), copy_size);

        msg.close();
        return static_cast<int>(copy_size);
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_msg_send(slk_msg_t *msg_, slk_socket_t *socket_, int flags)
{
    CHECK_PTR(msg_, -1);
    CHECK_PTR(socket_, -1);

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);
    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        int rc = socket->send(msg, flags);
        if (rc < 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_msg_recv(slk_msg_t *msg_, slk_socket_t *socket_, int flags)
{
    CHECK_PTR(msg_, -1);
    CHECK_PTR(socket_, -1);

    slk::msg_t *msg = reinterpret_cast<slk::msg_t*>(msg_);
    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        int rc = socket->recv(msg, flags);
        if (rc < 0) {
            return set_errno(map_errno(errno));
        }
        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_send_to(slk_socket_t *socket_, const void *routing_id, size_t id_len,
                        const void *data, size_t data_len, int flags)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(routing_id, -1);
    CHECK_PTR(data, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        // Send routing ID frame
        slk::msg_t id_msg;
        if (id_msg.init_buffer(routing_id, id_len) != 0) {
            return set_errno(SLK_ENOMEM);
        }

        int rc = socket->send(&id_msg, SLK_SNDMORE);
        id_msg.close();

        if (rc < 0) {
            return set_errno(map_errno(errno));
        }

        // Send data frame
        slk::msg_t data_msg;
        if (data_msg.init_buffer(data, data_len) != 0) {
            return set_errno(SLK_ENOMEM);
        }

        rc = socket->send(&data_msg, flags);
        data_msg.close();

        if (rc < 0) {
            return set_errno(map_errno(errno));
        }

        return static_cast<int>(data_len);
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

/****************************************************************************/
/*  Polling API                                                             */
/****************************************************************************/

int SL_CALL slk_poll(slk_pollitem_t *items, int nitems, long timeout)
{
    CHECK_PTR(items, -1);

    if (nitems <= 0) {
        return set_errno(SLK_EINVAL);
    }

    try {
        // CRITICAL: Process pending commands on all sockets before checking readiness
        // This ensures that any bind/activate_read commands from inproc connections
        // are processed, avoiding timing issues where messages are sent but the
        // receiving socket hasn't processed the pipe attachment yet.
        for (int i = 0; i < nitems; i++) {
            if (items[i].socket) {
                slk::socket_base_t *socket =
                    reinterpret_cast<slk::socket_base_t*>(items[i].socket);
                // Process commands with 0 timeout (non-blocking)
                int rc = socket->process_commands(0, false);
                if (rc != 0) {
                    // Command processing error, propagate errno
                    return -1;
                }
            }
        }

        // Check for immediate events (before potentially blocking in poll)
        int ready_count = 0;
        for (int i = 0; i < nitems; i++) {
            items[i].revents = 0;

            if (items[i].socket) {
                slk::socket_base_t *socket =
                    reinterpret_cast<slk::socket_base_t*>(items[i].socket);

                if ((items[i].events & SLK_POLLIN) && socket->has_in()) {
                    items[i].revents |= SLK_POLLIN;
                    ready_count++;
                }

                if ((items[i].events & SLK_POLLOUT) && socket->has_out()) {
                    items[i].revents |= SLK_POLLOUT;
                    ready_count++;
                }
            }
        }

        // If we found events or timeout is 0, return immediately
        if (ready_count > 0 || timeout == 0) {
            return ready_count;
        }

        // No immediate events and timeout != 0, so we need to wait using poll()
        // Build the pollfd array for system poll() call
#ifdef _WIN32
        // Windows: Use select() since poll() is not available on all versions
        fd_set readfds, writefds, exceptfds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);

        SOCKET max_fd = 0;
        for (int i = 0; i < nitems; i++) {
            if (items[i].socket) {
                slk::socket_base_t *socket =
                    reinterpret_cast<slk::socket_base_t*>(items[i].socket);

                size_t fd_size = sizeof(SOCKET);
                SOCKET fd;
                if (socket->getsockopt(slk::SL_FD, &fd, &fd_size) == 0) {
                    if (fd != INVALID_SOCKET) {
                        FD_SET(fd, &readfds);
                        FD_SET(fd, &exceptfds);
                        if (fd > max_fd) max_fd = fd;
                    }
                }
            }
        }

        struct timeval tv;
        struct timeval *ptv = nullptr;
        if (timeout >= 0) {
            tv.tv_sec = static_cast<long>(timeout / 1000);
            tv.tv_usec = static_cast<long>((timeout % 1000) * 1000);
            ptv = &tv;
        }

        int rc = select(static_cast<int>(max_fd + 1), &readfds, nullptr, &exceptfds, ptv);
        if (rc == SOCKET_ERROR) {
            return set_errno(SLK_EPROTO);
        }
#else
        // Unix: Use poll() system call
        struct pollfd *pollfds = static_cast<struct pollfd*>(
            malloc(nitems * sizeof(struct pollfd)));
        if (!pollfds) {
            return set_errno(SLK_ENOMEM);
        }

        int pollfd_count = 0;
        for (int i = 0; i < nitems; i++) {
            pollfds[i].fd = -1;
            pollfds[i].events = 0;
            pollfds[i].revents = 0;

            if (items[i].socket) {
                slk::socket_base_t *socket =
                    reinterpret_cast<slk::socket_base_t*>(items[i].socket);

                size_t fd_size = sizeof(slk::fd_t);
                slk::fd_t fd;
                if (socket->getsockopt(slk::SL_FD, &fd, &fd_size) == 0) {
                    pollfds[i].fd = fd;
                    pollfds[i].events = POLLIN;  // Always wait for mailbox signaling
                    pollfd_count++;
                }
            } else if (items[i].fd >= 0) {
                pollfds[i].fd = items[i].fd;
                if (items[i].events & SLK_POLLIN)
                    pollfds[i].events |= POLLIN;
                if (items[i].events & SLK_POLLOUT)
                    pollfds[i].events |= POLLOUT;
                pollfd_count++;
            }
        }

        // Compute timeout for poll (in milliseconds)
        int poll_timeout;
        if (timeout < 0) {
            poll_timeout = -1;  // Infinite
        } else if (timeout > INT_MAX) {
            poll_timeout = INT_MAX;
        } else {
            poll_timeout = static_cast<int>(timeout);
        }

        slk::clock_t clock;
        uint64_t now = 0;
        uint64_t end = 0;

        // For finite timeouts, track elapsed time
        if (timeout > 0) {
            now = clock.now_ms();
            end = now + timeout;
        }

        while (true) {
            int rc = poll(pollfds, pollfd_count, poll_timeout);

            if (rc < 0) {
                // Error occurred
                if (errno == EINTR) {
                    // Interrupted by signal, adjust timeout and retry
                    if (timeout > 0) {
                        now = clock.now_ms();
                        if (now >= end) {
                            // Timeout expired
                            free(pollfds);
                            return 0;
                        }
                        poll_timeout = static_cast<int>(end - now);
                    }
                    continue;
                }
                free(pollfds);
                return set_errno(SLK_EPROTO);
            } else if (rc == 0) {
                // Timeout expired with no events
                free(pollfds);
                return 0;
            }

            // poll() returned, now process commands and check for actual events
            // This two-phase approach ensures we detect ZMQ-level events correctly
            for (int i = 0; i < nitems; i++) {
                if (items[i].socket && pollfds[i].fd != -1) {
                    slk::socket_base_t *socket =
                        reinterpret_cast<slk::socket_base_t*>(items[i].socket);
                    // Process any pending commands
                    socket->process_commands(0, false);
                }
            }

            // Now check for actual socket events
            ready_count = 0;
            for (int i = 0; i < nitems; i++) {
                items[i].revents = 0;

                if (items[i].socket) {
                    slk::socket_base_t *socket =
                        reinterpret_cast<slk::socket_base_t*>(items[i].socket);

                    if ((items[i].events & SLK_POLLIN) && socket->has_in()) {
                        items[i].revents |= SLK_POLLIN;
                        ready_count++;
                    }

                    if ((items[i].events & SLK_POLLOUT) && socket->has_out()) {
                        items[i].revents |= SLK_POLLOUT;
                        ready_count++;
                    }
                } else if (items[i].fd >= 0 && pollfds[i].fd >= 0) {
                    // Raw file descriptor
                    if ((pollfds[i].revents & POLLIN) && (items[i].events & SLK_POLLIN))
                        items[i].revents |= SLK_POLLIN;
                    if ((pollfds[i].revents & POLLOUT) && (items[i].events & SLK_POLLOUT))
                        items[i].revents |= SLK_POLLOUT;
                    if (pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                        items[i].revents |= SLK_POLLERR;

                    if (items[i].revents)
                        ready_count++;
                }
            }

            // If we found events, return them
            if (ready_count > 0) {
                free(pollfds);
                return ready_count;
            }

            // No events found after processing, adjust timeout and retry
            if (timeout > 0) {
                now = clock.now_ms();
                if (now >= end) {
                    // Timeout expired
                    free(pollfds);
                    return 0;
                }
                poll_timeout = static_cast<int>(end - now);
            }
            // For infinite timeout (timeout < 0), loop continues indefinitely
        }
#endif
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

/****************************************************************************/
/*  Monitoring API                                                          */
/****************************************************************************/

#ifdef SL_ENABLE_MONITORING
// Wrapper structure to bridge public API callback to internal callback
struct monitor_callback_wrapper_t {
    slk_monitor_fn user_callback;
    void *user_data;
};

// Internal callback that converts event_data_t to slk_event_t
static void internal_monitor_callback(slk::socket_base_t *socket,
                                      const slk::event_data_t *event,
                                      void *user_data)
{
    monitor_callback_wrapper_t *wrapper =
        static_cast<monitor_callback_wrapper_t*>(user_data);

    if (!wrapper || !wrapper->user_callback) {
        return;
    }

    // Convert internal event to public API event
    slk_event_t public_event;

    // Map internal event types to public API event types
    switch (event->type) {
        case slk::EVENT_PEER_CONNECTED:
            public_event.event = SLK_EVENT_CONNECTED;
            break;
        case slk::EVENT_PEER_DISCONNECTED:
            public_event.event = SLK_EVENT_DISCONNECTED;
            break;
        case slk::EVENT_PEER_HANDSHAKE_FAILED:
            public_event.event = SLK_EVENT_HANDSHAKE_FAIL;
            break;
        default:
            return; // Unknown event type, ignore
    }

    public_event.peer_id = event->routing_id.data();
    public_event.peer_id_len = event->routing_id.size();
    public_event.endpoint = event->endpoint.empty() ? NULL : event->endpoint.c_str();
    public_event.err = event->error_code;
    public_event.timestamp = static_cast<uint64_t>(event->timestamp_us / 1000); // Convert to ms

    // Call the user's callback
    wrapper->user_callback(
        reinterpret_cast<slk_socket_t*>(socket),
        &public_event,
        wrapper->user_data
    );
}
#endif

int SL_CALL slk_socket_monitor(slk_socket_t *socket_, slk_monitor_fn callback,
                                void *user_data, int events)
{
    CHECK_PTR(socket_, -1);

#ifdef SL_ENABLE_MONITORING
    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        // Check if this is a ROUTER socket
        int socket_type = 0;
        size_t type_size = sizeof(socket_type);
        int rc = socket->getsockopt(slk::SL_TYPE, &socket_type, &type_size);
        if (rc != 0 || socket_type != SLK_ROUTER) {
            errno = ENOTSUP;
            return set_errno(SLK_EPROTO);
        }

        // Cast to router_t to access monitoring methods
        slk::router_t *router = static_cast<slk::router_t*>(socket);

        // Create wrapper to bridge callback signatures
        // Note: This wrapper is leaked intentionally as we don't have a mechanism
        // to free it when the socket is destroyed. For production use, this should
        // be stored in the router and freed in the destructor.
        monitor_callback_wrapper_t *wrapper = new monitor_callback_wrapper_t();
        wrapper->user_callback = callback;
        wrapper->user_data = user_data;

        router->set_monitor_callback(internal_monitor_callback, wrapper, events);

        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
#else
    // Monitoring not enabled at compile time
    (void)callback;
    (void)user_data;
    (void)events;
    errno = ENOTSUP;
    return set_errno(SLK_EPROTO);
#endif
}

/****************************************************************************/
/*  Router Connection Status API                                            */
/****************************************************************************/

int SL_CALL slk_is_connected(slk_socket_t *socket_, const void *routing_id, size_t id_len)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(routing_id, -1);

    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        // Use the get_peer_state method
        int state = socket->get_peer_state(routing_id, id_len);
        if (state < 0) {
            // If peer not found (EHOSTUNREACH), return 0 (not connected)
            // This is not an error condition for is_connected
            if (errno == EHOSTUNREACH) {
                return 0;
            }
            return set_errno(map_errno(errno));
        }
        return (state >= 0) ? 1 : 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

int SL_CALL slk_get_peer_stats(slk_socket_t *socket_, const void *routing_id,
                                size_t id_len, slk_peer_stats_t *stats)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(routing_id, -1);
    CHECK_PTR(stats, -1);

#ifdef SL_ENABLE_MONITORING
    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        // Check if this is a ROUTER socket
        int socket_type = 0;
        size_t type_size = sizeof(socket_type);
        int rc = socket->getsockopt(slk::SL_TYPE, &socket_type, &type_size);
        if (rc != 0 || socket_type != SLK_ROUTER) {
            errno = ENOTSUP;
            return set_errno(SLK_EPROTO);
        }

        // Cast to router_t to access monitoring methods
        slk::router_t *router = static_cast<slk::router_t*>(socket);

        // Create blob from routing_id
        slk::blob_t rid(static_cast<const unsigned char*>(routing_id), id_len);

        // Get internal statistics
        slk::peer_stats_t internal_stats;
        if (!router->get_peer_statistics(rid, &internal_stats)) {
            // Peer not found
            errno = EHOSTUNREACH;
            return set_errno(SLK_EHOSTUNREACH);
        }

        // Convert internal statistics to public API format
        stats->bytes_sent = internal_stats.bytes_sent;
        stats->bytes_received = internal_stats.bytes_recv;
        stats->msgs_sent = internal_stats.messages_sent;
        stats->msgs_received = internal_stats.messages_recv;

        // Convert connection time from microseconds to milliseconds
        stats->connected_time = internal_stats.connection_time / 1000;

        // Convert last heartbeat from microseconds to milliseconds
        stats->last_heartbeat = internal_stats.last_heartbeat_time / 1000;

        // Set is_alive based on connection state
        stats->is_alive = (internal_stats.state == slk::SLK_STATE_CONNECTED) ? 1 : 0;

        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
#else
    // Monitoring not enabled at compile time
    (void)id_len;
    errno = ENOTSUP;
    return set_errno(SLK_EPROTO);
#endif
}

int SL_CALL slk_get_peers(slk_socket_t *socket_, void **peer_ids, size_t *id_lens,
                          size_t *num_peers)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(peer_ids, -1);
    CHECK_PTR(num_peers, -1);

    // Note: id_lens can be NULL if caller only wants count

#ifdef SL_ENABLE_MONITORING
    slk::socket_base_t *socket = reinterpret_cast<slk::socket_base_t*>(socket_);

    try {
        // Check if this is a ROUTER socket
        int socket_type = 0;
        size_t type_size = sizeof(socket_type);
        int rc = socket->getsockopt(slk::SL_TYPE, &socket_type, &type_size);
        if (rc != 0 || socket_type != SLK_ROUTER) {
            errno = ENOTSUP;
            return set_errno(SLK_EPROTO);
        }

        // Cast to router_t to access monitoring methods
        slk::router_t *router = static_cast<slk::router_t*>(socket);

        // Get the list of connected peers
        std::vector<slk::blob_t> peers;
        router->get_connected_peers(&peers);

        const size_t count = peers.size();
        *num_peers = count;

        if (count == 0) {
            // No peers connected
            *peer_ids = NULL;
            if (id_lens) {
                // The API design is ambiguous - id_lens is size_t* but we need to
                // return an array. We interpret this as a pointer that will be
                // assigned to point to the allocated array.
                void *null_ptr = NULL;
                memcpy(id_lens, &null_ptr, sizeof(void*));
            }
            return 0;
        }

        // Allocate arrays for peer IDs and their lengths
        void **ids = static_cast<void**>(malloc(count * sizeof(void*)));
        size_t *lens = static_cast<size_t*>(malloc(count * sizeof(size_t)));

        if (!ids || !lens) {
            free(ids);
            free(lens);
            return set_errno(SLK_ENOMEM);
        }

        // Copy peer routing IDs
        for (size_t i = 0; i < count; i++) {
            const slk::blob_t &peer_id = peers[i];
            lens[i] = peer_id.size();

            // Allocate memory for this peer ID
            ids[i] = malloc(lens[i]);
            if (!ids[i]) {
                // Cleanup on allocation failure
                for (size_t j = 0; j < i; j++) {
                    free(ids[j]);
                }
                free(ids);
                free(lens);
                return set_errno(SLK_ENOMEM);
            }

            // Copy the routing ID data
            memcpy(ids[i], peer_id.data(), lens[i]);
        }

        *peer_ids = ids;

        // API design issue: id_lens is declared as size_t* but we need to return
        // a pointer to an allocated array. This requires treating id_lens as size_t**
        // We use memcpy to avoid strict aliasing violations.
        if (id_lens) {
            memcpy(id_lens, &lens, sizeof(size_t*));
        }

        return 0;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
#else
    // Monitoring not enabled at compile time
    *num_peers = 0;
    *peer_ids = NULL;
    if (id_lens) {
        void *null_ptr = NULL;
        memcpy(id_lens, &null_ptr, sizeof(void*));
    }
    errno = ENOTSUP;
    return set_errno(SLK_EPROTO);
#endif
}

void SL_CALL slk_free_peers(void **peer_ids, size_t *id_lens, size_t num_peers)
{
    if (!peer_ids || !id_lens) {
        return;
    }

    for (size_t i = 0; i < num_peers; i++) {
        if (peer_ids[i]) {
            free(peer_ids[i]);
        }
    }
    free(peer_ids);
    free(id_lens);
}

/****************************************************************************/
/*  Utility Functions                                                       */
/****************************************************************************/

uint64_t SL_CALL slk_clock(void)
{
    try {
        slk::clock_t clock;
        return clock.now_us();
    } catch (...) {
        return 0;
    }
}

void SL_CALL slk_sleep(int ms)
{
    if (ms <= 0) {
        return;
    }

#ifdef _WIN32
    Sleep(static_cast<DWORD>(ms));
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, nullptr);
#endif
}

} // extern "C"
