/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_TCP_STREAM_HPP_INCLUDED
#define SERVERLINK_TCP_STREAM_HPP_INCLUDED

#include "../../util/config.hpp"

#ifdef SL_USE_ASIO
#include "../fd.hpp"
#include "../i_async_stream.hpp"
#include "asio_context.hpp"
#include <asio.hpp>
#include <vector>

namespace slk
{
    class tcp_stream_t : public i_async_stream
    {
    public:
        inline tcp_stream_t(fd_t fd)
            : _socket(asio_context_t::instance().get_context(), asio::ip::tcp::v4(), fd)
        {
            asio::error_code ec;
            _socket.set_option(asio::ip::tcp::no_delay(true), ec);
            // Optimization: libzmq usually relies on OS defaults, but we ensure high speed
            _socket.set_option(asio::socket_base::send_buffer_size(128 * 1024), ec);
            _socket.set_option(asio::socket_base::receive_buffer_size(128 * 1024), ec);
        }

        inline tcp_stream_t(asio::ip::tcp::socket socket)
            : _socket(std::move(socket))
        {
            asio::error_code ec;
            _socket.set_option(asio::ip::tcp::no_delay(true), ec);
            _socket.set_option(asio::socket_base::send_buffer_size(128 * 1024), ec);
            _socket.set_option(asio::socket_base::receive_buffer_size(128 * 1024), ec);
        }

        inline ~tcp_stream_t() override { close(); }

        inline void async_read(void* buf, size_t len, read_handler handler) override {
            _socket.async_read_some(asio::buffer(buf, len), [handler](const asio::error_code& ec, size_t bt) {
                handler(bt, ec.value());
            });
        }

        inline void async_write(const void* buf, size_t len, write_handler handler) override {
            asio::async_write(_socket, asio::buffer(buf, len), [handler](const asio::error_code& ec, size_t bt) {
                handler(bt, ec.value());
            });
        }

        inline void async_writev(const std::vector<asio::const_buffer>& buffers, write_handler handler) override {
            asio::async_write(_socket, buffers, [handler](const asio::error_code& ec, size_t bt) {
                handler(bt, ec.value());
            });
        }

        inline void close() override { asio::error_code ec; _socket.close(ec); }
    private:
        asio::ip::tcp::socket _socket;
    };
}

#endif // SL_USE_ASIO
#endif
