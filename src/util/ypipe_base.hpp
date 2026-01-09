/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_YPIPE_BASE_HPP_INCLUDED
#define SL_YPIPE_BASE_HPP_INCLUDED

#include "macros.hpp"

namespace slk
{
template <typename T> class ypipe_base_t
{
  public:
    virtual ~ypipe_base_t () = default;
    virtual void write (const T &value_, bool incomplete_) = 0;
    virtual bool unwrite (T *value_) = 0;
    virtual bool flush () = 0;
    virtual bool check_read () = 0;
    virtual bool read (T *value_) = 0;
    virtual bool probe (bool (*fn_) (const T &)) = 0;
};
}

#endif
