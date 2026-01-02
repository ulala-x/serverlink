/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_ENCODER_HPP_INCLUDED
#define SL_ENCODER_HPP_INCLUDED

#include "i_encoder.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace slk
{
// Helper base class for encoders. It implements the state machine that
// fills the outgoing buffer. Derived classes should implement individual
// state machine actions.

template <typename T>
class encoder_base_t : public i_encoder
{
  public:
    explicit encoder_base_t(std::size_t bufsize)
        : m_write_pos(nullptr),
          m_to_write(0),
          m_next(nullptr),
          m_new_msg_flag(false),
          m_buf_size(bufsize),
          m_buf(static_cast<unsigned char*>(std::malloc(bufsize))),
          m_in_progress(nullptr)
    {
        alloc_assert(m_buf);
    }

    ~encoder_base_t() override
    {
        std::free(m_buf);
    }

    // The function returns a batch of binary data. The data
    // are filled to a supplied buffer. If no buffer is supplied (data
    // points to nullptr) encoder object will provide buffer of its own.
    std::size_t encode(unsigned char** data, std::size_t size) final
    {
        unsigned char* buffer = !*data ? m_buf : *data;
        const std::size_t buffersize = !*data ? m_buf_size : size;

        if (in_progress() == nullptr) {
            return 0;
        }

        std::size_t pos = 0;
        while (pos < buffersize) {
            // If there are no more data to return, run the state machine.
            // If there are still no data, return what we already have
            // in the buffer.
            if (!m_to_write) {
                if (m_new_msg_flag) {
                    int rc = m_in_progress->close();
                    errno_assert(rc == 0);
                    rc = m_in_progress->init();
                    errno_assert(rc == 0);
                    m_in_progress = nullptr;
                    break;
                }
                (static_cast<T*>(this)->*m_next)();
            }

            // If there are no data in the buffer yet and we are able to
            // fill whole buffer in a single go, let's use zero-copy.
            // There's no disadvantage to it as we cannot stuck multiple
            // messages into the buffer anyway. Note that subsequent
            // write(s) are non-blocking, thus each single write writes
            // at most SO_SNDBUF bytes at once not depending on how large
            // is the chunk returned from here.
            // As a consequence, large messages being sent won't block
            // other engines running in the same I/O thread for excessive
            // amounts of time.
            if (!pos && !*data && m_to_write >= buffersize) {
                *data = m_write_pos;
                pos = m_to_write;
                m_write_pos = nullptr;
                m_to_write = 0;
                return pos;
            }

            // Copy data to the buffer. If the buffer is full, return.
            // Use parentheses around std::min to avoid Windows min/max macro conflict
            const std::size_t to_copy = (std::min)(m_to_write, buffersize - pos);
            std::memcpy(buffer + pos, m_write_pos, to_copy);
            pos += to_copy;
            m_write_pos += to_copy;
            m_to_write -= to_copy;
        }

        *data = buffer;
        return pos;
    }

    void load_msg(msg_t* msg) final
    {
        slk_assert(in_progress() == nullptr);
        m_in_progress = msg;
        (static_cast<T*>(this)->*m_next)();
    }

  protected:
    // Prototype of state machine action.
    typedef void (T::*step_t)();

    // This function should be called from derived class to write the data
    // to the buffer and schedule next state machine action.
    void next_step(void* write_pos,
                   std::size_t to_write,
                   step_t next,
                   bool new_msg_flag)
    {
        m_write_pos = static_cast<unsigned char*>(write_pos);
        m_to_write = to_write;
        m_next = next;
        m_new_msg_flag = new_msg_flag;
    }

    msg_t* in_progress()
    {
        return m_in_progress;
    }

  private:
    // Where to get the data to write from.
    unsigned char* m_write_pos;

    // How much data to write before next step should be executed.
    std::size_t m_to_write;

    // Next step. If set to nullptr, it means that associated data stream
    // is dead.
    step_t m_next;

    bool m_new_msg_flag;

    // The buffer for encoded data.
    const std::size_t m_buf_size;
    unsigned char* const m_buf;

    msg_t* m_in_progress;

    SL_NON_COPYABLE_NOR_MOVABLE(encoder_base_t)
};

} // namespace slk

#endif
