/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_ASIO_POLLER_HPP_INCLUDED
#define SERVERLINK_ASIO_POLLER_HPP_INCLUDED

#include "../../util/config.hpp"

#if defined SL_USE_ASIO

#include <asio.hpp>
#include <map>
#include <memory>
#include <mutex>
#include "../poller_base.hpp"
#include "../fd.hpp"

namespace slk
{
    class ctx_t;

    class asio_poller_t final : public worker_poller_base_t
    {
    public:
        typedef void* handle_t;

        asio_poller_t(class ctx_t* ctx_);
        ~asio_poller_t() override;

        handle_t add_fd(fd_t fd_, i_poll_events* events_);
        void rm_fd(handle_t handle_);
        void set_pollin(handle_t handle_);
        void reset_pollin(handle_t handle_);
        void set_pollout(handle_t handle_);
        void reset_pollout(handle_t handle_);
        void stop();

        static int max_fds();

        // Accessor for the underlying io_context
        asio::io_context& get_context() { return _io_context; }

    private:
        void loop() override;
        void start_polling(handle_t handle);

        asio::io_context _io_context;
        asio::executor_work_guard<asio::io_context::executor_type> _work_guard;
        std::shared_ptr<int> _lifetime_sentinel;

#ifdef _WIN32
        typedef asio::ip::tcp::socket native_socket_t;
#else
        typedef asio::posix::stream_descriptor native_socket_t;
#endif

        struct fd_entry_t {
            fd_t fd;
            std::shared_ptr<native_socket_t> socket;
            i_poll_events* sink;
            bool pollin;
            bool pollout;
            bool reading;
            bool writing;
        };
        std::map<handle_t, fd_entry_t> _entries;
        std::mutex _entries_mutex;
    };

    typedef asio_poller_t poller_t;
}

#endif // SL_USE_ASIO
#endif
