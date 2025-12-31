/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_IPC_CONNECTER_HPP_INCLUDED
#define SERVERLINK_IPC_CONNECTER_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_HAVE_IPC

#include "../io/fd.hpp"
#include "../util/constants.hpp"
#include "stream_connecter_base.hpp"
#include "ipc_address.hpp"

namespace slk
{

class io_thread_t;
class session_base_t;

class ipc_connecter_t SL_FINAL : public stream_connecter_base_t
{
  public:
    // If 'delayed_start' is true connecter first waits for a while,
    // then starts connection process.
    ipc_connecter_t(slk::io_thread_t *io_thread_,
                    slk::session_base_t *session_,
                    const options_t &options_,
                    address_t *addr_,
                    bool delayed_start_);
    ~ipc_connecter_t() SL_OVERRIDE;

  private:
    // Handlers for I/O events
    void out_event() SL_OVERRIDE;

    // Internal function to start the actual connection establishment
    void start_connecting() SL_OVERRIDE;

    // Get socket name for logging
    std::string get_socket_name(fd_t fd_, socket_end_t socket_end_) const;

    // Open IPC connecting socket. Returns -1 in case of error,
    // 0 if connect was successful immediately. Returns -1 with
    // EINPROGRESS errno if async connect was launched.
    int open();

    // Get the file descriptor of newly created connection. Returns
    // retired_fd if the connection was unsuccessful.
    fd_t connect();

    // Address to connect to
    ipc_address_t _address;

    SL_NON_COPYABLE_NOR_MOVABLE(ipc_connecter_t)
};

}  // namespace slk

#endif  // SL_HAVE_IPC
#endif  // SERVERLINK_IPC_CONNECTER_HPP_INCLUDED
