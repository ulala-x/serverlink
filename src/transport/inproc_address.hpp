/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_INPROC_ADDRESS_HPP_INCLUDED
#define SL_INPROC_ADDRESS_HPP_INCLUDED

#include <string>

namespace slk
{
class inproc_address_t
{
  public:
    inproc_address_t ();
    inproc_address_t (const inproc_address_t &other_);
    inproc_address_t &operator= (const inproc_address_t &other_);
    ~inproc_address_t ();

    int resolve (const char *name_);

    int to_string (std::string &addr_) const;

    const std::string &name () const;

  private:
    std::string _name;
};
}

#endif
