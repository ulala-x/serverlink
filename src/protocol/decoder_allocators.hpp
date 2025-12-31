/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_DECODER_ALLOCATORS_HPP_INCLUDED
#define SL_DECODER_ALLOCATORS_HPP_INCLUDED

#include "../util/macros.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"
#include "../util/atomic_counter.hpp"
#include <cstddef>
#include <cstdlib>

namespace slk
{
// Static buffer policy for decoders.
// Allocates a single fixed-size buffer on construction.
class c_single_allocator
{
  public:
    explicit c_single_allocator(std::size_t bufsize)
        : m_buf_size(bufsize),
          m_buf(static_cast<unsigned char*>(std::malloc(bufsize)))
    {
        alloc_assert(m_buf);
    }

    ~c_single_allocator()
    {
        std::free(m_buf);
    }

    unsigned char* allocate()
    {
        return m_buf;
    }

    void deallocate()
    {
        // No-op for single allocator
    }

    std::size_t size() const
    {
        return m_buf_size;
    }

    // This buffer is fixed, size must not be changed
    void resize(std::size_t /*new_size*/)
    {
        // No-op
    }

  private:
    std::size_t m_buf_size;
    unsigned char* m_buf;

    SL_NON_COPYABLE_NOR_MOVABLE(c_single_allocator)
};

// Reference-counted buffer allocator for zero-copy message decoding.
// This allocator allocates a reference counted buffer which is used by v2_decoder_t
// to use zero-copy msg::init to create messages with memory from this buffer as
// data storage.
//
// The buffer is allocated with a reference count of 1 to ensure it stays alive while
// decoding messages. The buffer may be allocated longer than necessary because it is
// only deleted when allocate is called the next time.
class shared_message_memory_allocator
{
  public:
    explicit shared_message_memory_allocator(std::size_t bufsize);

    // Create an allocator for a maximum number of messages
    shared_message_memory_allocator(std::size_t bufsize,
                                     std::size_t max_messages);

    ~shared_message_memory_allocator();

    // Allocate a new buffer.
    // This releases the current buffer to be bound to the lifetime of the messages
    // created on this buffer.
    unsigned char* allocate();

    // Force deallocation of buffer.
    void deallocate();

    // Give up ownership of the buffer. The buffer's lifetime is now coupled to
    // the messages constructed on top of it.
    unsigned char* release();

    void inc_ref();

    static void call_dec_ref(void* data, void* hint);

    std::size_t size() const;

    // Return pointer to the first message data byte.
    unsigned char* data();

    // Return pointer to the first byte of the buffer.
    unsigned char* buffer()
    {
        return m_buf;
    }

    void resize(std::size_t new_size)
    {
        m_buf_size = new_size;
    }

    msg_t::content_t* provide_content()
    {
        return m_msg_content;
    }

    void advance_content()
    {
        m_msg_content++;
    }

  private:
    void clear();

    unsigned char* m_buf;
    std::size_t m_buf_size;
    const std::size_t m_max_size;
    msg_t::content_t* m_msg_content;
    std::size_t m_max_counters;

    SL_NON_COPYABLE_NOR_MOVABLE(shared_message_memory_allocator)
};

} // namespace slk

#endif
