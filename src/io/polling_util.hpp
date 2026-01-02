/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SERVERLINK_POLLING_UTIL_HPP_INCLUDED
#define SERVERLINK_POLLING_UTIL_HPP_INCLUDED

#include <stdlib.h>
#include <vector>
#include <cstring>

#if defined _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#endif

#include "../util/macros.hpp"
#include "../util/err.hpp"

namespace slk
{
// Fast vector with stack storage for small sizes
template <typename T, size_t S> class fast_vector_t
{
  public:
    explicit fast_vector_t (const size_t nitems_)
    {
        if (nitems_ > S) {
            _buf = new (std::nothrow) T[nitems_];
            alloc_assert (_buf);
        } else {
            _buf = _static_buf;
        }
    }

    T &operator[] (const size_t i) { return _buf[i]; }

    ~fast_vector_t ()
    {
        if (_buf != _static_buf)
            delete[] _buf;
    }

  private:
    T _static_buf[S];
    T *_buf;

    SL_NON_COPYABLE_NOR_MOVABLE (fast_vector_t)
};

// Resizable fast vector
template <typename T, size_t S> class resizable_fast_vector_t
{
  public:
    resizable_fast_vector_t () : _dynamic_buf (NULL) {}

    void resize (const size_t nitems_)
    {
        if (_dynamic_buf) {
            _dynamic_buf->resize (nitems_);
        } else if (nitems_ > S) {
            _dynamic_buf = new (std::nothrow) std::vector<T> (nitems_);
            alloc_assert (_dynamic_buf);
            memcpy (&(*_dynamic_buf)[0], _static_buf, sizeof _static_buf);
        }
    }

    T *get_buf ()
    {
        return _dynamic_buf ? &(*_dynamic_buf)[0] : _static_buf;
    }

    T &operator[] (const size_t i) { return get_buf ()[i]; }

    ~resizable_fast_vector_t () { delete _dynamic_buf; }

  private:
    T _static_buf[S];
    std::vector<T> *_dynamic_buf;

    SL_NON_COPYABLE_NOR_MOVABLE (resizable_fast_vector_t)
};

#if defined SL_POLL_BASED_ON_POLL
typedef int timeout_t;

timeout_t
compute_timeout (bool first_pass_, long timeout_, uint64_t now_, uint64_t end_);
#endif

// Optimized fd_set handling
#if defined _WIN32
inline size_t valid_pollset_bytes (const fd_set &pollset_)
{
    // On Windows we don't need to copy the whole fd_set.
    // SOCKETS are continuous from the beginning of fd_array in fd_set.
    return reinterpret_cast<const char *> (
             &pollset_.fd_array[pollset_.fd_count])
           - reinterpret_cast<const char *> (&pollset_);
}

// C++20: Default pollitems for fast allocation (inline constexpr for type safety)
inline constexpr size_t SL_POLLITEMS_DFLT = 16;

class optimized_fd_set_t
{
  public:
    explicit optimized_fd_set_t (size_t nevents_) : _fd_set (1 + nevents_) {}

    fd_set *get () { return reinterpret_cast<fd_set *> (&_fd_set[0]); }

  private:
    fast_vector_t<SOCKET, 1 + SL_POLLITEMS_DFLT> _fd_set;
};

class resizable_optimized_fd_set_t
{
  public:
    void resize (size_t nevents_) { _fd_set.resize (1 + nevents_); }

    fd_set *get () { return reinterpret_cast<fd_set *> (&_fd_set[0]); }

  private:
    resizable_fast_vector_t<SOCKET, 1 + SL_POLLITEMS_DFLT> _fd_set;
};
#else
inline size_t valid_pollset_bytes (const fd_set & /*pollset_*/)
{
    return sizeof (fd_set);
}

// C++20: Default pollitems for fast allocation (inline constexpr for type safety)
inline constexpr size_t SL_POLLITEMS_DFLT = 16;

class optimized_fd_set_t
{
  public:
    explicit optimized_fd_set_t (size_t /*nevents_*/) {}

    fd_set *get () { return &_fd_set; }

  private:
    fd_set _fd_set;
};

class resizable_optimized_fd_set_t : public optimized_fd_set_t
{
  public:
    resizable_optimized_fd_set_t () : optimized_fd_set_t (0) {}

    void resize (size_t /*nevents_*/) {}
};
#endif
}

#endif
