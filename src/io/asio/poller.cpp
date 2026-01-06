/* SPDX-License-Identifier: MPL-2.0 */

#include "../../precompiled.hpp"
#include "poller.hpp"
#include "../i_poll_events.hpp"
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>

#if defined SL_USE_ASIO

#ifndef _WIN32
#include <asio/posix/stream_descriptor.hpp>
#endif

slk::asio_poller_t::asio_poller_t(ctx_t* ctx_)
    : worker_poller_base_t(ctx_),
      _work_guard(asio::make_work_guard(_io_context.get_executor())),
      _lifetime_sentinel(std::make_shared<int>(0))
{
}

slk::asio_poller_t::~asio_poller_t()
{
    stop_worker();
    
    // Explicitly release all native handles to avoid double-close
    std::lock_guard<std::mutex> lock(_entries_mutex);
    for (auto& pair : _entries) {
        if (pair.second.socket) {
            asio::error_code ec;
            pair.second.socket->cancel(ec);
#ifdef _WIN32
            pair.second.socket->release(ec);
#else
            (void)pair.second.socket->release();
#endif
        }
    }
    _entries.clear();
}

slk::asio_poller_t::handle_t slk::asio_poller_t::add_fd(fd_t fd_, i_poll_events* events_)
{
    adjust_load (1);
    
    handle_t handle = reinterpret_cast<handle_t>(static_cast<uintptr_t>(fd_));
    
    fd_entry_t entry;
    entry.fd = fd_;
    entry.sink = events_;
    entry.pollin = false;
    entry.pollout = false;
    entry.reading = false;
    entry.writing = false;
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
        if (it->second.socket) {
            asio::error_code ec;
            it->second.socket->cancel(ec);
#ifdef _WIN32
            it->second.socket->release(ec);
#else
            (void)it->second.socket->release();
#endif
        }
        _entries.erase(it);
        adjust_load (-1);
    }
}

void slk::asio_poller_t::set_pollin(handle_t handle_)
{
    {
        std::lock_guard<std::mutex> lock(_entries_mutex);
        auto it = _entries.find(handle_);
        if (it != _entries.end()) {
            it->second.pollin = true;
        }
    }
    start_polling(handle_);
}

void slk::asio_poller_t::reset_pollin(handle_t handle_)
{
    std::lock_guard<std::mutex> lock(_entries_mutex);
    auto it = _entries.find(handle_);
    if (it != _entries.end()) {
        it->second.pollin = false;
    }
}

void slk::asio_poller_t::set_pollout(handle_t handle_)
{
    {
        std::lock_guard<std::mutex> lock(_entries_mutex);
        auto it = _entries.find(handle_);
        if (it != _entries.end()) {
            it->second.pollout = true;
        }
    }
    start_polling(handle_);
}

void slk::asio_poller_t::reset_pollout(handle_t handle_)
{
    std::lock_guard<std::mutex> lock(_entries_mutex);
    auto it = _entries.find(handle_);
    if (it != _entries.end()) {
        it->second.pollout = false;
    }
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
        // 1. Execute ServerLink timers
        uint64_t wait_ms = execute_timers();
        if (wait_ms == 0) wait_ms = 10;

        // 2. Process Asio events
        if (_io_context.stopped()) _io_context.restart();
        _io_context.run_one_for(std::chrono::milliseconds(wait_ms));
    }
}

void slk::asio_poller_t::start_polling(handle_t handle)
{
    std::unique_lock<std::mutex> lock(_entries_mutex);
    auto it = _entries.find(handle);
    if (it == _entries.end()) return;

    auto& entry = it->second;

    if (!entry.socket) {
        entry.socket = std::make_shared<native_socket_t>(_io_context);
        asio::error_code ec;
#ifdef _WIN32
        entry.socket->assign(asio::ip::tcp::v4(), entry.fd, ec);
#else
        entry.socket->assign(entry.fd, ec); 
#endif
        if (ec) {
            entry.socket.reset();
            return;
        }
    }

    if (entry.pollin && !entry.reading) {
        entry.reading = true;
        std::weak_ptr<int> sentinel = _lifetime_sentinel;
        entry.socket->async_wait(native_socket_t::wait_read,
            [this, handle, sentinel](const asio::error_code& ec) {
                if (sentinel.expired()) return;
                
                i_poll_events* sink = nullptr;
                {
                    std::lock_guard<std::mutex> lock(_entries_mutex);
                    if (_entries.count(handle)) {
                        _entries[handle].reading = false;
                        if (!ec && _entries[handle].pollin) sink = _entries[handle].sink;
                    }
                }
                
                if (sink) {
                    sink->in_event();
                    // Re-arm
                    start_polling(handle);
                }
            });
    }

    if (entry.pollout && !entry.writing) {
        entry.writing = true;
        std::weak_ptr<int> sentinel = _lifetime_sentinel;
        entry.socket->async_wait(native_socket_t::wait_write,
            [this, handle, sentinel](const asio::error_code& ec) {
                if (sentinel.expired()) return;
                
                i_poll_events* sink = nullptr;
                {
                    std::lock_guard<std::mutex> lock(_entries_mutex);
                    if (_entries.count(handle)) {
                        _entries[handle].writing = false;
                        if (!ec && _entries[handle].pollout) sink = _entries[handle].sink;
                    }
                }
                
                if (sink) {
                    sink->out_event();
                    // Re-arm
                    start_polling(handle);
                }
            });
    }
}

#endif
