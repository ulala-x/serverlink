/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_IO_OBJECT_HPP_INCLUDED
#define SL_IO_OBJECT_HPP_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include "fd.hpp"
#include "i_poll_events.hpp"

namespace slk
{
class io_thread_t;

// Simple base class for objects that live in I/O threads.
class io_object_t : public i_poll_events
{
  public:
    io_object_t (slk::io_thread_t *io_thread_ = NULL);
    virtual ~io_object_t ();

    void plug (slk::io_thread_t *io_thread_);
    void unplug ();

  protected:
    // Handle type is simplified to void* to avoid circular dependencies
    typedef void* handle_t;

    // Methods to access underlying poller object
    handle_t add_fd (fd_t fd_);
    void rm_fd (handle_t handle_);
    void set_pollin (handle_t handle_);
    void reset_pollin (handle_t handle_);
    void set_pollout (handle_t handle_);
    void reset_pollout (handle_t handle_);
    void add_timer (int timeout_, int id_);
    void cancel_timer (int id_);

    // i_poll_events interface implementation
    void in_event () override;
    void out_event () override;
    void timer_event (int id_) override;

  private:
    // Poller is stored as a void pointer to decouple from specific poller types
    void *_poller;

    SL_NON_COPYABLE_NOR_MOVABLE (io_object_t)
};
}

#endif
