/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_YPIPE_CONFLATE_HPP_INCLUDED
#define SL_YPIPE_CONFLATE_HPP_INCLUDED

#include "dbuffer.hpp"
#include "ypipe_base.hpp"

namespace slk
{
    template <typename T>
    class ypipe_conflate_t final : public ypipe_base_t<T>
    {
    public:
        ypipe_conflate_t () = default;
        ~ypipe_conflate_t () override = default;

        void write (const T &value_, bool incomplete_) override {
            (void)incomplete_;
            _dbuffer.write (value_);
        }

        bool unwrite (T *) override { return false; }
        bool flush () override { return true; }
        bool check_read () override { return _dbuffer.check_read (); }
        bool read (T *value_) override { return _dbuffer.read (value_); }
        bool probe (bool (*)(const T &)) override { return false; }
        
        // Matches libzmq parity
        void rollback() override {}

    private:
        dbuffer_t<T> _dbuffer;
    };
}

#endif