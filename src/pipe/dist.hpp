/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_DIST_HPP_INCLUDED
#define SL_DIST_HPP_INCLUDED

#include <vector>
#include "../core/array.hpp"
#include "../msg/msg.hpp"

namespace slk
{
class pipe_t;

class dist_t
{
  public:
    dist_t ();
    ~dist_t ();

    void attach (pipe_t *pipe_);
    void pipe_terminated (pipe_t *pipe_);
    void activated (pipe_t *pipe_);

    int send_to_all (msg_t *msg_);
    int send_to_matching (msg_t *msg_);

    bool has_out ();

  private:
    void distribute (msg_t *msg_);

    typedef array_t<pipe_t, 2> pipes_t;
    pipes_t _pipes;

    // Number of all known pipes
    int _eligible;
    // Number of active pipes (those that pass check_write)
    int _active;
    // Number of pipes matching the current message
    int _matching;

    bool _more;

    SL_NON_COPYABLE_NOR_MOVABLE (dist_t)
};
}

#endif