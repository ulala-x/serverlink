/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq - 1:1 Parity Parity with msg.cpp */

#include "../precompiled.hpp"
#include "msg.hpp"
#include "../util/err.hpp"
#include <stdlib.h>
#include <new>

namespace slk {

int msg_t::init () {
    _u.vsm.metadata = NULL; _u.vsm.type = type_vsm; _u.vsm.flags = 0;
    _u.vsm.size = 0; _u.vsm.group.type = group_type_short; _u.vsm.group.u.sgroup[0] = '\0';
    _u.vsm.routing_id = 0;
    return 0;
}

int msg_t::init_size (size_t size_) {
    if (size_ <= max_vsm_size) {
        _u.vsm.metadata = NULL; _u.vsm.type = type_vsm; _u.vsm.flags = 0;
        _u.vsm.size = (unsigned char) size_;
        _u.vsm.group.type = group_type_short; _u.vsm.group.u.sgroup[0] = '\0';
        _u.vsm.routing_id = 0;
    } else {
        _u.lmsg.metadata = NULL; _u.lmsg.type = type_lmsg; _u.lmsg.flags = 0;
        _u.lmsg.group.type = group_type_short; _u.lmsg.group.u.sgroup[0] = '\0';
        _u.lmsg.routing_id = 0;
        _u.lmsg.content = (content_t*) malloc (sizeof (content_t) + size_);
        if (!_u.lmsg.content) { errno = ENOMEM; return -1; }
        _u.lmsg.content->data = _u.lmsg.content + 1;
        _u.lmsg.content->size = size_;
        _u.lmsg.content->ffn = NULL; _u.lmsg.content->hint = NULL;
        _u.lmsg.content->refcnt.set(1);
    }
    return 0;
}

int msg_t::init_delimiter () {
    init(); _u.base.type = type_delimiter; return 0;
}

int msg_t::init_subscribe (size_t size_, const unsigned char *topic_) {
    int rc = init_size(size_); if (rc != 0) return rc;
    memcpy(data(), topic_, size_);
    _u.base.flags |= command; _u.base.flags |= subscribe;
    return 0;
}

int msg_t::init_cancel (size_t size_, const unsigned char *topic_) {
    int rc = init_size(size_); if (rc != 0) return rc;
    memcpy(data(), topic_, size_);
    _u.base.flags |= command; _u.base.flags |= cancel;
    return 0;
}

int msg_t::close () {
    if (_u.base.type == type_lmsg || _u.base.type == type_zclmsg) {
        if (_u.lmsg.content->refcnt.sub(1) == 0) {
            if (_u.lmsg.content->ffn) _u.lmsg.content->ffn (_u.lmsg.content->data, _u.lmsg.content->hint);
            free (_u.lmsg.content);
        }
    }
    _u.base.type = 0; return 0;
}

void *msg_t::data () {
    if (_u.base.type == type_vsm || _u.base.type == type_delimiter) return _u.vsm.data;
    if (_u.base.type == type_lmsg || _u.base.type == type_zclmsg) return _u.lmsg.content->data;
    return NULL;
}

size_t msg_t::size () const {
    if (_u.base.type == type_vsm || _u.base.type == type_delimiter) return _u.vsm.size;
    if (_u.base.type == type_lmsg || _u.base.type == type_zclmsg) return _u.lmsg.content->size;
    return 0;
}

int msg_t::move (msg_t *src_) {
    if (this == src_) return 0;
    close (); memcpy(this, src_, sizeof(msg_t)); src_->init ();
    return 0;
}

int msg_t::copy (msg_t *src_) {
    if (this == src_) return 0;
    close ();
    if (src_->_u.base.type == type_vsm || src_->_u.base.type == type_delimiter) {
        memcpy(this, src_, sizeof(msg_t));
    } else {
        src_->add_refs (1); memcpy(this, src_, sizeof(msg_t));
    }
    return 0;
}

void msg_t::add_refs (int refs_) {
    if (_u.base.type == type_lmsg || _u.base.type == type_zclmsg) _u.lmsg.content->refcnt.add (refs_);
}

bool msg_t::rm_refs (int refs_) {
    if (_u.base.type == type_lmsg || _u.base.type == type_zclmsg) {
        if (_u.lmsg.content->refcnt.sub (refs_) == 0) { close (); return false; }
    }
    return true;
}

unsigned char msg_t::flags () const { return _u.base.flags; }
void msg_t::set_flags (unsigned char flags_) { _u.base.flags |= flags_; }
void msg_t::reset_flags (unsigned char flags_) { _u.base.flags &= ~flags_; }
metadata_t *msg_t::metadata () const { return _u.base.metadata; }
void msg_t::set_metadata (metadata_t *metadata_) { _u.base.metadata = metadata_; }
void msg_t::reset_metadata () { _u.base.metadata = NULL; }
bool msg_t::is_routing_id () const { return (_u.base.flags & routing_id) != 0; }
bool msg_t::is_delimiter () const { return _u.base.type == type_delimiter; }
bool msg_t::is_vsm () const { return _u.base.type == type_vsm; }
bool msg_t::is_lmsg () const { return _u.base.type == type_lmsg; }
bool msg_t::is_zcmsg () const { return _u.base.type == type_zclmsg; }
uint32_t msg_t::get_routing_id () const { return _u.base.routing_id; }
int msg_t::set_routing_id (uint32_t id_) { _u.base.routing_id = id_; _u.base.flags |= routing_id; return 0; }

const char *msg_t::group () const { return ""; }
int msg_t::set_group (const char *) { return 0; }
int msg_t::set_group (const char *, size_t) { return 0; }
void msg_t::shrink (size_t size_) { if (_u.base.type == type_vsm) _u.vsm.size = (unsigned char)size_; else _u.lmsg.content->size = size_; }
bool msg_t::check () const { return _u.base.type >= type_min && _u.base.type <= type_zclmsg; }
atomic_counter_t *msg_t::refcnt () { return &_u.lmsg.content->refcnt; }

} // namespace slk