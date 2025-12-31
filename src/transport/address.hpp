/* SPDX-License-Identifier: MPL-2.0 */

#ifndef __SL_ADDRESS_HPP_INCLUDED__
#define __SL_ADDRESS_HPP_INCLUDED__

#include "../io/fd.hpp"

#include <string>

#ifndef SL_HAVE_WINDOWS
#include <sys/socket.h>
#else
#include <ws2tcpip.h>
#endif

namespace slk
{
class ctx_t;
class tcp_address_t;
#if defined SL_HAVE_IPC
class ipc_address_t;
#endif
class inproc_address_t;

namespace protocol_name
{
static const char tcp[] = "tcp";
#if defined SL_HAVE_IPC
static const char ipc[] = "ipc";
#endif
static const char inproc[] = "inproc";
}

struct address_t
{
    address_t (const std::string &protocol_,
               const std::string &address_,
               ctx_t *parent_);

    ~address_t ();

    const std::string protocol;
    const std::string address;
    ctx_t *const parent;

    //  Protocol specific resolved address
    //  All members must be pointers to allow for consistent initialization
    union
    {
        void *dummy;
        tcp_address_t *tcp_addr;
#if defined SL_HAVE_IPC
        ipc_address_t *ipc_addr;
#endif
        inproc_address_t *inproc_addr;
    } resolved;

    int to_string (std::string &addr_) const;
};

#if defined(SL_HAVE_HPUX) || defined(SL_HAVE_VXWORKS)                        \
  || defined(SL_HAVE_WINDOWS)
typedef int slk_socklen_t;
#else
typedef socklen_t slk_socklen_t;
#endif

enum socket_end_t
{
    socket_end_local,
    socket_end_remote
};

slk_socklen_t
get_socket_address (fd_t fd_, socket_end_t socket_end_, sockaddr_storage *ss_);

template <typename T>
std::string get_socket_name (fd_t fd_, socket_end_t socket_end_)
{
    struct sockaddr_storage ss;
    const slk_socklen_t sl = get_socket_address (fd_, socket_end_, &ss);
    if (sl == 0) {
        return std::string ();
    }

    const T addr (reinterpret_cast<struct sockaddr *> (&ss), sl);
    std::string address_string;
    addr.to_string (address_string);
    return address_string;
}
}

#endif
