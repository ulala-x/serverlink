/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "msg.hpp"

#include <string.h>
#include <stdlib.h>
#include <new>

#include "../util/likely.hpp"
#include "../util/macros.hpp"
#include "metadata.hpp"
#include "../util/err.hpp"

// Message property constant
#define SL_MSG_PROPERTY_ROUTING_ID "Routing-Id"

bool slk::msg_t::check () const
{
    return _u.base.type >= type_min && _u.base.type <= type_max;
}

int slk::msg_t::init (void *data_,
                      size_t size_,
                      msg_free_fn *ffn_,
                      void *hint_,
                      content_t *content_)
{
    if (size_ < max_vsm_size) {
        const int rc = init_size (size_);

        if (rc != -1) {
            memcpy (data (), data_, size_);
            return 0;
        }
        return -1;
    }
    if (content_) {
        return init_external_storage (content_, data_, size_, ffn_, hint_);
    }
    return init_data (data_, size_, ffn_, hint_);
}

int slk::msg_t::init ()
{
    _u.vsm.metadata = NULL;
    _u.vsm.type = type_vsm;
    _u.vsm.flags = 0;
    _u.vsm.size = 0;
    _u.vsm.group.sgroup.group[0] = '\0';
    _u.vsm.group.type = group_type_short;
    _u.vsm.routing_id = 0;
    return 0;
}

int slk::msg_t::init_size (size_t size_)
{
    if (size_ <= max_vsm_size) {
        _u.vsm.metadata = NULL;
        _u.vsm.type = type_vsm;
        _u.vsm.flags = 0;
        _u.vsm.size = static_cast<unsigned char> (size_);
        _u.vsm.group.sgroup.group[0] = '\0';
        _u.vsm.group.type = group_type_short;
        _u.vsm.routing_id = 0;
    } else {
        _u.lmsg.metadata = NULL;
        _u.lmsg.type = type_lmsg;
        _u.lmsg.flags = 0;
        _u.lmsg.group.sgroup.group[0] = '\0';
        _u.lmsg.group.type = group_type_short;
        _u.lmsg.routing_id = 0;
        _u.lmsg.content = NULL;
        if (sizeof (content_t) + size_ > size_)
            _u.lmsg.content =
              static_cast<content_t *> (malloc (sizeof (content_t) + size_));
        if (unlikely (!_u.lmsg.content)) {
            errno = ENOMEM;
            return -1;
        }

        _u.lmsg.content->data = _u.lmsg.content + 1;
        _u.lmsg.content->size = size_;
        _u.lmsg.content->ffn = NULL;
        _u.lmsg.content->hint = NULL;
        _u.lmsg.content->refcnt.set(1);
    }
    return 0;
}

int slk::msg_t::init_buffer (const void *buf_, size_t size_)
{
    const int rc = init_size (size_);
    if (unlikely (rc < 0)) {
        return -1;
    }
    if (size_) {
        // NULL and zero size is allowed
        slk_assert (NULL != buf_);
        memcpy (data (), buf_, size_);
    }
    return 0;
}

int slk::msg_t::init_external_storage (content_t *content_,
                                       void *data_,
                                       size_t size_,
                                       msg_free_fn *ffn_,
                                       void *hint_)
{
    slk_assert (NULL != data_);
    slk_assert (NULL != content_);

    _u.zclmsg.metadata = NULL;
    _u.zclmsg.type = type_zclmsg;
    _u.zclmsg.flags = 0;
    _u.zclmsg.group.sgroup.group[0] = '\0';
    _u.zclmsg.group.type = group_type_short;
    _u.zclmsg.routing_id = 0;

    _u.zclmsg.content = content_;
    _u.zclmsg.content->data = data_;
    _u.zclmsg.content->size = size_;
    _u.zclmsg.content->ffn = ffn_;
    _u.zclmsg.content->hint = hint_;
    new (&_u.zclmsg.content->refcnt) slk::atomic_counter_t ();

    return 0;
}

