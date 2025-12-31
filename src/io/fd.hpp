/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_FD_HPP_INCLUDED
#define SERVERLINK_FD_HPP_INCLUDED

#if defined _WIN32
#include <winsock2.h>
#endif

namespace slk
{
#ifdef _WIN32
typedef SOCKET fd_t;
#else
typedef int fd_t;
#endif

#ifdef _WIN32
enum : fd_t
{
    retired_fd = INVALID_SOCKET
};
#else
enum
{
    retired_fd = -1
};
#endif
}

#endif
