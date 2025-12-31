/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_DECODER_HPP_INCLUDED
#define SL_DECODER_HPP_INCLUDED

#include "decoder_allocators.hpp"
#include "i_decoder.hpp"
#include "../util/err.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cstdint>

namespace slk
{
// Helper base class for decoders that know the amount of data to read
// in advance at any moment. Knowing the amount in advance is a property
// of the protocol used. ZMTP framing protocol is based on size-prefixed
// paradigm, which qualifies it to be parsed by this class.
//
// This class implements the state machine that parses the incoming buffer.
// Derived class should implement individual state machine actions.
//
// Buffer management is done by an allocator policy.
template <typename T, typename A = c_single_allocator>
class decoder_base_t : public i_decoder
{
  public:
    explicit decoder_base_t(std::size_t buf_size)
        : m_next(nullptr),
          m_read_pos(nullptr),
          m_to_read(0),
          m_allocator(buf_size)
    {
        m_buf = m_allocator.allocate();
    }

    ~decoder_base_t() override
    {
        m_allocator.deallocate();
    }

    // Returns a buffer to be filled with binary data.
    void get_buffer(unsigned char** data, std::size_t* size) final
    {
        m_buf = m_allocator.allocate();

        // If we are expected to read large message, we'll opt for zero-
        // copy, i.e. we'll ask caller to fill the data directly to the
        // message. Note that subsequent read(s) are non-blocking, thus
        // each single read reads at most SO_RCVBUF bytes at once not
        // depending on how large is the chunk returned from here.
        // As a consequence, large messages being received won't block
        // other engines running in the same I/O thread for excessive
        // amounts of time.
        if (m_to_read >= m_allocator.size()) {
            *data = m_read_pos;
            *size = m_to_read;
            return;
        }

        *data = m_buf;
        *size = m_allocator.size();
    }

    // Processes the data in the buffer previously allocated using
    // get_buffer function. size argument specifies number of bytes
    // actually filled into the buffer. Function returns 1 when the
    // whole message was decoded or 0 when more data is required.
    // On error, -1 is returned and errno set accordingly.
    // Number of bytes processed is returned in bytes_used.
    int decode(const unsigned char* data,
               std::size_t size,
               std::size_t& bytes_used) final
    {
        bytes_used = 0;

        // In case of zero-copy simply adjust the pointers, no copying
        // is required. Also, run the state machine in case all the data
        // were processed.
        if (data == m_read_pos) {
            slk_assert(size <= m_to_read);
            m_read_pos += size;
            m_to_read -= size;
            bytes_used = size;

            while (!m_to_read) {
                const int rc =
                    (static_cast<T*>(this)->*m_next)(data + bytes_used);
                if (rc != 0) {
                    return rc;
                }
            }
            return 0;
        }

        while (bytes_used < size) {
            // Copy the data from buffer to the message.
            const std::size_t to_copy = std::min(m_to_read, size - bytes_used);

            // Only copy when destination address is different from the
            // current address in the buffer.
            if (m_read_pos != data + bytes_used) {
                std::memcpy(m_read_pos, data + bytes_used, to_copy);
            }

            m_read_pos += to_copy;
            m_to_read -= to_copy;
            bytes_used += to_copy;

            // Try to get more space in the message to fill in.
            // If none is available, return.
            while (m_to_read == 0) {
                // Pass current address in the buffer
                const int rc =
                    (static_cast<T*>(this)->*m_next)(data + bytes_used);
                if (rc != 0) {
                    return rc;
                }
            }
        }

        return 0;
    }

    void resize_buffer(std::size_t new_size) final
    {
        m_allocator.resize(new_size);
    }

  protected:
    // Prototype of state machine action. Action should return:
    //   0 if successful
    //   1 if a message is ready
    //  -1 on error
    typedef int (T::*step_t)(const unsigned char*);

    // This function should be called from derived class to read data
    // from the buffer and schedule next state machine action.
    void next_step(void* read_pos, std::size_t to_read, step_t next)
    {
        m_read_pos = static_cast<unsigned char*>(read_pos);
        m_to_read = to_read;
        m_next = next;
    }

    A& get_allocator()
    {
        return m_allocator;
    }

  private:
    // Next step. If set to nullptr, it means that associated data stream
    // is dead. Note that there can be still data in the process in such case.
    step_t m_next;

    // Where to store the read data.
    unsigned char* m_read_pos;

    // How much data to read before taking next step.
    std::size_t m_to_read;

    // The buffer for data to decode.
    A m_allocator;
    unsigned char* m_buf;

    SL_NON_COPYABLE_NOR_MOVABLE(decoder_base_t)
};

} // namespace slk

#endif
