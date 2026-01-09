/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_CLOCK_HPP_INCLUDED
#define SL_CLOCK_HPP_INCLUDED

#include <stdint.h>
#include "../util/macros.hpp"

namespace slk
{
class clock_t
{
  public:
    clock_t ();
    virtual ~clock_t () = default;

    uint64_t now_ms ();
    uint64_t now_us ();
    static uint64_t rdtsc ();

  private:
    uint64_t _last_tsc;
    uint64_t _last_time;

    SL_NON_COPYABLE_NOR_MOVABLE (clock_t)
};
}

#endif