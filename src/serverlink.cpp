/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Public C API Implementation */

#include "serverlink/serverlink.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "core/ctx.hpp"
#include "core/socket_base.hpp"
#include "core/router.hpp"
#include "msg/msg.hpp"
#include "util/err.hpp"
#include "util/clock.hpp"

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
        ctx->terminate();
        delete ctx;
    } catch (...) {
        // Best effort cleanup
        delete ctx;
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

    // This is a simplified polling implementation
    // A full implementation would integrate with the I/O subsystem
    try {
        // TODO: Implement proper polling with timeout support
        (void)timeout; // Unused for now

        // TODO: Implement proper polling using the socket's has_in/has_out methods
        // For now, just check immediate availability
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

        return ready_count;
    } catch (...) {
        return set_errno(SLK_EPROTO);
    }
}

/****************************************************************************/
/*  Monitoring API                                                          */
/****************************************************************************/

int SL_CALL slk_socket_monitor(slk_socket_t *socket_, slk_monitor_fn callback,
                                void *user_data, int events)
{
    CHECK_PTR(socket_, -1);

    // Suppress unused parameter warnings
    (void)callback;
    (void)user_data;
    (void)events;

    // TODO: Implement monitoring integration with connection_manager and event_dispatcher
    // This requires extending the socket_base interface
    return set_errno(SLK_EPROTO);
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
            return set_errno(map_errno(errno));
        }
        return (state > 0) ? 1 : 0;
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

    // Suppress unused parameter warning
    (void)id_len;

    // TODO: Implement peer statistics retrieval
    // This requires integration with connection_manager
    return set_errno(SLK_EPROTO);
}

int SL_CALL slk_get_peers(slk_socket_t *socket_, void **peer_ids, size_t *id_lens,
                          size_t *num_peers)
{
    CHECK_PTR(socket_, -1);
    CHECK_PTR(peer_ids, -1);
    CHECK_PTR(id_lens, -1);
    CHECK_PTR(num_peers, -1);

    // TODO: Implement peer list retrieval
    // This requires integration with connection_manager
    return set_errno(SLK_EPROTO);
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
