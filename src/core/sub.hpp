/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#ifndef SL_SUB_HPP_INCLUDED
#define SL_SUB_HPP_INCLUDED

#include "xsub.hpp"

namespace slk
{
class ctx_t;
class msg_t;

class sub_t SL_FINAL : public xsub_t
{
  public:
    sub_t (ctx_t *parent_, uint32_t tid_, int sid_);
    ~sub_t () SL_OVERRIDE;

  protected:
    int xsetsockopt (int option_,
                     const void *optval_,
                     size_t optvallen_) SL_OVERRIDE;
    int xsend (msg_t *msg_) SL_OVERRIDE;
    bool xhas_out () SL_OVERRIDE;

    SL_NON_COPYABLE_NOR_MOVABLE (sub_t)
};
}

#endif
