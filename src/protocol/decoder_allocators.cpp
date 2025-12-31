/* SPDX-License-Identifier: MPL-2.0 */

#include "decoder_allocators.hpp"
#include "../msg/msg.hpp"
#include "../util/atomic_counter.hpp"
#include "../util/err.hpp"
#include <cstdlib>
#include <new>

namespace slk
{

shared_message_memory_allocator::shared_message_memory_allocator(
    std::size_t bufsize)
    : m_buf(nullptr),
      m_buf_size(0),
      m_max_size(bufsize),
      m_msg_content(nullptr),
      m_max_counters((m_max_size + msg_t::max_vsm_size - 1) / msg_t::max_vsm_size)
{
}

shared_message_memory_allocator::shared_message_memory_allocator(
    std::size_t bufsize,
    std::size_t max_messages)
    : m_buf(nullptr),
      m_buf_size(0),
      m_max_size(bufsize),
      m_msg_content(nullptr),
      m_max_counters(max_messages)
{
}

shared_message_memory_allocator::~shared_message_memory_allocator()
{
    deallocate();
}

unsigned char* shared_message_memory_allocator::allocate()
{
    if (m_buf) {
        // Release reference count to couple lifetime to messages
        atomic_counter_t* c = reinterpret_cast<atomic_counter_t*>(m_buf);

        // If refcnt drops to 0, there are no messages using the buffer
        // because either all messages have been closed or only vsm-messages
        // were created
        if (c->sub(1)) {
            // Buffer is still in use as message data. "Release" it and create a new one
            release();
        }
    }

    // If buf != nullptr it is not used by any message so we can re-use it for the next run
    if (!m_buf) {
        // Allocate memory for reference counter together with reception buffer
        std::size_t const allocationsize =
            m_max_size + sizeof(atomic_counter_t) +
            m_max_counters * sizeof(msg_t::content_t);

        m_buf = static_cast<unsigned char*>(std::malloc(allocationsize));
        alloc_assert(m_buf);

        new (m_buf) atomic_counter_t(1);
    } else {
        // Reuse existing buffer, reset reference count
        atomic_counter_t* c = reinterpret_cast<atomic_counter_t*>(m_buf);
        c->set(1);
    }

    m_buf_size = m_max_size;
    m_msg_content = reinterpret_cast<msg_t::content_t*>(
        m_buf + sizeof(atomic_counter_t) + m_max_size);

    return m_buf + sizeof(atomic_counter_t);
}

void shared_message_memory_allocator::deallocate()
{
    if (m_buf) {
        atomic_counter_t* c = reinterpret_cast<atomic_counter_t*>(m_buf);
        if (!c->sub(1)) {
            c->~atomic_counter_t();
            std::free(m_buf);
        }
    }
    clear();
}

unsigned char* shared_message_memory_allocator::release()
{
    unsigned char* b = m_buf;
    clear();
    return b;
}

void shared_message_memory_allocator::clear()
{
    m_buf = nullptr;
    m_buf_size = 0;
    m_msg_content = nullptr;
}

void shared_message_memory_allocator::inc_ref()
{
    (reinterpret_cast<atomic_counter_t*>(m_buf))->add(1);
}

void shared_message_memory_allocator::call_dec_ref(void* /*data*/, void* hint)
{
    slk_assert(hint);
    unsigned char* buf = static_cast<unsigned char*>(hint);
    atomic_counter_t* c = reinterpret_cast<atomic_counter_t*>(buf);

    if (!c->sub(1)) {
        c->~atomic_counter_t();
        std::free(buf);
    }
}

std::size_t shared_message_memory_allocator::size() const
{
    return m_buf_size;
}

unsigned char* shared_message_memory_allocator::data()
{
    return m_buf + sizeof(atomic_counter_t);
}

} // namespace slk