int slk::msg_t::init_data (void *data_,
                           size_t size_,
                           msg_free_fn *ffn_,
                           void *hint_)
{
    //  If data is NULL and size is not 0, a segfault
    //  would occur once the data is accessed
    slk_assert (data_ != NULL || size_ == 0);

    //  Initialize constant message if there's no need to deallocate
    if (ffn_ == NULL) {
        _u.cmsg.metadata = NULL;
        _u.cmsg.type = type_cmsg;
        _u.cmsg.flags = 0;
        _u.cmsg.data = data_;
        _u.cmsg.size = size_;
        _u.cmsg.group.sgroup.group[0] = '\0';
        _u.cmsg.group.type = group_type_short;
        _u.cmsg.routing_id = 0;
    } else {
        _u.lmsg.metadata = NULL;
        _u.lmsg.type = type_lmsg;
        _u.lmsg.flags = 0;
        _u.lmsg.group.sgroup.group[0] = '\0';
        _u.lmsg.group.type = group_type_short;
        _u.lmsg.routing_id = 0;
        _u.lmsg.content =
          static_cast<content_t *> (malloc (sizeof (content_t)));
        if (!_u.lmsg.content) {
            errno = ENOMEM;
            return -1;
        }

        _u.lmsg.content->data = data_;
        _u.lmsg.content->size = size_;
        _u.lmsg.content->ffn = ffn_;
        _u.lmsg.content->hint = hint_;
        new (&_u.lmsg.content->refcnt) slk::atomic_counter_t ();
    }
    return 0;
}

int slk::msg_t::init_delimiter ()
{
    _u.delimiter.metadata = NULL;
    _u.delimiter.type = type_delimiter;
    _u.delimiter.flags = 0;
    _u.delimiter.group.sgroup.group[0] = '\0';
    _u.delimiter.group.type = group_type_short;
    _u.delimiter.routing_id = 0;
    return 0;
}

int slk::msg_t::init_join ()
{
    _u.base.metadata = NULL;
    _u.base.type = type_join;
    _u.base.flags = 0;
    _u.base.group.sgroup.group[0] = '\0';
    _u.base.group.type = group_type_short;
    _u.base.routing_id = 0;
    return 0;
}

int slk::msg_t::init_leave ()
{
    _u.base.metadata = NULL;
    _u.base.type = type_leave;
    _u.base.flags = 0;
    _u.base.group.sgroup.group[0] = '\0';
    _u.base.group.type = group_type_short;
    _u.base.routing_id = 0;
    return 0;
}

int slk::msg_t::init_subscribe (const size_t size_, const unsigned char *topic_)
{
    int rc = init_size (size_);
    if (rc == 0) {
        set_flags (slk::msg_t::subscribe);

        //  We explicitly allow a NULL subscription with size zero
        if (size_) {
            slk_assert (topic_);
            memcpy (data (), topic_, size_);
        }
    }
    return rc;
}

int slk::msg_t::init_cancel (const size_t size_, const unsigned char *topic_)
{
    int rc = init_size (size_);
    if (rc == 0) {
        set_flags (slk::msg_t::cancel);

        //  We explicitly allow a NULL subscription with size zero
        if (size_) {
            slk_assert (topic_);
            memcpy (data (), topic_, size_);
        }
    }
    return rc;
}

