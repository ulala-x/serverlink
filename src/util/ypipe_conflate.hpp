/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_YPIPE_CONFLATE_HPP_INCLUDED
#define SL_YPIPE_CONFLATE_HPP_INCLUDED

#include "dbuffer.hpp"
#include "ypipe_base.hpp"
#include "macros.hpp"

namespace slk
{
template <typename T> class ypipe_conflate_t final : public ypipe_base_t<T>
{
  public:
    ypipe_conflate_t () : reader_awake (false) {}

    void write (const T &value_, bool incomplete_) override
    {
        (void) incomplete_;
        dbuffer.write (value_);
    }

    bool unwrite (T *) override
    {
        return false;
    }

    bool flush () override
    {
        return reader_awake;
    }

    bool check_read () override
    {
        const bool res = dbuffer.check_read ();
        if (!res)
            reader_awake = false;
        return res;
    }

    bool read (T *value_) override
    {
        if (!check_read ())
            return false;
        return dbuffer.read (value_);
    }

    bool probe (bool (*fn_) (const T &)) override
    {
        return dbuffer.probe (fn_);
    }

  protected:
    dbuffer_t<T> dbuffer;
    bool reader_awake;

    SL_NON_COPYABLE_NOR_MOVABLE (ypipe_conflate_t)
};
}

#endif
