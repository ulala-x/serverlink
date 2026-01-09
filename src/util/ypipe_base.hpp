/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_YPIPE_BASE_HPP_INCLUDED
#define SL_YPIPE_BASE_HPP_INCLUDED

namespace slk
{
    template <typename T>
    class ypipe_base_t
    {
    public:
        virtual ~ypipe_base_t() = default;
        virtual void write(const T& value, bool incomplete) = 0;
        virtual bool unwrite(T* value) = 0;
        virtual bool flush() = 0;
        virtual bool check_read() = 0;
        virtual bool read(T* value) = 0;
        virtual bool probe(bool (*fn)(const T&)) = 0;
        
        // Add rollback for libzmq parity
        virtual void rollback() = 0;
    };
}

#endif