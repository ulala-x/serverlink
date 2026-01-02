/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_YQUEUE_HPP_INCLUDED
#define SL_YQUEUE_HPP_INCLUDED

#include <cstdlib>
#include <cstddef>

#include "err.hpp"
#include "atomic_ptr.hpp"
#include "macros.hpp"
#include <serverlink/config.h>

#if SL_HAVE_CONCEPTS
#include "concepts.hpp"
#endif

namespace slk {

// yqueue is an efficient queue implementation. The main goal is
// to minimise number of allocations/deallocations needed. Thus yqueue
// allocates/deallocates elements in batches of N.
//
// yqueue allows one thread to use push/back function and another one
// to use pop/front functions. However, user must ensure that there's no
// pop on the empty queue and that both threads don't access the same
// element in unsynchronised manner.
//
// T is the type of the object in the queue (must satisfy YPipeable concept).
// N is granularity of the queue (how many pushes have to be done till
// actual memory allocation is required).

#if SL_HAVE_CONCEPTS
template <YPipeable T, int N>
#else
template <typename T, int N>
#endif
class yqueue_t {
  public:
    // Create the queue.
    yqueue_t()
    {
        _begin_chunk = allocate_chunk();
        alloc_assert(_begin_chunk);
        _begin_pos = 0;
        _back_chunk = nullptr;
        _back_pos = 0;
        _end_chunk = _begin_chunk;
        _end_pos = 0;
    }

    // Destroy the queue.
    ~yqueue_t()
    {
        while (true) {
            if (_begin_chunk == _end_chunk) {
                free(_begin_chunk);
                break;
            }
            chunk_t *o = _begin_chunk;
            _begin_chunk = _begin_chunk->next;
            free(o);
        }

        chunk_t *sc = _spare_chunk.xchg(nullptr);
        free(sc);
    }

    // Returns reference to the front element of the queue.
    // If the queue is empty, behaviour is undefined.
    T &front() { return _begin_chunk->values[_begin_pos]; }

    // Returns reference to the back element of the queue.
    // If the queue is empty, behaviour is undefined.
    T &back() { return _back_chunk->values[_back_pos]; }

    // Adds an element to the back end of the queue.
    void push()
    {
        _back_chunk = _end_chunk;
        _back_pos = _end_pos;

        if (++_end_pos != N)
            return;

        chunk_t *sc = _spare_chunk.xchg(nullptr);
        if (sc) {
            _end_chunk->next = sc;
            sc->prev = _end_chunk;
        } else {
            _end_chunk->next = allocate_chunk();
            alloc_assert(_end_chunk->next);
            _end_chunk->next->prev = _end_chunk;
        }
        _end_chunk = _end_chunk->next;
        _end_pos = 0;
    }

    // Removes element from the back end of the queue (rollback).
    // The caller must guarantee that the queue isn't empty.
    void unpush()
    {
        if (_back_pos)
            --_back_pos;
        else {
            _back_pos = N - 1;
            _back_chunk = _back_chunk->prev;
        }

        if (_end_pos)
            --_end_pos;
        else {
            _end_pos = N - 1;
            _end_chunk = _end_chunk->prev;
            free(_end_chunk->next);
            _end_chunk->next = nullptr;
        }
    }

    // Removes an element from the front end of the queue.
    void pop()
    {
        if (++_begin_pos == N) {
            chunk_t *o = _begin_chunk;
            _begin_chunk = _begin_chunk->next;
            _begin_chunk->prev = nullptr;
            _begin_pos = 0;

            // 'o' has been more recently used than _spare_chunk,
            // so for cache reasons we'll get rid of the spare and
            // use 'o' as the spare.
            chunk_t *cs = _spare_chunk.xchg(o);
            free(cs);
        }
    }

  private:
    // Individual memory chunk to hold N elements.
    struct chunk_t {
        T values[N];
        chunk_t *prev;
        chunk_t *next;
    };

    static chunk_t *allocate_chunk()
    {
        return static_cast<chunk_t *>(malloc(sizeof(chunk_t)));
    }

    // Back position may point to invalid memory if the queue is empty,
    // while begin & end positions are always valid. Begin position is
    // accessed exclusively be queue reader (front/pop), while back and
    // end positions are accessed exclusively by queue writer (back/push).
    chunk_t *_begin_chunk;
    int _begin_pos;
    chunk_t *_back_chunk;
    int _back_pos;
    chunk_t *_end_chunk;
    int _end_pos;

    // Spare chunk to reduce malloc/free calls.
    atomic_ptr_t<chunk_t> _spare_chunk;

    SL_NON_COPYABLE_NOR_MOVABLE(yqueue_t)
};

}  // namespace slk

#endif
