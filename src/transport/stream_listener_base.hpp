/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_STREAM_LISTENER_BASE_HPP_INCLUDED
#define SERVERLINK_STREAM_LISTENER_BASE_HPP_INCLUDED

#include <string>

#include "../io/fd.hpp"
#include "../core/own.hpp"
#include "../util/constants.hpp"
#include "../io/io_object.hpp"
#include "address.hpp"

namespace slk
{
class io_thread_t;
class socket_base_t;

class stream_listener_base_t : public own_t, public io_object_t
{
  public:
    stream_listener_base_t (slk::io_thread_t *io_thread_,
                            slk::socket_base_t *socket_,
                            const options_t &options_);
    ~stream_listener_base_t () SL_OVERRIDE;

    // Get the bound address for use with wildcards
    int get_local_address (std::string &addr_) const;

  protected:
    virtual std::string get_socket_name (fd_t fd_,
                                         socket_end_t socket_end_) const = 0;

  private:
    //  Handlers for incoming commands.
    void process_plug () SL_FINAL;
    void process_term (int linger_) SL_FINAL;

  protected:
    //  Close the listening socket.
    virtual int close ();

    virtual void create_engine (fd_t fd);

    //  Underlying socket.
    fd_t _s;

    //  Handle corresponding to the listening socket.
    handle_t _handle;

    //  Socket the listener belongs to.
    slk::socket_base_t *_socket;

    // String representation of endpoint to bind to
    std::string _endpoint;

    SL_NON_COPYABLE_NOR_MOVABLE (stream_listener_base_t)
};
}

#endif
