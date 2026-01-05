/* SPDX-License-Identifier: MPL-2.0 */

#include "../../precompiled.hpp"
#include "poller.hpp"
#include "../i_poll_events.hpp"
#include <cstdio>
#include <chrono>
#include <thread>

#if defined SL_USE_ASIO

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif

slk::asio_poller_t::asio_poller_t(ctx_t* ctx_)
    : worker_poller_base_t(ctx_),
      _work_guard(asio::make_work_guard(_io_context.get_executor()))
{
}

slk::asio_poller_t::~asio_poller_t()
{
}

slk::asio_poller_t::handle_t slk::asio_poller_t::add_fd(fd_t fd_, i_poll_events* events_)
{
    adjust_load (1);
    
    handle_t handle = reinterpret_cast<handle_t>(static_cast<uintptr_t>(fd_));
    
    fd_entry_t entry;
    entry.fd = fd_;
    entry.sink = events_;
    entry.polling = false;
    entry.socket = nullptr; 
    
    {
        std::lock_guard<std::mutex> lock(_entries_mutex);
        _entries[handle] = entry;
    }
    return handle;
}

void slk::asio_poller_t::rm_fd(handle_t handle_)
{
    std::lock_guard<std::mutex> lock(_entries_mutex);
    auto it = _entries.find(handle_);
    if (it != _entries.end()) {
        _entries.erase(it);
        adjust_load (-1);
    }
}

void slk::asio_poller_t::set_pollin(handle_t handle_)
{
    std::lock_guard<std::mutex> lock(_entries_mutex);
    auto it = _entries.find(handle_);
    if (it != _entries.end()) {
        it->second.polling = true;
    }
}

void slk::asio_poller_t::reset_pollin(handle_t handle_)
{
    std::lock_guard<std::mutex> lock(_entries_mutex);
    auto it = _entries.find(handle_);
    if (it != _entries.end()) {
        it->second.polling = false;
    }
}

void slk::asio_poller_t::set_pollout(handle_t handle_)
{
}

void slk::asio_poller_t::reset_pollout(handle_t handle_)
{
}

void slk::asio_poller_t::stop()
{
    _stopping = true;
    _io_context.stop();
}

int slk::asio_poller_t::max_fds()
{
    return -1;
}

void slk::asio_poller_t::loop()
{
    while (!_stopping) {
        // 1. Process Asio events (wait up to 10ms if no work)
        _io_context.run_one_for(std::chrono::milliseconds(10));
        
        // 2. Poll traditional FDs (mailbox signals)
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;
        bool has_fds = false;

        {
            std::lock_guard<std::mutex> lock(_entries_mutex);
            for (auto const& [handle, entry] : _entries) {
                if (entry.polling) {
                    FD_SET(entry.fd, &readfds);
#ifndef _WIN32
                    if ((int)entry.fd > max_fd) max_fd = (int)entry.fd;
#endif
                    has_fds = true;
                }
            }
        }

        if (has_fds) {
            timeval tv = {0, 0}; // Non-blocking check since run_one_for already waited
#ifdef _WIN32
            int rc = select(0, &readfds, NULL, NULL, &tv);
#else
            int rc = select(max_fd + 1, &readfds, NULL, NULL, &tv);
#endif
            if (rc > 0) {
                // To avoid calling in_event while holding the lock, 
                // we collect targets first.
                std::vector<i_poll_events*> targets;
                {
                    std::lock_guard<std::mutex> lock(_entries_mutex);
                    for (auto& [handle, entry] : _entries) {
                        if (entry.polling && FD_ISSET(entry.fd, &readfds)) {
                            targets.push_back(entry.sink);
                        }
                    }
                }
                for (auto sink : targets) {
                    sink->in_event();
                }
            }
        }
        
        // 3. Process ServerLink timers
        execute_timers();
        
        // 4. If io_context was stopped, restart it for the next iteration if not stopping
        if (_io_context.stopped() && !_stopping) {
            _io_context.restart();
        }
    }
}

void slk::asio_poller_t::start_polling(handle_t handle)
{
}

#endif