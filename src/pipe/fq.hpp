/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_FQ_HPP_INCLUDED
#define SL_FQ_HPP_INCLUDED

#include "../core/array.hpp"
#include "../util/macros.hpp"

namespace slk
{
class msg_t;
class pipe_t;

//  Class manages a set of inbound pipes. On receive it performs fair
//  queueing so that senders gone berserk won't cause denial of
//  service for decent senders.

class fq_t
{
  public:
    fq_t ();
    ~fq_t ();

    void attach (slk::pipe_t *pipe_);
    void activated (slk::pipe_t *pipe_);
    void pipe_terminated (slk::pipe_t *pipe_);

    int recv (slk::msg_t *msg_);
    int recvpipe (slk::msg_t *msg_, slk::pipe_t **pipe_);
    bool has_in ();

  private:
    //  Inbound pipes.
    typedef array_t<slk::pipe_t, 1> pipes_t;
    pipes_t _pipes;

    //  Number of active pipes. All the active pipes are located at the
    //  beginning of the pipes array.
    pipes_t::size_type _active;

    //  Index of the next bound pipe to read a message from.
    pipes_t::size_type _current;

    //  If true, part of a multipart message was already received, but
    //  there are following parts still waiting in the current pipe.
    bool _more;

    SL_NON_COPYABLE_NOR_MOVABLE (fq_t)
};
}

#endif