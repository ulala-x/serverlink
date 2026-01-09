/* SPDX-License-Identifier: MPL-2.0 */
#include "../precompiled.hpp"
#include "object.hpp"
#include "ctx.hpp"
#include "own.hpp"
#include "command.hpp"
#include "../pipe/pipe.hpp"

slk::object_t::object_t (slk::ctx_t *parent_, uint32_t tid_) : _parent (parent_), _tid (tid_) {}
slk::object_t::object_t (slk::object_t *parent_) : _parent (parent_->get_ctx ()), _tid (parent_->get_tid ()) {}

uint32_t slk::object_t::get_tid () const { return _tid; }
slk::ctx_t *slk::object_t::get_ctx () { return _parent; }

void slk::object_t::process_command (const command_t &cmd_) {
    switch (cmd_.type) {
        case command_t::activate_read: process_activate_read (); break;
        case command_t::activate_write: process_activate_write (cmd_.args.activate_write.msgs_read); break;
        case command_t::hiccup: process_hiccup (cmd_.args.hiccup.pipe); break;
        case command_t::pipe_term: process_pipe_term (); break;
        case command_t::pipe_term_ack: process_pipe_term_ack (); break;
        case command_t::term: process_term (cmd_.args.term.linger); break;
        case command_t::term_ack: process_term_ack (); break;
        case command_t::own: process_own (cmd_.args.own.object); break;
        case command_t::attach: process_attach (cmd_.args.attach.pipe); break;
        case command_t::bind: process_bind (cmd_.args.bind.pipe); break;
        default: slk_assert (false);
    }
}

void slk::object_t::send_command (const command_t &cmd_) { _parent->send_command (cmd_.destination->get_tid (), cmd_); }

void slk::object_t::send_hiccup (pipe_t *destination_) {
    command_t cmd; cmd.destination = (object_t*)destination_; cmd.type = command_t::hiccup;
    cmd.args.hiccup.pipe = destination_; send_command (cmd);
}

void slk::object_t::send_term (own_t *destination_, int linger_) {
    command_t cmd; cmd.destination = (object_t*)destination_; cmd.type = command_t::term;
    cmd.args.term.linger = linger_; send_command (cmd);
}

void slk::object_t::send_activate_read (pipe_t *destination_) {
    command_t cmd; cmd.destination = (object_t*)destination_; cmd.type = command_t::activate_read;
    send_command (cmd);
}

void slk::object_t::send_activate_write (pipe_t *destination_, uint64_t msgs_read_) {
    command_t cmd; cmd.destination = (object_t*)destination_; cmd.type = command_t::activate_write;
    cmd.args.activate_write.msgs_read = msgs_read_; send_command (cmd);
}

void slk::object_t::send_bind (socket_base_t *destination_, pipe_t *pipe_, bool) {
    command_t cmd; cmd.destination = (object_t*)destination_; cmd.type = command_t::bind;
    cmd.args.bind.pipe = pipe_; send_command (cmd);
}

void slk::object_t::process_stop () {}
void slk::object_t::process_plug () {}
void slk::object_t::process_own (own_t *) {}
void slk::object_t::process_attach (pipe_t *) {}
void slk::object_t::process_bind (pipe_t *) {}
void slk::object_t::process_activate_read () {}
void slk::object_t::process_activate_write (uint64_t) {}
void slk::object_t::process_hiccup (void *) {}
void slk::object_t::process_pipe_term () {}
void slk::object_t::process_pipe_term_ack () {}
void slk::object_t::process_term (int) {}
void slk::object_t::process_term_ack () {}