/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Monitoring System */

#include "../precompiled.hpp"
#include "event_dispatcher.hpp"

slk::event_dispatcher_t::event_dispatcher_t () : _callback (), _mutex ()
{
}

slk::event_dispatcher_t::~event_dispatcher_t ()
{
}

void slk::event_dispatcher_t::register_callback (monitor_callback_fn callback,
                                                  void *user_data,
                                                  int event_mask)
{
    scoped_lock_t lock (_mutex);
    _callback = callback_info_t (callback, user_data, event_mask);
}

void slk::event_dispatcher_t::unregister_callback ()
{
    scoped_lock_t lock (_mutex);
    _callback = callback_info_t ();
}

void slk::event_dispatcher_t::dispatch_event (socket_base_t *socket,
                                               const event_data_t &event)
{
    scoped_lock_t lock (_mutex);

    // Check if callback is registered and event is enabled
    if (_callback.callback != NULL && is_event_enabled (event.type)) {
        // Invoke callback with event data
        _callback.callback (socket, &event, _callback.user_data);
    }
}

bool slk::event_dispatcher_t::is_enabled () const
{
    scoped_lock_t lock (_mutex);
    return _callback.callback != NULL;
}

int slk::event_dispatcher_t::get_event_mask () const
{
    scoped_lock_t lock (_mutex);
    return _callback.event_mask;
}

bool slk::event_dispatcher_t::is_event_enabled (event_type_t type) const
{
    // Note: Assumes lock is already held
    if (_callback.event_mask == 0xFFFF) {
        // All events enabled
        return true;
    }

    // Check if specific event bit is set
    return (_callback.event_mask & (1 << type)) != 0;
}
