/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_PRECOMPILED_HPP_INCLUDED
#define SERVERLINK_PRECOMPILED_HPP_INCLUDED

//  On AIX platform, poll.h has to be included first to get consistent
//  definition of pollfd structure (AIX uses 'reqevents' and 'retnevents'
//  instead of 'events' and 'revents' and defines macros to map from POSIX-y
//  names to AIX-specific names).
#if defined SL_POLL_BASED_ON_POLL && defined SL_HAVE_AIX
#include <poll.h>
#endif

#include "util/config.hpp"

#define __STDC_LIMIT_MACROS

#if defined SL_HAVE_OPENBSD
#define ucred sockpeercred
#endif

// TODO: expand pch implementation to non-windows builds.
#ifdef _MSC_VER

// Windows headers must be included first and in the correct order
// winsock2.h must come before windows.h and any other Windows SDK headers
#include "io/windows.hpp"

// Windows networking headers (after winsock2.h)
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <mswsock.h>

// Other Windows SDK headers (after winsock2.h)
#include <ipexport.h>
#include <iphlpapi.h>

// standard C headers
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <io.h>
#include <limits.h>
#include <process.h>
#include <rpc.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

// standard C++ headers
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#if _MSC_VER >= 1800
#include <inttypes.h>
#endif

#if _MSC_VER >= 1700
#include <atomic>
#endif

#if defined _WIN32_WCE
#include <cmnintrin.h>
#else
#include <intrin.h>
#endif

#include "core/options.hpp"

#endif // _MSC_VER

#endif // SERVERLINK_PRECOMPILED_HPP_INCLUDED
