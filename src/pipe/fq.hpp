/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_FQ_HPP_INCLUDED
#define SL_FQ_HPP_INCLUDED

#include "../core/array.hpp"
#include "../msg/msg.hpp"

namespace slk
{
class pipe_t;

class fq_t
{
  public:
    fq_t ();
    ~fq_t ();

    void attach (pipe_t *pipe_);
    void pipe_terminated (pipe_t *pipe_);
    void activated (pipe_t *pipe_);

    int recv (msg_t *msg_);
    int recvpipe (msg_t *msg_, pipe_t **pipe_);

    bool has_in ();

  private:
    typedef array_t<pipe_t, 1> pipes_t;
    pipes_t _pipes;

    int _active;
    int _current;
    bool _more;

    SL_NON_COPYABLE_NOR_MOVABLE (fq_t)
};
}

#endif