/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_IPC_LISTENER_HPP_INCLUDED
#define SERVERLINK_IPC_LISTENER_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_HAVE_IPC

#include "../io/fd.hpp"
#include "stream_listener_base.hpp"
#include "ipc_address.hpp"

namespace slk
{

class io_thread_t;
class socket_base_t;

class ipc_listener_t SL_FINAL : public stream_listener_base_t
{
  public:
    ipc_listener_t(slk::io_thread_t *io_thread_,
                   slk::socket_base_t *socket_,
                   const options_t &options_);
    ~ipc_listener_t() SL_OVERRIDE;

    // Set address to listen on
    int set_local_address(const char *addr_);

  protected:
    std::string get_socket_name(fd_t fd_, socket_end_t socket_end_) const SL_OVERRIDE;

  private:
    // Handlers for I/O events
    void in_event() SL_OVERRIDE;

    // Accept the new connection. Returns the file descriptor of the
    // newly created connection. The function may return retired_fd
    // if the connection was dropped while waiting in the listen backlog.
    fd_t accept();

    // Close the listening socket and unlink the socket file
    int close() SL_OVERRIDE;

    // Address to listen on
    ipc_address_t _address;

    // Path of the socket file
    std::string _filename;

    // Did we create the socket file?
    bool _has_file;

    SL_NON_COPYABLE_NOR_MOVABLE(ipc_listener_t)
};

}  // namespace slk

#endif  // SL_HAVE_IPC
#endif  // SERVERLINK_IPC_LISTENER_HPP_INCLUDED
