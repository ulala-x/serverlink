/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_POLLER_HPP_INCLUDED
#define SERVERLINK_POLLER_HPP_INCLUDED

#include "../util/config.hpp"

// Select the appropriate poller implementation based on platform
#if defined SL_USE_EPOLL
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
#elif defined SL_USE_SELECT
#define SL_POLL_BASED_ON_SELECT
#endif

#endif
