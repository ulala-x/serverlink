/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - 1:1 Parity with v2_decoder.cpp */

#include "v2_decoder.hpp"
#include "v2_protocol.hpp"
#include "wire.hpp"
#include "../util/err.hpp"
#include "../util/likely.hpp"
#include <cstdlib>
#include <cstring>
#include <cerrno>

namespace slk {

v2_decoder_t::v2_decoder_t (size_t bufsize_, int64_t maxmsgsize_, bool zero_copy_) :
    decoder_base_t<v2_decoder_t, shared_message_memory_allocator> (bufsize_),
    m_msg_flags (0), m_zero_copy (zero_copy_), m_max_msg_size (maxmsgsize_)
{
    int rc = m_in_progress.init ();
    errno_assert (rc == 0);
    next_step (m_tmpbuf, 1, &v2_decoder_t::flags_ready);
}

v2_decoder_t::~v2_decoder_t () {
    int rc = m_in_progress.close ();
    errno_assert (rc == 0);
}

int v2_decoder_t::flags_ready (unsigned char const *) {
    m_msg_flags = 0;
    if (m_tmpbuf[0] & v2_protocol_t::more_flag) m_msg_flags |= msg_t::more;
    if (m_tmpbuf[0] & v2_protocol_t::command_flag) m_msg_flags |= msg_t::command;

    if (m_tmpbuf[0] & v2_protocol_t::large_flag)
        next_step (m_tmpbuf, 8, &v2_decoder_t::eight_byte_size_ready);
    else
        next_step (m_tmpbuf, 1, &v2_decoder_t::one_byte_size_ready);
    return 0;
}

int v2_decoder_t::one_byte_size_ready (unsigned char const *read_from_) {
    return size_ready (m_tmpbuf[0], read_from_);
}

int v2_decoder_t::eight_byte_size_ready (unsigned char const *read_from_) {
    const uint64_t msg_size = get_uint64 (m_tmpbuf);
    return size_ready (msg_size, read_from_);
}

int v2_decoder_t::size_ready (uint64_t msg_size_, unsigned char const *read_pos_) {
    if (m_max_msg_size >= 0 && unlikely (msg_size_ > (uint64_t) m_max_msg_size)) {
        errno = EMSGSIZE; return -1;
    }
    if (unlikely (msg_size_ != (size_t) msg_size_)) { errno = EMSGSIZE; return -1; }

    int rc = m_in_progress.close ();
    slk_assert (rc == 0);

    shared_message_memory_allocator &allocator = get_allocator ();
    if (unlikely (!m_zero_copy || msg_size_ > (size_t) (allocator.data () + allocator.size () - read_pos_))) {
        rc = m_in_progress.init_size ((size_t) msg_size_);
    } else {
        rc = m_in_progress.init_external_storage (allocator.content (), (void *) read_pos_, (size_t) msg_size_, shared_message_memory_allocator::call_free, &allocator);
    }

    if (unlikely (rc != 0)) { errno = ENOMEM; return -1; }
    m_in_progress.set_flags (m_msg_flags);
    next_step (m_in_progress.data (), m_in_progress.size (), &v2_decoder_t::message_ready);
    return 0;
}

int v2_decoder_t::message_ready (unsigned char const *) {
    next_step (m_tmpbuf, 1, &v2_decoder_t::flags_ready);
    return 1;
}

} // namespace slk