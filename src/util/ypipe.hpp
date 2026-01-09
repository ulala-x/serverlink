/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_YPIPE_HPP_INCLUDED
#define SL_YPIPE_HPP_INCLUDED

#include "atomic_ptr.hpp"
#include "yqueue.hpp"
#include "ypipe_base.hpp"

namespace slk
{
template <typename T, int N> class ypipe_t final : public ypipe_base_t<T>
{
  public:
    inline ypipe_t ()
    {
        _queue.push ();
        _r = _w = _f = &_queue.back ();
        _c.set (&_queue.back ());
    }

    inline ~ypipe_t () override {}

    inline void write (const T &value_, bool incomplete_) override
    {
        _queue.back () = value_;
        _queue.push ();

        if (!incomplete_)
            _f = &_queue.back ();
    }

    inline bool unwrite (T *value_) override
    {
        if (_f == &_queue.back ())
            return false;
        _queue.unpush ();
        *value_ = _queue.back ();
        return true;
    }

    inline bool flush () override
    {
        if (_w == _f)
            return true;

        if (_c.cas (_w, _f) != _w) {
            _c.set (_f);
            _w = _f;
            return false;
        }

        _w = _f;
        return true;
    }

    inline bool check_read () override
    {
        if (&_queue.front () != _r && _r)
            return true;

        _r = _c.cas (&_queue.front (), NULL);

        if (&_queue.front () == _r || !_r)
            return false;

        return true;
    }

    inline bool read (T *value_) override
    {
        if (!check_read ())
            return false;

        *value_ = _queue.front ();
        _queue.pop ();
        return true;
    }

    inline bool probe (bool (*fn_) (const T &)) override
    {
        bool rc = check_read ();
        slk_assert (rc);

        return (*fn_) (_queue.front ());
    }

  private:
    yqueue_t<T, N> _queue;
    T *_r;
    T *_w;
    T *_f;
    atomic_ptr_t<T> _c;

    SL_NON_COPYABLE_NOR_MOVABLE (ypipe_t)
};
}

#endif
