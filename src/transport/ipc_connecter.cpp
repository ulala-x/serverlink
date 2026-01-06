/* SPDX-License-Identifier: MPL-2.0 */

#include "precompiled.hpp"

#if defined SL_HAVE_IPC

#include <new>
#include <string>
#include <memory>

#include "ipc_connecter.hpp"
#include "../io/asio/ipc_stream.hpp"
#include "../io/io_thread.hpp"
#include "../util/config.hpp"
#include "../util/err.hpp"
#include "../core/session_base.hpp"
#include "address.hpp"

slk::ipc_connecter_t::ipc_connecter_t(io_thread_t *io_thread_,
                                       session_base_t *session_,
                                       const options_t &options_,
                                       address_t *addr_,
                                       bool delayed_start_) :
    stream_connecter_base_t(io_thread_, session_, options_, addr_, delayed_start_),
    _socket(io_thread_->get_io_context())
{
    _path = _addr->address;
}

slk::ipc_connecter_t::~ipc_connecter_t()
{
}

void slk::ipc_connecter_t::close()
{
    if (_socket.is_open()) {
        asio::error_code ec;
        _socket.close(ec);
    }
}

void slk::ipc_connecter_t::start_connecting()
{
    asio::local::stream_protocol::endpoint endpoint(_path);
    
    std::weak_ptr<int> sentinel = _lifetime_sentinel;
    _socket.async_connect(endpoint,
        [this, sentinel](const asio::error_code& ec) {
            if (sentinel.expired()) return;
            handle_connect(ec);
        });
}

void slk::ipc_connecter_t::handle_connect(const asio::error_code& ec)
{
    if (ec) {
        if (ec != asio::error::operation_aborted) {
            close();
            add_reconnect_timer();
        }
        return;
    }

    // Create stream and engine
    auto stream = std::make_unique<ipc_stream_t>(std::move(_socket));
    
    create_engine(std::move(stream), _endpoint);
}

#endif  // SL_HAVE_IPC