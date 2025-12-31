/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"
#include "ipc_address.hpp"

#if defined SL_HAVE_IPC

#include <string>
#include <cstring>
#include <climits>

#include "../util/err.hpp"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

slk::ipc_address_t::ipc_address_t()
{
    memset(&_address, 0, sizeof(_address));
    _addrlen = 0;
}

slk::ipc_address_t::ipc_address_t(const sockaddr *sa_, socklen_t sa_len_)
{
    slk_assert(sa_ && sa_->sa_family == AF_UNIX);
    slk_assert(sa_len_ <= static_cast<socklen_t>(sizeof(_address)));

    memcpy(&_address, sa_, sa_len_);
    _addrlen = sa_len_;
}

slk::ipc_address_t::~ipc_address_t()
{
}

int slk::ipc_address_t::resolve(const char *path_)
{
    slk_assert(path_);

    memset(&_address, 0, sizeof(_address));
    _address.sun_family = AF_UNIX;

    const size_t path_len = strlen(path_);

    // Check if path is too long
    // Note: sun_path size includes null terminator
    if (path_len >= sizeof(_address.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    // Copy the path
    memcpy(_address.sun_path, path_, path_len);
    _address.sun_path[path_len] = '\0';

    // Calculate address length
    // offsetof gives us the offset to sun_path, then add path length + 1 for null terminator
    _addrlen = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path_len + 1);

    return 0;
}

const sockaddr *slk::ipc_address_t::addr() const
{
    return reinterpret_cast<const sockaddr *>(&_address);
}

socklen_t slk::ipc_address_t::addrlen() const
{
    return _addrlen;
}

int slk::ipc_address_t::to_string(std::string &addr_) const
{
    if (_address.sun_family != AF_UNIX) {
        addr_.clear();
        return -1;
    }

    // Return the path
    addr_ = std::string(_address.sun_path);
    return 0;
}

#endif  // SL_HAVE_IPC
