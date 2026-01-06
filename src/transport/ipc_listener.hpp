/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_IPC_LISTENER_HPP_INCLUDED
#define SERVERLINK_IPC_LISTENER_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_HAVE_IPC

#include <asio.hpp>
#include "../io/fd.hpp"
#include "stream_listener_base.hpp"
#include "ipc_address.hpp"

namespace slk
{

class io_thread_t;
class socket_base_t;

class ipc_listener_t final : public stream_listener_base_t
{
  public:
    ipc_listener_t(slk::io_thread_t *io_thread_,
                   slk::socket_base_t *socket_,
                   const options_t &options_);
    ~ipc_listener_t() override;

    // Set address to listen on
    int set_local_address(const char *addr_);

  private:
    //  Start the accept loop.
    void start_accept();

    //  Handler for a new connection.
    void handle_accept(const asio::error_code& ec, asio::local::stream_protocol::socket socket);

    // Close the listening socket and unlink the socket file
    void close() override;

    //  Asio acceptor for incoming connections.
    asio::local::stream_protocol::acceptor _acceptor;

    // Address to listen on
    ipc_address_t _address;

    // Path of the socket file
    std::string _filename;

    // Did we create the socket file?
    bool _has_file;

    // Lifetime sentinel for async handlers.
    std::shared_ptr<int> _lifetime_sentinel;

    SL_NON_COPYABLE_NOR_MOVABLE(ipc_listener_t)
};

}  // namespace slk

#endif  // SL_HAVE_IPC
#endif  // SERVERLINK_IPC_LISTENER_HPP_INCLUDED