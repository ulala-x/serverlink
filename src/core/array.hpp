/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_ARRAY_HPP_INCLUDED
#define SL_ARRAY_HPP_INCLUDED

#include <vector>
#include <algorithm>
#include "../util/err.hpp"

namespace slk
{
//  Base class for objects stored in the array. If you want to store
//  same object in multiple arrays, each of those arrays has to have
//  different ID. The item itself has to be derived from instantiations of
//  array_item_t template for all relevant IDs.

template <int ID = 0> class array_item_t
{
  public:
    inline array_item_t () : _array_index (-1) {}

    //  The parameter is ID of the array we want to know the index in.
    //  If the item isn't in the array, -1 is returned.
    inline int get_array_index () const { return _array_index; }

  protected:
    //  The index of this object in the containing array.
    int _array_index;

    template <typename T, int ID1> friend class array_t;
};

//  Fast array implementation with O(1) access, insertion and removal.
//  Array stores pointers rather than objects. The objects have to be
//  derived from array_item_t<ID> class.

template <typename T, int ID = 0> class array_t
{
  public:
    typedef typename std::vector<T *>::size_type size_type;

    inline array_t () {}

    inline ~array_t () {}

    inline size_type size () const { return _items.size (); }

    inline bool empty () const { return _items.empty (); }

    inline T *&operator[] (size_type index_) { return _items[index_]; }

    inline const T *operator[] (size_type index_) const
    {
        return _items[index_];
    }

    inline void push_back (T *item_)
    {
        if (item_)
            item_->array_item_t<ID>::_array_index = static_cast<int> (_items.size ());
        _items.push_back (item_);
    }

    inline void erase (T *item_)
    {
        erase (item_->array_item_t<ID>::_array_index);
    }

    inline void erase (size_type index_)
    {
        if (_items.back ())
            _items.back ()->array_item_t<ID>::_array_index = static_cast<int> (index_);
        _items[index_] = _items.back ();
        _items.pop_back ();
    }

    inline void swap (size_type index1_, size_type index2_)
    {
        if (_items[index1_])
            _items[index1_]->array_item_t<ID>::_array_index = static_cast<int> (index2_);
        if (_items[index2_])
            _items[index2_]->array_item_t<ID>::_array_index = static_cast<int> (index1_);
        std::swap (_items[index1_], _items[index2_]);
    }

    inline void clear () { _items.clear (); }

    inline size_type index (T *item_) const
    {
        return static_cast<size_type> (item_->array_item_t<ID>::_array_index);
    }

  private:
    typedef std::vector<T *> items_t;
    items_t _items;

    array_t (const array_t &);
    const array_t &operator= (const array_t &);
};
}

#endif
