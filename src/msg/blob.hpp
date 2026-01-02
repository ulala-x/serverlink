/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_BLOB_HPP_INCLUDED
#define SL_BLOB_HPP_INCLUDED

#include "../util/macros.hpp"
#include "../util/err.hpp"
#include "../util/config.hpp"

#include <stdlib.h>
#include <string.h>
#include <ios>

#if SL_HAVE_SPAN
#include <span>
#endif

#if SL_HAVE_THREE_WAY_COMPARISON
#include <compare>
#else
#include <algorithm>  // for std::min in C++17 fallback
#endif

// C++11 is required, so we always have move semantics
#define SL_HAS_MOVE_SEMANTICS
#define SL_MAP_INSERT_OR_EMPLACE(k, v) emplace (k, v)
#define SL_PUSH_OR_EMPLACE_BACK emplace_back
#define SL_MOVE(x) std::move (x)

namespace slk
{
struct reference_tag_t
{
};

//  Object to hold dynamically allocated opaque binary data.
//  On modern compilers, it will be movable but not copyable. Copies
//  must be explicitly created by set_deep_copy.
struct blob_t
{
    //  Creates an empty blob_t.
    blob_t () : _data (0), _size (0), _owned (true) {}

    //  Creates a blob_t of a given size, with uninitialized content.
    explicit blob_t (const size_t size_) :
        _data (static_cast<unsigned char *> (malloc (size_))),
        _size (size_),
        _owned (true)
    {
        alloc_assert (!_size || _data);
    }

    //  Creates a blob_t of a given size, an initializes content by copying
    // from another buffer.
    blob_t (const unsigned char *const data_, const size_t size_) :
        _data (static_cast<unsigned char *> (malloc (size_))),
        _size (size_),
        _owned (true)
    {
        alloc_assert (!size_ || _data);
        if (size_ && _data) {
            memcpy (_data, data_, size_);
        }
    }

    //  Creates a blob_t for temporary use that only references a
    //  pre-allocated block of data.
    //  Use with caution and ensure that the blob_t will not outlive
    //  the referenced data.
    blob_t (unsigned char *const data_, const size_t size_, reference_tag_t) :
        _data (data_), _size (size_), _owned (false)
    {
    }

    //  Returns the size of the blob_t.
    size_t size () const { return _size; }

    //  Returns a pointer to the data of the blob_t.
    const unsigned char *data () const { return _data; }

    //  Returns a pointer to the data of the blob_t.
    unsigned char *data () { return _data; }

#if SL_HAVE_SPAN
    //  Returns a span view over the blob data (mutable).
    [[nodiscard]] std::span<unsigned char> span () noexcept
    {
        return std::span<unsigned char> (_data, _size);
    }

    //  Returns a span view over the blob data (const).
    [[nodiscard]] std::span<const unsigned char> span () const noexcept
    {
        return std::span<const unsigned char> (_data, _size);
    }
#endif

#if SL_HAVE_THREE_WAY_COMPARISON
    //  C++20 three-way comparison operator
    //  Provides all six comparison operators (==, !=, <, <=, >, >=)
    [[nodiscard]] std::strong_ordering operator<=>(const blob_t &other_) const noexcept
    {
        // First compare sizes
        if (auto cmp = _size <=> other_._size; cmp != 0) {
            return cmp;
        }
        // If sizes are equal and zero, they're equal
        if (_size == 0) {
            return std::strong_ordering::equal;
        }
        // Otherwise compare contents lexicographically
        const int result = memcmp (_data, other_._data, _size);
        if (result < 0) {
            return std::strong_ordering::less;
        }
        if (result > 0) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }

    //  Equality comparison (required with <=> for optimal code generation)
    [[nodiscard]] bool operator==(const blob_t &other_) const noexcept
    {
        return _size == other_._size &&
               (_size == 0 || memcmp (_data, other_._data, _size) == 0);
    }
#else
    //  Legacy C++17 fallback: defines an order relationship on blob_t.
    bool operator<(blob_t const &other_) const
    {
        const int cmpres =
          memcmp (_data, other_._data, std::min (_size, other_._size));
        return cmpres < 0 || (cmpres == 0 && _size < other_._size);
    }
#endif

    //  Sets a blob_t to a deep copy of another blob_t.
    void set_deep_copy (blob_t const &other_)
    {
        clear ();
        _data = static_cast<unsigned char *> (malloc (other_._size));
        alloc_assert (!other_._size || _data);
        _size = other_._size;
        _owned = true;
        if (_size && _data) {
            memcpy (_data, other_._data, _size);
        }
    }

    //  Sets a blob_t to a copy of a given buffer.
    void set (const unsigned char *const data_, const size_t size_)
    {
        clear ();
        _data = static_cast<unsigned char *> (malloc (size_));
        alloc_assert (!size_ || _data);
        _size = size_;
        _owned = true;
        if (size_ && _data) {
            memcpy (_data, data_, size_);
        }
    }

    //  Empties a blob_t.
    void clear ()
    {
        if (_owned) {
            free (_data);
        }
        _data = 0;
        _size = 0;
    }

    ~blob_t ()
    {
        if (_owned) {
            free (_data);
        }
    }

#ifdef SL_HAS_MOVE_SEMANTICS
    blob_t (const blob_t &) = delete;
    blob_t &operator= (const blob_t &) = delete;

    blob_t (blob_t &&other_) noexcept : _data (other_._data),
                                        _size (other_._size),
                                        _owned (other_._owned)
    {
        other_._owned = false;
    }
    blob_t &operator= (blob_t &&other_) noexcept
    {
        if (this != &other_) {
            clear ();
            _data = other_._data;
            _size = other_._size;
            _owned = other_._owned;
            other_._owned = false;
        }
        return *this;
    }
#else
    blob_t (const blob_t &other) : _owned (false)
    {
        set_deep_copy (other);
    }
    blob_t &operator= (const blob_t &other)
    {
        if (this != &other) {
            clear ();
            set_deep_copy (other);
        }
        return *this;
    }
#endif

  private:
    unsigned char *_data;
    size_t _size;
    bool _owned;
};
}

#endif
