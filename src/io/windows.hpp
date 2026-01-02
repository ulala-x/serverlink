/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Windows Platform Header */

#ifndef SL_WINDOWS_HPP_INCLUDED
#define SL_WINDOWS_HPP_INCLUDED

// This file must be included before any Windows headers

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 // Windows Vista or later
#endif

// Prevent Windows headers from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Reduce the size of the Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Prevent Winsock v1 from being included
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#endif // SL_WINDOWS_HPP_INCLUDED
