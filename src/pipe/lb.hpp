/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_LB_HPP_INCLUDED
#define SL_LB_HPP_INCLUDED

#include "../core/array.hpp"
#include "../msg/msg.hpp"
#include <vector>

namespace slk
{
class pipe_t;

class lb_t
{
  public:
    lb_t ();
    ~lb_t ();

    void attach (pipe_t *pipe_);
    void pipe_terminated (pipe_t *pipe_);
    void activated (pipe_t *pipe_);

    int send (msg_t *msg_);
    int sendpipe (msg_t *msg_, pipe_t **pipe_);

    bool has_out ();

  private:
    // libzmq parity: Use array with swap-based active pipe management
    typedef array_t<pipe_t, 2> pipes_t;
    pipes_t _pipes;

    // Number of active pipes (those that pass check_write)
    int _active;

    // Index of the pipe to send the next message to
    int _current;

    // True if we are in the middle of a multipart message
    bool _more;

    // True if we are dropping the current multipart message
    bool _dropping;

    SL_NON_COPYABLE_NOR_MOVABLE (lb_t)
};
}

#endif