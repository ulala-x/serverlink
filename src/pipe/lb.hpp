/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_LB_HPP_INCLUDED
#define SL_LB_HPP_INCLUDED

#include "pipe.hpp"
#include "../core/array.hpp"

namespace slk
{
class msg_t;
class pipe_t;

class lb_t : public array_item_t<1>
{
  public:
    lb_t ();
    ~lb_t ();

    void attach (pipe_t *pipe_);
    void pipe_terminated (pipe_t *pipe_);
    void activated (pipe_t *pipe_);

    int send (msg_t *msg_);
    int sendpipe (msg_t *msg_, pipe_t **pipe_);
    void flush ();

    bool has_out ();

  private:
    typedef array_t<pipe_t, 2> pipes_t;
    pipes_t _pipes;

    pipes_t::size_type _active;
    pipes_t::size_type _current;

    bool _more;
    bool _dropping;

    SL_NON_COPYABLE_NOR_MOVABLE (lb_t)
};
}

#endif
