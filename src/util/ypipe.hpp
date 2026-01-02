/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_YPIPE_HPP_INCLUDED
#define SL_YPIPE_HPP_INCLUDED

#include "atomic_ptr.hpp"
#include "yqueue.hpp"
#include "ypipe_base.hpp"
#include <serverlink/config.h>

#if SL_HAVE_CONCEPTS
#include "concepts.hpp"
#endif

namespace slk {

// Lock-free queue implementation.
// Only a single thread can read from the pipe at any specific moment.
// Only a single thread can write to the pipe at any specific moment.
// T is the type of the object in the queue (must satisfy YPipeable concept).
// N is granularity of the pipe, i.e. how many items are needed to
// perform next memory allocation.

#if SL_HAVE_CONCEPTS
template <YPipeable T, int N>
#else
template <typename T, int N>
#endif
class ypipe_t final : public ypipe_base_t<T> {
  public:
    // Initialises the pipe.
    ypipe_t()
    {
        // Insert terminator element into the queue.
        _queue.push();

        // Let all the pointers to point to the terminator.
        _r = _w = _f = &_queue.back();
        _c.set(&_queue.back());
    }

    // Write an item to the pipe. Don't flush it yet. If incomplete is
    // set to true the item is assumed to be continued by items
    // subsequently written to the pipe. Incomplete items are never
    // flushed down the stream.
    void write(const T &value, bool incomplete) override
    {
        // Place the value to the queue, add new terminator element.
        _queue.back() = value;
        _queue.push();

        // Move the "flush up to here" pointer.
        if (!incomplete)
            _f = &_queue.back();
    }

    // Pop an incomplete item from the pipe. Returns true if such
    // item exists, false otherwise.
    bool unwrite(T *value) override
    {
        if (_f == &_queue.back()) SL_UNLIKELY_ATTR
            return false;
        _queue.unpush();
        *value = _queue.back();
        return true;
    }

    // Flush all the completed items into the pipe. Returns false if
    // the reader thread is sleeping. In that case, caller is obliged to
    // wake the reader up before using the pipe again.
    bool flush() override
    {
        // If there are no un-flushed items, do nothing.
        // In high-throughput scenarios, we typically have items to flush
        if (_w == _f)
            return true;

        // Try to set 'c' to 'f'.
        if (_c.cas(_w, _f) != _w) SL_UNLIKELY_ATTR {
            // Compare-and-swap was unsuccessful because 'c' is NULL.
            // This means that the reader is asleep. Therefore we don't
            // care about thread-safeness and update c in non-atomic
            // manner. We'll return false to let the caller know
            // that reader is sleeping.
            _c.set(_f);
            _w = _f;
            return false;
        }

        // Reader is alive. Nothing special to do now. Just move
        // the 'first un-flushed item' pointer to 'f'.
        _w = _f;
        return true;
    }

    // Check whether item is available for reading.
    bool check_read() override
    {
        // Was the value prefetched already? If so, return.
        // This is the fast path - data is already available
        if (&_queue.front() != _r && _r) SL_LIKELY_ATTR
            return true;

        // There's no prefetched value, so let us prefetch more values.
        // Prefetching is to simply retrieve the pointer from c in atomic
        // fashion. If there are no items to prefetch, set c to NULL.
        _r = _c.cas(&_queue.front(), nullptr);

        // If there are no elements prefetched, exit.
        if (&_queue.front() == _r || !_r) SL_UNLIKELY_ATTR
            return false;

        // There was at least one value prefetched.
        return true;
    }

    // Reads an item from the pipe. Returns false if there is no value.
    bool read(T *value) override
    {
        // Try to prefetch a value.
        if (!check_read()) SL_UNLIKELY_ATTR
            return false;

        // There was at least one value prefetched.
        *value = _queue.front();
        _queue.pop();
        return true;
    }

    // Applies the function fn to the first element in the pipe
    // and returns the value returned by the fn.
    bool probe(bool (*fn)(const T &)) override
    {
        const bool rc = check_read();
        slk_assert(rc);

        return (*fn)(_queue.front());
    }

  protected:
    // Allocation-efficient queue to store pipe items.
    yqueue_t<T, N> _queue;

    // Points to the first un-flushed item.
    T *_w;

    // Points to the first un-prefetched item.
    T *_r;

    // Points to the first item to be flushed in the future.
    T *_f;

    // The single point of contention between writer and reader thread.
    atomic_ptr_t<T> _c;

    SL_NON_COPYABLE_NOR_MOVABLE(ypipe_t)
};

}  // namespace slk

#endif
