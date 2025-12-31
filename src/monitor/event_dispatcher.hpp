/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Monitoring System */

#ifndef SL_EVENT_DISPATCHER_HPP_INCLUDED
#define SL_EVENT_DISPATCHER_HPP_INCLUDED

#include <vector>
#include "peer_stats.hpp"
#include "../msg/blob.hpp"
#include "../util/mutex.hpp"
#include "../util/macros.hpp"

namespace slk
{
// Forward declaration
class socket_base_t;

// Event data structure
struct event_data_t
{
    event_type_t type;
    blob_t routing_id;
    std::string endpoint;
    int error_code;
    int64_t timestamp_us;

    event_data_t (event_type_t type_, const blob_t &id_, int64_t ts)
        : type (type_), routing_id (), endpoint (), error_code (0),
          timestamp_us (ts)
    {
        routing_id.set_deep_copy (id_);
    }

    event_data_t (event_type_t type_, const blob_t &id_,
                  const std::string &endpoint_, int64_t ts)
        : type (type_), routing_id (), endpoint (endpoint_),
          error_code (0), timestamp_us (ts)
    {
        routing_id.set_deep_copy (id_);
    }

    event_data_t (event_type_t type_, const blob_t &id_, int err, int64_t ts)
        : type (type_), routing_id (), endpoint (), error_code (err),
          timestamp_us (ts)
    {
        routing_id.set_deep_copy (id_);
    }
};

// Callback function type for monitoring events
// Parameters: socket, event_data, user_data
typedef void (*monitor_callback_fn) (socket_base_t *socket,
                                      const event_data_t *event,
                                      void *user_data);

// Event dispatcher - manages callbacks and event delivery
class event_dispatcher_t
{
  public:
    event_dispatcher_t ();
    ~event_dispatcher_t ();

    // Callback registration
    void register_callback (monitor_callback_fn callback, void *user_data,
                            int event_mask);
    void unregister_callback ();

    // Event dispatching
    void dispatch_event (socket_base_t *socket, const event_data_t &event);

    // Check if monitoring is enabled
    bool is_enabled () const;

    // Get current event mask
    int get_event_mask () const;

  private:
    struct callback_info_t
    {
        monitor_callback_fn callback;
        void *user_data;
        int event_mask;

        callback_info_t ()
            : callback (NULL), user_data (NULL), event_mask (0)
        {
        }

        callback_info_t (monitor_callback_fn cb, void *ud, int mask)
            : callback (cb), user_data (ud), event_mask (mask)
        {
        }
    };

    callback_info_t _callback;
    mutable mutex_t _mutex;

    // Check if event type is in mask
    bool is_event_enabled (event_type_t type) const;

    SL_NON_COPYABLE_NOR_MOVABLE (event_dispatcher_t)
};

}

#endif
