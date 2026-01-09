/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_IPC_STREAM_HPP_INCLUDED
#define SERVERLINK_IPC_STREAM_HPP_INCLUDED

#include "../../util/config.hpp"

#ifdef SL_USE_ASIO
#include "../fd.hpp"
#include "../i_async_stream.hpp"
#include "asio_context.hpp"
#include <asio.hpp>
#include <vector>

namespace slk
{
    class ipc_stream_t : public i_async_stream
    {
    public:
        inline ipc_stream_t(fd_t fd)
            : _socket(asio_context_t::instance().get_context(), asio::local::stream_protocol(), fd)
        {
        }

        inline ipc_stream_t(asio::local::stream_protocol::socket socket)
            : _socket(std::move(socket))
        {
        }

        inline ~ipc_stream_t() override { close(); }

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
        asio::local::stream_protocol::socket _socket;
    };
}

#endif // SL_USE_ASIO
#endif
