/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_CTX_HPP_INCLUDED
#define SL_CTX_HPP_INCLUDED

#include <map>
#include <vector>
#include <string>
#include <set>
#include <stdint.h>

#include "../io/mailbox.hpp"
#include "array.hpp"
#include "../util/config.hpp"
#include "../util/mutex.hpp"
#include "options.hpp"
#include "../util/atomic_counter.hpp"
#include "../util/thread.hpp"
#include "../util/constants.hpp"

namespace slk
{
class object_t;
class io_thread_t;
class socket_base_t;
class reaper_t;
class pipe_t;

// Information associated with inproc endpoint.
struct endpoint_t
{
    socket_base_t *socket;
    options_t options;

    endpoint_t() : socket(nullptr) {}
    endpoint_t(socket_base_t* s, const options_t& o) : socket(s) {
        copy_options(o);
    }
    endpoint_t(const endpoint_t& other) : socket(other.socket) {
        copy_options(other.options);
    }
    endpoint_t& operator=(const endpoint_t& other) {
        if (this != &other) {
            socket = other.socket;
            copy_options(other.options);
        }
        return *this;
    }

  private:
    void copy_options(const options_t& o) {
        // Atomic members must be handled via load/store
        options.linger.store(o.linger.load());
        options.sndhwm = o.sndhwm;
        options.rcvhwm = o.rcvhwm;
        options.type = o.type;
        options.recv_routing_id = o.recv_routing_id;
        options.routing_id_size = o.routing_id_size;
        memcpy(options.routing_id, o.routing_id, o.routing_id_size);
        // Add other critical inproc options as needed
    }
};

class thread_ctx_t
{
  public:
    thread_ctx_t ();

    void start_thread (thread_t &thread_,
                       thread_fn *tfn_,
                       void *arg_,
                       const char *name_ = NULL) const;

    int set (int option_, const void *optval_, size_t optvallen_);
    int get (int option_, void *optval_, const size_t *optvallen_);

  protected:
    mutex_t _opt_sync;

  private:
    int _thread_priority;
    int _thread_sched_policy;
    std::set<int> _thread_affinity_cpus;
    std::string _thread_name_prefix;
};

class ctx_t : public thread_ctx_t
{
  public:
    ctx_t ();
    bool check_tag () const;
    int terminate ();
    int shutdown ();
    int set (int option_, const void *optval_, size_t optvallen_);
    int get (int option_, void *optval_, const size_t *optvallen_);
    int get (int option_);
    slk::socket_base_t *create_socket (int type_);
    void destroy_socket (slk::socket_base_t *socket_);
    void send_command (uint32_t tid_, const command_t &command_);
    slk::io_thread_t *choose_io_thread (uint64_t affinity_);
    slk::object_t *get_reaper () const;
    int register_endpoint (const char *addr_, const endpoint_t &endpoint_);
    int unregister_endpoint (const std::string &addr_,
                             const socket_base_t *socket_);
    void unregister_endpoints (const slk::socket_base_t *socket_);
    endpoint_t find_endpoint (const char *addr_);
    void pend_connection (const std::string &addr_,
                          const endpoint_t &endpoint_,
                          pipe_t **pipes_);
    void connect_pending (const char *addr_, slk::socket_base_t *bind_socket_);

    enum { term_tid = 0, reaper_tid = 1 };
    ~ctx_t ();
    bool valid () const;

  private:
    bool start ();
    struct pending_connection_t {
        endpoint_t endpoint;
        pipe_t *connect_pipe;
        pipe_t *bind_pipe;
    };

    uint32_t _tag;
    typedef array_t<socket_base_t> sockets_t;
    sockets_t _sockets;
    typedef std::vector<uint32_t> empty_slots_t;
    empty_slots_t _empty_slots;
    bool _starting;
    bool _terminating;
    mutex_t _slot_sync;
    slk::reaper_t *_reaper;
    typedef std::vector<slk::io_thread_t *> io_threads_t;
    io_threads_t _io_threads;
    std::vector<i_mailbox *> _slots;
    mailbox_t _term_mailbox;
    typedef std::map<std::string, endpoint_t> endpoints_t;
    endpoints_t _endpoints;
    typedef std::multimap<std::string, pending_connection_t> pending_connections_t;
    pending_connections_t _pending_connections;
    mutex_t _endpoints_sync;
    static atomic_counter_t max_socket_id;
    int _max_sockets;
    int _max_msgsz;
    int _io_thread_count;
    bool _blocky;
    bool _ipv6;
    bool _zero_copy;

    SL_NON_COPYABLE_NOR_MOVABLE (ctx_t)

    enum side { connect_side, bind_side };
    static void connect_inproc_sockets (slk::socket_base_t *bind_socket_,
                            const options_t &bind_options_,
                            const pending_connection_t &pending_connection_,
                            side side_);
};
}

#endif