/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_I_ASYNC_STREAM_HPP_INCLUDED
#define SERVERLINK_I_ASYNC_STREAM_HPP_INCLUDED

#include <stddef.h>
#include <functional>
#include <vector>
#include <asio.hpp>

namespace slk
{
    using read_handler = std::function<void(size_t, int)>;
    using write_handler = std::function<void(size_t, int)>;

    class i_async_stream
    {
    public:
        virtual ~i_async_stream() = default;
        virtual void async_read(void* buf, size_t len, read_handler handler) = 0;
        virtual void async_write(const void* buf, size_t len, write_handler handler) = 0;
        
        // Use standard ASIO buffer sequence for zero-overhead vector I/O
        virtual void async_writev(const std::vector<asio::const_buffer>& buffers, write_handler handler) = 0;

        virtual void close() = 0;
    };
}

#endif
