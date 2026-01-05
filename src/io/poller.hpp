/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_POLLER_HPP_INCLUDED
#define SERVERLINK_POLLER_HPP_INCLUDED

#include "../util/config.hpp"

// Force Asio if requested
#if defined SL_USE_ASIO
  #undef SL_USE_WEPOLL
  #undef SL_USE_EPOLL
  #undef SL_USE_KQUEUE
  #undef SL_USE_SELECT
  #include "asio/poller.hpp"
#elif defined SL_USE_WEPOLL
  #include "wepoll.hpp"
#elif defined SL_USE_EPOLL
  #include "epoll.hpp"
#elif defined SL_USE_KQUEUE
  #include "kqueue.hpp"
#elif defined SL_USE_SELECT
  #include "select.hpp"
#else
  #error "No I/O multiplexing mechanism available"
#endif

// Define polling mechanism for signaler wait function
#if defined SL_USE_EPOLL || defined SL_USE_KQUEUE
  #define SL_POLL_BASED_ON_POLL
#elif defined SL_USE_SELECT || defined SL_USE_WEPOLL || defined SL_USE_ASIO
  #define SL_POLL_BASED_ON_SELECT
#endif

#endif
