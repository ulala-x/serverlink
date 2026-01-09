/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_I_ASYNC_STREAM_HPP_INCLUDED
#define SERVERLINK_I_ASYNC_STREAM_HPP_INCLUDED

#include <functional>
#include <cstddef>

namespace slk
{
    // I/O 추상화 인터페이스
    // TCP, inproc, WebSocket 등 모든 전송 계층의 공통 인터페이스
    class i_async_stream
    {
    public:
        using read_handler = std::function<void(size_t bytes_transferred, int error)>;
        using write_handler = std::function<void(size_t bytes_transferred, int error)>;

        virtual ~i_async_stream() = default;

        // 비동기 읽기
        // buf: 읽은 데이터를 저장할 버퍼
        // len: 버퍼 크기
        // handler: 완료 시 호출될 콜백 (bytes_transferred, error)
        virtual void async_read(void* buf, size_t len, read_handler handler) = 0;

        // 비동기 쓰기
        // buf: 전송할 데이터 버퍼
        // len: 데이터 크기
        // handler: 완료 시 호출될 콜백 (bytes_transferred, error)
        virtual void async_write(const void* buf, size_t len, write_handler handler) = 0;

        // 스트림 닫기
        virtual void async_writev(const asio::const_buffer* buffers, std::size_t count, write_handler handler) = 0;
        virtual void close() = 0;
    };
}

#endif
