/* SPDX-License-Identifier: MPL-2.0 */

#include "v2_decoder.hpp"
#include "v2_protocol.hpp"
#include "wire.hpp"
#include "../util/err.hpp"
#include "../util/likely.hpp"
#include <cstdlib>
#include <cstring>
#include <cerrno>

namespace slk
{

v2_decoder_t::v2_decoder_t(std::size_t bufsize,
                           int64_t maxmsgsize,
                           bool zero_copy)
    : decoder_base_t<v2_decoder_t, shared_message_memory_allocator>(bufsize),
      m_msg_flags(0),
      m_zero_copy(zero_copy),
      m_max_msg_size(maxmsgsize)
{
    int rc = m_in_progress.init();
    errno_assert(rc == 0);

    // At the beginning, read one byte and go to flags_ready state.
    next_step(m_tmpbuf, 1, &v2_decoder_t::flags_ready);
}

v2_decoder_t::~v2_decoder_t()
{
    const int rc = m_in_progress.close();
    errno_assert(rc == 0);
}

int v2_decoder_t::flags_ready(const unsigned char* /*data*/)
{
    m_msg_flags = 0;
    if (m_tmpbuf[0] & v2_protocol_t::more_flag) {
        m_msg_flags |= msg_t::more;
    }
    if (m_tmpbuf[0] & v2_protocol_t::command_flag) {
        m_msg_flags |= msg_t::command;
    }

    // The payload length is either one or eight bytes,
    // depending on whether the 'large' bit is set.
    if (m_tmpbuf[0] & v2_protocol_t::large_flag) {
        next_step(m_tmpbuf, 8, &v2_decoder_t::eight_byte_size_ready);
    } else {
        next_step(m_tmpbuf, 1, &v2_decoder_t::one_byte_size_ready);
    }

    return 0;
}

int v2_decoder_t::one_byte_size_ready(const unsigned char* read_from)
{
    return size_ready(m_tmpbuf[0], read_from);
}

int v2_decoder_t::eight_byte_size_ready(const unsigned char* read_from)
{
    // The payload size is encoded as 64-bit unsigned integer.
    // The most significant byte comes first.
    const uint64_t msg_size = get_uint64(m_tmpbuf);

    return size_ready(msg_size, read_from);
}

int v2_decoder_t::size_ready(uint64_t msg_size, const unsigned char* read_pos)
{
    // Message size must not exceed the maximum allowed size.
    if (m_max_msg_size >= 0) {
        if (unlikely(msg_size > static_cast<uint64_t>(m_max_msg_size))) {
            errno = EMSGSIZE;
            return -1;
        }
    }

    // Message size must fit into size_t data type.
    if (unlikely(msg_size != static_cast<std::size_t>(msg_size))) {
        errno = EMSGSIZE;
        return -1;
    }

    int rc = m_in_progress.close();
    slk_assert(rc == 0);

    // The current message can exceed the current buffer. We have to copy the buffer
    // data into a new message and complete it in the next receive.

    shared_message_memory_allocator& allocator = get_allocator();
    if (unlikely(!m_zero_copy ||
                 msg_size > static_cast<std::size_t>(
                     allocator.data() + allocator.size() - read_pos))) {
        // A new message has started, but the size would exceed the pre-allocated arena
        // this happens every time when a message does not fit completely into the buffer
        rc = m_in_progress.init_size(static_cast<std::size_t>(msg_size));
    } else {
        // Construct message using n bytes from the buffer as storage
        // increase buffer ref count
        // if the message will be a large message, pass a valid refcnt memory location as well
        rc = m_in_progress.init(const_cast<unsigned char*>(read_pos),
                                static_cast<std::size_t>(msg_size),
                                shared_message_memory_allocator::call_dec_ref,
                                allocator.buffer(),
                                allocator.provide_content());

        // For small messages, data has been copied and refcount does not have to be increased
        if (m_in_progress.is_zcmsg()) {
            allocator.advance_content();
            allocator.inc_ref();
        }
    }

    if (unlikely(rc != 0)) {
        errno_assert(errno == ENOMEM);
        rc = m_in_progress.init();
        errno_assert(rc == 0);
        errno = ENOMEM;
        return -1;
    }

    m_in_progress.set_flags(m_msg_flags);

    // This sets read_pos to
    // the message data address if the data needs to be copied
    // for small message / messages exceeding the current buffer
    // or
    // to the current start address in the buffer because the message
    // was constructed to use n bytes from the address passed as argument
    next_step(m_in_progress.data(),
              m_in_progress.size(),
              &v2_decoder_t::message_ready);

    return 0;
}

int v2_decoder_t::message_ready(const unsigned char* /*data*/)
{
    // Message is completely read. Signal this to the caller
    // and prepare to decode next message.
    next_step(m_tmpbuf, 1, &v2_decoder_t::flags_ready);
    return 1;
}

} // namespace slk
