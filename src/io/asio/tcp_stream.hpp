/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_TCP_STREAM_HPP_INCLUDED
#define SERVERLINK_TCP_STREAM_HPP_INCLUDED

#include "../../util/config.hpp"

#ifdef SL_USE_ASIO
#include "../i_async_stream.hpp"
#include "asio_context.hpp"
#include <asio.hpp>

namespace slk
{
    // Asio 기반 TCP 스트림 구현
    class tcp_stream_t : public i_async_stream
    {
    public:
        // 기존 소켓 파일 디스크립터로 생성
        inline tcp_stream_t(fd_t fd)
            : _socket(asio_context_t::instance().get_context(), asio::ip::tcp::v4(), fd)
        {
        }

        // 소켓 생성자 (Asio 소켓)
        inline tcp_stream_t(asio::ip::tcp::socket socket)
            : _socket(std::move(socket))
        {
        }

        inline ~tcp_stream_t() override
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

        inline void close() override
        {
            asio::error_code ec;
            _socket.close(ec);
        }

        // 소켓 반환 (테스트용)
        inline asio::ip::tcp::socket& get_socket()
        {
            return _socket;
        }

    private:
        asio::ip::tcp::socket _socket;
    };
}

#endif // SL_USE_ASIO
#endif