int slk::msg_t::close ()
{
    //  Check the validity of the message.
    if (unlikely (!check ())) {
        errno = EFAULT;
        return -1;
    }

    if (_u.base.type == type_lmsg) {
        //  If the content is not shared, or if it is shared and the reference
        //  count has dropped to zero, deallocate it.
        if (!(_u.lmsg.flags & msg_t::shared)
            || !_u.lmsg.content->refcnt.sub (1)) {
            //  We used "placement new" operator to initialize the reference
            //  counter so we call the destructor explicitly now.
            _u.lmsg.content->refcnt.~atomic_counter_t ();

            if (_u.lmsg.content->ffn)
                _u.lmsg.content->ffn (_u.lmsg.content->data,
                                      _u.lmsg.content->hint);
            free (_u.lmsg.content);
        }
    }

    if (is_zcmsg ()) {
        slk_assert (_u.zclmsg.content->ffn);

        //  If the content is not shared, or if it is shared and the reference
        //  count has dropped to zero, deallocate it.
        if (!(_u.zclmsg.flags & msg_t::shared)
            || !_u.zclmsg.content->refcnt.sub (1)) {
            //  We used "placement new" operator to initialize the reference
            //  counter so we call the destructor explicitly now.
            _u.zclmsg.content->refcnt.~atomic_counter_t ();

            _u.zclmsg.content->ffn (_u.zclmsg.content->data,
                                    _u.zclmsg.content->hint);
        }
    }

    if (_u.base.metadata != NULL) {
        if (_u.base.metadata->drop_ref ()) {
            SL_DELETE (_u.base.metadata);
        }
        _u.base.metadata = NULL;
    }

    if (_u.base.group.type == group_type_long) {
        if (!_u.base.group.lgroup.content->refcnt.sub (1)) {
            //  We used "placement new" operator to initialize the reference
            //  counter so we call the destructor explicitly now.
            _u.base.group.lgroup.content->refcnt.~atomic_counter_t ();

            free (_u.base.group.lgroup.content);
        }
    }

    //  Make the message invalid.
    _u.base.type = 0;

    return 0;
}

int slk::msg_t::move (msg_t &src_)
{
    //  Check the validity of the source.
    if (unlikely (!src_.check ())) {
        errno = EFAULT;
        return -1;
    }

    int rc = close ();
    if (unlikely (rc < 0))
        return rc;

    *this = src_;

    rc = src_.init ();
    if (unlikely (rc < 0))
        return rc;

    return 0;
}

int slk::msg_t::copy (msg_t &src_)
{
    //  Check the validity of the source.
    if (unlikely (!src_.check ())) {
        errno = EFAULT;
        return -1;
    }

    const int rc = close ();
    if (unlikely (rc < 0))
        return rc;

    // The initial reference count, when a non-shared message is initially
    // shared (between the original and the copy we create here).
    const atomic_counter_t::integer_t initial_shared_refcnt = 2;

    if (src_.is_lmsg () || src_.is_zcmsg ()) {
        //  One reference is added to shared messages. Non-shared messages
        //  are turned into shared messages.
        if (src_.flags () & msg_t::shared)
            src_.refcnt ()->add (1);
        else {
            src_.set_flags (msg_t::shared);
            src_.refcnt ()->set (initial_shared_refcnt);
        }
    }

    if (src_._u.base.metadata != NULL)
        src_._u.base.metadata->add_ref ();

    if (src_._u.base.group.type == group_type_long)
        src_._u.base.group.lgroup.content->refcnt.add (1);

    *this = src_;

    return 0;
}

void *slk::msg_t::data ()
{
    //  Check the validity of the message.
    slk_assert (check ());

    switch (_u.base.type) {
        case type_vsm:
            return _u.vsm.data;
        case type_lmsg:
            return _u.lmsg.content->data;
        case type_cmsg:
            return _u.cmsg.data;
        case type_zclmsg:
            return _u.zclmsg.content->data;
        default:
            slk_assert (false);
            return NULL;
    }
}

size_t slk::msg_t::size () const
{
    //  Check the validity of the message.
    slk_assert (check ());

    switch (_u.base.type) {
        case type_vsm:
            return _u.vsm.size;
        case type_lmsg:
            return _u.lmsg.content->size;
        case type_zclmsg:
            return _u.zclmsg.content->size;
        case type_cmsg:
            return _u.cmsg.size;
        default:
            slk_assert (false);
            return 0;
    }
}

