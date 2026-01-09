/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_ASIO_IPC_STREAM_HPP_INCLUDED
#define SERVERLINK_ASIO_IPC_STREAM_HPP_INCLUDED

#include "../../util/config.hpp"

#ifdef SL_USE_ASIO
#include "../i_async_stream.hpp"
#include <asio.hpp>

#if defined(ASIO_HAS_LOCAL_SOCKETS)

namespace slk
{
    // Asio 기반 IPC (Unix Domain Socket) 스트림 구현
    class ipc_stream_t : public i_async_stream
    {
    public:
        // 소켓 생성자
        inline ipc_stream_t(asio::local::stream_protocol::socket socket)
            : _socket(std::move(socket))
        {
        }

        inline ~ipc_stream_t() override
        {
            close();
        }

        // i_async_stream 구현
        inline void async_read(void* buf, size_t len, read_handler handler) override
        {
            _socket.async_read_some(
                asio::buffer(buf, len),
                [handler](const asio::error_code& ec, size_t bytes_transferred) {
                    handler(bytes_transferred, ec.value());
                }
            );
        }

        inline void async_write(const void* buf, size_t len, write_handler handler) override
        {
            asio::async_write(
                _socket,
                asio::buffer(buf, len),
                [handler](const asio::error_code& ec, size_t bytes_transferred) {
                    handler(bytes_transferred, ec.value());
                }
            );
        }

        inline void async_writev(const asio::const_buffer* buffers, std::size_t count, write_handler handler) override { asio::async_write(_socket, std::span<const asio::const_buffer>(buffers, count), [handler](const asio::error_code& ec, std::size_t bt) { handler(bt, ec.value()); }); }
        inline void close() override
        {
            asio::error_code ec;
            _socket.close(ec);
        }

    private:
        asio::local::stream_protocol::socket _socket;
    };
}

#endif // ASIO_HAS_LOCAL_SOCKETS
#endif // SL_USE_ASIO
#endif
