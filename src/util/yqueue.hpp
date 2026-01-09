/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - 1:1 Parity with yqueue.hpp */

#ifndef SL_YQUEUE_HPP_INCLUDED
#define SL_YQUEUE_HPP_INCLUDED

#include <cstdlib>
#include <cstddef>
#include "../util/err.hpp"
#include "../util/atomic_ptr.hpp"
#include "../util/macros.hpp"

namespace slk {

template <typename T, int N> class yqueue_t {
  public:
    yqueue_t() {
        _begin_chunk = allocate_chunk();
        alloc_assert(_begin_chunk);
        _begin_pos = 0; _back_chunk = NULL; _back_pos = 0; _end_chunk = _begin_chunk; _end_pos = 0;
    }

    ~yqueue_t() {
        while (_begin_chunk != _end_chunk) {
            chunk_t *next = _begin_chunk->next;
            free (_begin_chunk);
            _begin_chunk = next;
        }
        free (_begin_chunk);
    }

    T &front() { return _begin_chunk->values[_begin_pos]; }
    T &back() { return _back_chunk->values[_back_pos]; }

    void push() {
        _back_chunk = _end_chunk; _back_pos = _end_pos;
        if (++_end_pos != N) return;
        chunk_t *sc = _spare_chunk.xchg (NULL);
        if (sc) { _end_chunk->next = sc; sc->prev = _end_chunk; }
        else { _end_chunk->next = allocate_chunk(); alloc_assert (_end_chunk->next); _end_chunk->next->prev = _end_chunk; }
        _end_chunk = _end_chunk->next; _end_pos = 0;
    }

    void unpush() {
        if (_end_pos) --_end_pos;
        else { _end_chunk = _end_chunk->prev; _end_pos = N - 1; }
        _back_chunk = _end_chunk; _back_pos = _end_pos;
    }

    void pop() {
        if (++_begin_pos != N) return;
        chunk_t *old_chunk = _begin_chunk;
        _begin_chunk = _begin_chunk->next; _begin_chunk->prev = NULL; _begin_pos = 0;
        chunk_t *sc = _spare_chunk.xchg (old_chunk);
        if (sc) free (sc);
    }

  private:
    struct chunk_t {
        T values[N];
        chunk_t *prev;
        chunk_t *next;
    };

    chunk_t *allocate_chunk() { return (chunk_t*) malloc (sizeof (chunk_t)); }

    chunk_t *_begin_chunk, *_back_chunk, *_end_chunk;
    int _begin_pos, _back_pos, _end_pos;
    atomic_ptr_t<chunk_t> _spare_chunk;

    SL_NON_COPYABLE_NOR_MOVABLE (yqueue_t)
};

} // namespace slk

#endif