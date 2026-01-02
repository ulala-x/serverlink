/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_SUB_HPP_INCLUDED
#define SL_SUB_HPP_INCLUDED

#include "xsub.hpp"

namespace slk
{
class ctx_t;
class msg_t;

class sub_t final : public xsub_t
{
  public:
    sub_t (ctx_t *parent_, uint32_t tid_, int sid_);
    ~sub_t () override;

  protected:
    int xsetsockopt (int option_,
                     const void *optval_,
                     size_t optvallen_) override;
    int xsend (msg_t *msg_) override;
    bool xhas_out () override;

    SL_NON_COPYABLE_NOR_MOVABLE (sub_t)
};
}

#endif