void slk::msg_t::shrink (size_t new_size_)
{
    //  Check the validity of the message.
    slk_assert (check ());
    slk_assert (new_size_ <= size ());

    switch (_u.base.type) {
        case type_vsm:
            _u.vsm.size = static_cast<unsigned char> (new_size_);
            break;
        case type_lmsg:
            _u.lmsg.content->size = new_size_;
            break;
        case type_zclmsg:
            _u.zclmsg.content->size = new_size_;
            break;
        case type_cmsg:
            _u.cmsg.size = new_size_;
            break;
        default:
            slk_assert (false);
    }
}

unsigned char slk::msg_t::flags () const
{
    return _u.base.flags;
}

void slk::msg_t::set_flags (unsigned char flags_)
{
    _u.base.flags |= flags_;
}

void slk::msg_t::reset_flags (unsigned char flags_)
{
    _u.base.flags &= ~flags_;
}

slk::metadata_t *slk::msg_t::metadata () const
{
    return _u.base.metadata;
}

void slk::msg_t::set_metadata (slk::metadata_t *metadata_)
{
    slk_assert (metadata_ != NULL);
    slk_assert (_u.base.metadata == NULL);
    metadata_->add_ref ();
    _u.base.metadata = metadata_;
}

void slk::msg_t::reset_metadata ()
{
    if (_u.base.metadata) {
        if (_u.base.metadata->drop_ref ()) {
            SL_DELETE (_u.base.metadata);
        }
        _u.base.metadata = NULL;
    }
}

bool slk::msg_t::is_routing_id () const
{
    return (_u.base.flags & routing_id) == routing_id;
}

bool slk::msg_t::is_credential () const
{
    return (_u.base.flags & credential) == credential;
}

bool slk::msg_t::is_delimiter () const
{
    return _u.base.type == type_delimiter;
}

bool slk::msg_t::is_vsm () const
{
    return _u.base.type == type_vsm;
}

bool slk::msg_t::is_cmsg () const
{
    return _u.base.type == type_cmsg;
}

bool slk::msg_t::is_lmsg () const
{
    return _u.base.type == type_lmsg;
}

bool slk::msg_t::is_zcmsg () const
{
    return _u.base.type == type_zclmsg;
}

bool slk::msg_t::is_join () const
{
    return _u.base.type == type_join;
}

bool slk::msg_t::is_leave () const
{
    return _u.base.type == type_leave;
}

bool slk::msg_t::is_ping () const
{
    return (_u.base.flags & CMD_TYPE_MASK) == ping;
}

bool slk::msg_t::is_pong () const
{
    return (_u.base.flags & CMD_TYPE_MASK) == pong;
}

bool slk::msg_t::is_close_cmd () const
{
    return (_u.base.flags & CMD_TYPE_MASK) == close_cmd;
}

size_t slk::msg_t::command_body_size () const
{
    if (this->is_ping () || this->is_pong ())
        return this->size () - ping_cmd_name_size;
    else if (!(this->flags () & msg_t::command)
             && (this->is_subscribe () || this->is_cancel ()))
        return this->size ();
    else if (this->is_subscribe ())
        return this->size () - sub_cmd_name_size;
    else if (this->is_cancel ())
        return this->size () - cancel_cmd_name_size;

    return 0;
}

void *slk::msg_t::command_body ()
{
    unsigned char *data = NULL;

    if (this->is_ping () || this->is_pong ())
        data =
          static_cast<unsigned char *> (this->data ()) + ping_cmd_name_size;
    //  With inproc, command flag is not set for sub/cancel
    else if (!(this->flags () & msg_t::command)
             && (this->is_subscribe () || this->is_cancel ()))
        data = static_cast<unsigned char *> (this->data ());
    else if (this->is_subscribe ())
        data = static_cast<unsigned char *> (this->data ()) + sub_cmd_name_size;
    else if (this->is_cancel ())
        data =
          static_cast<unsigned char *> (this->data ()) + cancel_cmd_name_size;

    return data;
}

