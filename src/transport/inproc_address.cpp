/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "precompiled.hpp"
#include "../util/macros.hpp"
#include "inproc_address.hpp"
#include "../util/err.hpp"

#include <string>
#include <sstream>

slk::inproc_address_t::inproc_address_t ()
{
}

slk::inproc_address_t::inproc_address_t (const inproc_address_t &other_) :
    _name (other_._name)
{
}

slk::inproc_address_t &
slk::inproc_address_t::operator= (const inproc_address_t &other_)
{
    if (this != &other_) {
        _name = other_._name;
    }
    return *this;
}

slk::inproc_address_t::~inproc_address_t ()
{
}

int slk::inproc_address_t::resolve (const char *name_)
{
    if (!name_) {
        errno = EINVAL;
        return -1;
    }

    _name = name_;

    // Validate that the name is not empty
    if (_name.empty ()) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

int slk::inproc_address_t::to_string (std::string &addr_) const
{
    if (_name.empty ()) {
        addr_.clear ();
        return -1;
    }

    std::stringstream s;
    s << "inproc://" << _name;
    addr_ = s.str ();
    return 0;
}

const std::string &slk::inproc_address_t::name () const
{
    return _name;
}
