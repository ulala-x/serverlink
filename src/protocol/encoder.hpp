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

    ~encoder_base_t() override { std::free(m_buf); }

    void load_msg(msg_t* msg_) final {
        slk_assert(m_in_progress == nullptr);
        m_in_progress = msg_;
        (static_cast<T*>(this)->*m_next)();
    }

    // libzmq parity: Efficient encoding with zero-copy fast path
    std::size_t encode(unsigned char** data, std::size_t size) final {
        unsigned char* buffer = !*data ? m_buf : *data;
        const std::size_t buffersize = !*data ? m_buf_size : size;

        if (m_in_progress == nullptr) return 0;

        std::size_t pos = 0;
        while (pos < buffersize) {
            // If current step is finished, advance state machine
            if (!m_to_write) {
                if (m_new_msg_flag) {
                    m_in_progress = nullptr;
                    break;
                }
                (static_cast<T*>(this)->*m_next)();
            }

            // Zero-copy fast path: If remaining data is large, return it directly
            // libzmq does this to avoid copying large payloads into the internal batch buffer
            if (pos == 0 && !*data && m_to_write >= buffersize) {
                *data = m_write_pos;
                pos = m_to_write;
                m_write_pos = nullptr;
                m_to_write = 0;
                return pos;
            }

            // Small copy path
            const std::size_t to_copy = (std::min)(m_to_write, buffersize - pos);
            std::memcpy(buffer + pos, m_write_pos, to_copy);
            pos += to_copy;
            m_write_pos += to_copy;
            m_to_write -= to_copy;
        }

        *data = buffer;
        return pos;
    }

  protected:
    void next_step(void* write_pos, std::size_t to_write, void (T::*next)(), bool new_msg_flag) {
        m_write_pos = static_cast<unsigned char*>(write_pos);
        m_to_write = to_write;
        m_next = next;
        m_new_msg_flag = new_msg_flag;
    }

    msg_t* in_progress() { return m_in_progress; }

  private:
    unsigned char* m_write_pos;
    std::size_t m_to_write;
    void (T::*m_next)();
    bool m_new_msg_flag;
    const std::size_t m_buf_size;
    unsigned char* const m_buf;
    msg_t* m_in_progress;

    SL_NON_COPYABLE_NOR_MOVABLE(encoder_base_t)
};
}

#endif