void slk::msg_t::add_refs (int refs_)
{
    slk_assert (refs_ >= 0);

    //  Operation not supported for messages with metadata.
    slk_assert (_u.base.metadata == NULL);

    //  No copies required.
    if (!refs_)
        return;

    //  VSMs, CMSGS and delimiters can be copied straight away. The only
    //  message type that needs special care are long messages.
    if (_u.base.type == type_lmsg || is_zcmsg ()) {
        if (_u.base.flags & msg_t::shared)
            refcnt ()->add (refs_);
        else {
            refcnt ()->set (refs_ + 1);
            _u.base.flags |= msg_t::shared;
        }
    }
}

bool slk::msg_t::rm_refs (int refs_)
{
    slk_assert (refs_ >= 0);

    //  Operation not supported for messages with metadata.
    slk_assert (_u.base.metadata == NULL);

    //  No copies required.
    if (!refs_)
        return true;

    //  If there's only one reference close the message.
    if ((_u.base.type != type_zclmsg && _u.base.type != type_lmsg)
        || !(_u.base.flags & msg_t::shared)) {
        close ();
        return false;
    }

    //  The only message type that needs special care are long and zcopy messages.
    if (_u.base.type == type_lmsg && !_u.lmsg.content->refcnt.sub (refs_)) {
        //  We used "placement new" operator to initialize the reference
        //  counter so we call the destructor explicitly now.
        _u.lmsg.content->refcnt.~atomic_counter_t ();

        if (_u.lmsg.content->ffn)
            _u.lmsg.content->ffn (_u.lmsg.content->data, _u.lmsg.content->hint);
        free (_u.lmsg.content);

        return false;
    }

    if (is_zcmsg () && !_u.zclmsg.content->refcnt.sub (refs_)) {
        // storage for rfcnt is provided externally
        if (_u.zclmsg.content->ffn) {
            _u.zclmsg.content->ffn (_u.zclmsg.content->data,
                                    _u.zclmsg.content->hint);
        }

        return false;
    }

    return true;
}

uint32_t slk::msg_t::get_routing_id () const
{
    return _u.base.routing_id;
}

int slk::msg_t::set_routing_id (uint32_t routing_id_)
{
    if (routing_id_) {
        _u.base.routing_id = routing_id_;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

int slk::msg_t::reset_routing_id ()
{
    _u.base.routing_id = 0;
    return 0;
}

const char *slk::msg_t::group () const
{
    if (_u.base.group.type == group_type_long)
        return _u.base.group.lgroup.content->group;
    return _u.base.group.sgroup.group;
}

int slk::msg_t::set_group (const char *group_)
{
    size_t length = strnlen (group_, SL_GROUP_MAX_LENGTH);

    return set_group (group_, length);
}

int slk::msg_t::set_group (const char *group_, size_t length_)
{
    if (length_ > SL_GROUP_MAX_LENGTH) {
        errno = EINVAL;
        return -1;
    }

    if (length_ > 14) {
        _u.base.group.lgroup.type = group_type_long;
        _u.base.group.lgroup.content =
          (long_group_t *) malloc (sizeof (long_group_t));
        slk_assert (_u.base.group.lgroup.content);
        new (&_u.base.group.lgroup.content->refcnt) slk::atomic_counter_t ();
        _u.base.group.lgroup.content->refcnt.set (1);
        strncpy (_u.base.group.lgroup.content->group, group_, length_);
        _u.base.group.lgroup.content->group[length_] = '\0';
    } else {
        strncpy (_u.base.group.sgroup.group, group_, length_);
        _u.base.group.sgroup.group[length_] = '\0';
    }

    return 0;
}

slk::atomic_counter_t *slk::msg_t::refcnt ()
{
    switch (_u.base.type) {
        case type_lmsg:
            return &_u.lmsg.content->refcnt;
        case type_zclmsg:
            return &_u.zclmsg.content->refcnt;
        default:
            slk_assert (false);
            return NULL;
    }
}
