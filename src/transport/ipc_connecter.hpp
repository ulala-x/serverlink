/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_IPC_CONNECTER_HPP_INCLUDED
#define SERVERLINK_IPC_CONNECTER_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_HAVE_IPC

#include <asio.hpp>
#include "../io/fd.hpp"
#include "../util/constants.hpp"
#include "stream_connecter_base.hpp"
#include "ipc_address.hpp"

namespace slk
{

class io_thread_t;
class session_base_t;

class ipc_connecter_t final : public stream_connecter_base_t
{
  public:
    // If 'delayed_start' is true connecter first waits for a while,
    // then starts connection process.
    ipc_connecter_t(slk::io_thread_t *io_thread_,
                    slk::session_base_t *session_,
                    const options_t &options_,
                    address_t *addr_,
                    bool delayed_start_);
    ~ipc_connecter_t() override;

  protected:
    // Overrides from stream_connecter_base_t
    void start_connecting() override;
    void close() override;

  private:
    // Handlers for async operations
    void handle_connect(const asio::error_code& ec);

    // Asio members
    asio::local::stream_protocol::socket _socket;
    std::string _path;

    SL_NON_COPYABLE_NOR_MOVABLE(ipc_connecter_t)
};

}  // namespace slk

#endif  // SL_HAVE_IPC
#endif  // SERVERLINK_IPC_CONNECTER_HPP_INCLUDED