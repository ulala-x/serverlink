/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_IPC_ADDRESS_HPP_INCLUDED
#define SERVERLINK_IPC_ADDRESS_HPP_INCLUDED

#include "../util/config.hpp"

#if defined SL_HAVE_IPC

#include <string>
#include <sys/socket.h>
#include <sys/un.h>

namespace slk
{

class ipc_address_t
{
  public:
    ipc_address_t();
    ipc_address_t(const sockaddr *sa_, socklen_t sa_len_);
    ~ipc_address_t();

    // Set address from string path
    int resolve(const char *path_);

    // Get sockaddr for bind/connect
    const sockaddr *addr() const;
    socklen_t addrlen() const;

    // Get string representation
    int to_string(std::string &addr_) const;

  private:
    sockaddr_un _address;
    socklen_t _addrlen;
};

}  // namespace slk

#endif  // SL_HAVE_IPC
#endif  // SERVERLINK_IPC_ADDRESS_HPP_INCLUDED
