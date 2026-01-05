/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_STREAM_LISTENER_BASE_HPP_INCLUDED
#define SERVERLINK_STREAM_LISTENER_BASE_HPP_INCLUDED

#include <string>
#include <memory>

#include "../core/own.hpp"
#include "../util/constants.hpp"
#include "../io/i_async_stream.hpp"

namespace slk
{
class io_thread_t;
class socket_base_t;
class stream_engine_base_t;

class stream_listener_base_t : public own_t
{
  public:
    stream_listener_base_t (slk::io_thread_t *io_thread_,
                            slk::socket_base_t *socket_,
                            const options_t &options_);
    ~stream_listener_base_t () override;

    // Get the bound address for use with wildcards
    int get_local_address (std::string &addr_) const;

  protected:
    // This method is now responsible for creating the engine with an async stream
    void create_engine (std::unique_ptr<i_async_stream> stream);

    // Socket the listener belongs to.
    slk::socket_base_t *_socket;
    
    // IO thread context
    slk::io_thread_t *_io_thread;

    // Common options
    const options_t _options;

    // String representation of endpoint to bind to
    std::string _endpoint;

  private:
    //  Handlers for incoming commands.
    void process_plug () final;
    void process_term (int linger_) final;

  protected:
    //  Close the listening socket.
    virtual void close () = 0;


    SL_NON_COPYABLE_NOR_MOVABLE (stream_listener_base_t)
};
}

#endif
