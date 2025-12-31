/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "metadata.hpp"

// Constants for message properties
#define SL_MSG_PROPERTY_ROUTING_ID "Routing-Id"

slk::metadata_t::metadata_t (const dict_t &dict_) : _ref_cnt (1), _dict (dict_)
{
}

const char *slk::metadata_t::get (const std::string &property_) const
{
    const dict_t::const_iterator it = _dict.find (property_);
    if (it == _dict.end ()) {
        // Handle deprecated "Identity" property name
        if (property_ == "Identity")
            return get (SL_MSG_PROPERTY_ROUTING_ID);

        return NULL;
    }
    return it->second.c_str ();
}

void slk::metadata_t::add_ref ()
{
    _ref_cnt.add (1);
}

bool slk::metadata_t::drop_ref ()
{
    return !_ref_cnt.sub (1);
}
