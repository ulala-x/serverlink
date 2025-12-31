/* SPDX-License-Identifier: MPL-2.0 */

#include "raw_decoder.hpp"
#include "../util/err.hpp"
#include <cstdlib>
#include <cstring>

namespace slk
{

raw_decoder_t::raw_decoder_t(std::size_t bufsize)
    : m_allocator(bufsize, 1)
{
    const int rc = m_in_progress.init();
    errno_assert(rc == 0);
}

raw_decoder_t::~raw_decoder_t()
{
    const int rc = m_in_progress.close();
    errno_assert(rc == 0);
}

void raw_decoder_t::get_buffer(unsigned char** data, std::size_t* size)
{
    *data = m_allocator.allocate();
    *size = m_allocator.size();
}

int raw_decoder_t::decode(const unsigned char* data,
                          std::size_t size,
                          std::size_t& bytes_used)
{
    // For raw mode, the entire buffer becomes a message
    const int rc = m_in_progress.init(const_cast<unsigned char*>(data),
                                      size,
                                      shared_message_memory_allocator::call_dec_ref,
                                      m_allocator.buffer(),
                                      m_allocator.provide_content());

    // If the buffer serves as memory for a zero-copy message, release it
    // and allocate a new buffer in get_buffer for the next decode
    if (m_in_progress.is_zcmsg()) {
        m_allocator.advance_content();
        m_allocator.release();
    }

    errno_assert(rc != -1);
    bytes_used = size;
    return 1; // Always returns 1 message
}

} // namespace slk
