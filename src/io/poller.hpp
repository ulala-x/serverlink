/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_POLLER_HPP_INCLUDED
#define SERVERLINK_POLLER_HPP_INCLUDED

#include "../util/config.hpp"

// ServerLink now exclusively uses Boost.Asio for its I/O backend.
// Legacy pollers (epoll, kqueue, select, wepoll) have been removed.

#if defined SL_USE_ASIO
  #include "asio/poller.hpp"
#else
  #error "Boost.Asio is required for ServerLink I/O"
#endif

// Define polling mechanism for signaler wait function
// All platforms now use SELECT-style or POLL-style timeout logic for signaller wait
#if defined _WIN32
  #define SL_POLL_BASED_ON_SELECT
#else
  #define SL_POLL_BASED_ON_POLL
#endif

#endif