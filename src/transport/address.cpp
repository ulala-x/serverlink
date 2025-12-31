/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"
#include "../util/macros.hpp"
#include "address.hpp"
#include "../core/ctx.hpp"
#include "../util/err.hpp"
#include "tcp_address.hpp"
#if defined SL_HAVE_IPC
#include "ipc_address.hpp"
#endif
#include "inproc_address.hpp"

#include <string>
#include <sstream>

slk::address_t::address_t (const std::string &protocol_,
                           const std::string &address_,
                           ctx_t *parent_) :
    protocol (protocol_), address (address_), parent (parent_)
{
    resolved.dummy = NULL;
}

slk::address_t::~address_t ()
{
    if (protocol == protocol_name::tcp) {
        SL_DELETE (resolved.tcp_addr);
    }
#if defined SL_HAVE_IPC
    else if (protocol == protocol_name::ipc) {
        SL_DELETE (resolved.ipc_addr);
    }
#endif
    else if (protocol == protocol_name::inproc) {
        SL_DELETE (resolved.inproc_addr);
    }
}

int slk::address_t::to_string (std::string &addr_) const
{
    if (protocol == protocol_name::tcp && resolved.tcp_addr)
        return resolved.tcp_addr->to_string (addr_);
#if defined SL_HAVE_IPC
    if (protocol == protocol_name::ipc && resolved.ipc_addr)
        return resolved.ipc_addr->to_string (addr_);
#endif
    if (protocol == protocol_name::inproc && resolved.inproc_addr)
        return resolved.inproc_addr->to_string (addr_);

    if (!protocol.empty () && !address.empty ()) {
        std::stringstream s;
        s << protocol << "://" << address;
        addr_ = s.str ();
        return 0;
    }
    addr_.clear ();
    return -1;
}

slk::slk_socklen_t slk::get_socket_address (fd_t fd_,
                                            socket_end_t socket_end_,
                                            sockaddr_storage *ss_)
{
    slk_socklen_t sl = static_cast<slk_socklen_t> (sizeof (*ss_));

    const int rc =
      socket_end_ == socket_end_local
        ? getsockname (fd_, reinterpret_cast<struct sockaddr *> (ss_), &sl)
        : getpeername (fd_, reinterpret_cast<struct sockaddr *> (ss_), &sl);

    return rc != 0 ? 0 : sl;
}
