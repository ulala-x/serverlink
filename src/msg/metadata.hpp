/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_METADATA_HPP_INCLUDED
#define SL_METADATA_HPP_INCLUDED

#include <map>
#include <string>

#include "../util/atomic_counter.hpp"
#include "../util/macros.hpp"

namespace slk
{
class metadata_t
{
  public:
    typedef std::map<std::string, std::string> dict_t;

    metadata_t (const dict_t &dict_);

    //  Returns pointer to property value or NULL if
    //  property is not found.
    const char *get (const std::string &property_) const;

    void add_ref ();

    //  Drop reference. Returns true iff the reference
    //  counter drops to zero.
    bool drop_ref ();

  private:
    //  Reference counter.
    atomic_counter_t _ref_cnt;

    //  Dictionary holding metadata.
    const dict_t _dict;

    SL_NON_COPYABLE_NOR_MOVABLE (metadata_t)
};
}

#endif
