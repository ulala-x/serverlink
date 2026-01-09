/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_YPIPE_HPP_INCLUDED
#define SL_YPIPE_HPP_INCLUDED

#include "atomic_ptr.hpp"
#include "yqueue.hpp"
#include "ypipe_base.hpp"
#include <serverlink/config.h>

namespace slk {

template <typename T, int N>
class ypipe_t final : public ypipe_base_t<T> {
  public:
    ypipe_t() {
        _queue.push();
        _r = _w = _f = &_queue.back();
        _c.set(&_queue.back());
    }

    void write(const T &value, bool incomplete) override {
        _queue.back() = value;
        _queue.push();
        if (!incomplete) _f = &_queue.back();
    }

    bool unwrite(T *value) override {
        if (_f == &_queue.back()) return false;
        _queue.unpush();
        *value = _queue.back();
        return true;
    }

    bool flush() override {
        if (_w == _f) return true;
        if (_c.cas(_w, _f) != _w) {
            _c.set(_f); _w = _f; return false;
        }
        _w = _f; return true;
    }

    bool check_read() override {
        if (&_queue.front() != _r && _r) return true;
        _r = _c.cas(&_queue.front(), nullptr);
        if (&_queue.front() == _r || !_r) return false;
        return true;
    }

    bool read(T *value) override {
        if (!check_read()) return false;
        *value = _queue.front();
        _queue.pop();
        return true;
    }

    bool probe(bool (*fn)(const T &)) override {
        if (!check_read()) return false;
        return fn(_queue.front());
    }

    void rollback() override {
        while (_f != &_queue.back()) _queue.unpush();
    }

  private:
    yqueue_t<T, N> _queue;
    T *_r, *_w, *_f;
    atomic_ptr_t<T> _c;

    SL_NON_COPYABLE_NOR_MOVABLE (ypipe_t)
};
}

#endif