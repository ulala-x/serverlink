/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include <string.h>
#include <utility>
#include <stdio.h>

#include "xpub.hpp"
#include "../pipe/pipe.hpp"
#include "../util/err.hpp"
#include "../msg/msg.hpp"
#include "../util/macros.hpp"
#include "../pipe/mtrie_impl.hpp"  // Required for template instantiation
#include "ctx.hpp"

slk::xpub_t::xpub_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    socket_base_t (parent_, tid_, sid_),
    _verbose_subs (false),
    _verbose_unsubs (false),
    _more_send (false),
    _more_recv (false),
    _process_subscribe (false),
    _only_first_subscribe (false),
    _lossy (true),
    _manual (false),
    _send_last_pipe (false),
    _pending_pipes (),
    _welcome_msg ()
{
    _last_pipe = NULL;
    options.type = SL_XPUB;
    _welcome_msg.init ();
}

slk::xpub_t::~xpub_t ()
{
    _welcome_msg.close ();
    for (std::deque<metadata_t *>::iterator it = _pending_metadata.begin (),
                                            end = _pending_metadata.end ();
         it != end; ++it)
        if (*it && (*it)->drop_ref ())
            SL_DELETE (*it);
}

void slk::xpub_t::xattach_pipe (pipe_t *pipe_,
                                bool subscribe_to_all_,
                                bool locally_initiated_)
{
    SL_UNUSED (locally_initiated_);

    slk_assert (pipe_);
    _dist.attach (pipe_);

    // If subscribe_to_all_ is specified, the caller would like to subscribe
    // to all data on this pipe, implicitly
    if (subscribe_to_all_)
        _subscriptions.add (NULL, 0, pipe_);

    // If welcome message exists, send a copy of it
    if (_welcome_msg.size () > 0) {
        msg_t copy;
        copy.init ();
        const int rc = copy.copy (_welcome_msg);
        errno_assert (rc == 0);
        const bool ok = pipe_->write (&copy);
        slk_assert (ok);
        pipe_->flush ();
    }

    // The pipe is active when attached. Let's read the subscriptions from
    // it, if any.
    xread_activated (pipe_);
}

void slk::xpub_t::xread_activated (pipe_t *pipe_)
{
    // There are some subscriptions waiting. Let's process them
    msg_t msg;
    while (pipe_->read (&msg)) {
        metadata_t *metadata = msg.metadata ();
        unsigned char *msg_data = static_cast<unsigned char *> (msg.data ()),
                      *data = NULL;
        size_t size = 0;
        bool subscribe = false;
        bool is_subscribe_or_cancel = false;
        bool notify = false;

        const bool first_part = !_more_recv;
        _more_recv = (msg.flags () & msg_t::more) != 0;

        if (first_part || _process_subscribe) {
            // Apply the subscription to the trie
            if (msg.is_subscribe () || msg.is_cancel ()) {
                data = static_cast<unsigned char *> (msg.command_body ());
                size = msg.command_body_size ();
                subscribe = msg.is_subscribe ();
                is_subscribe_or_cancel = true;
            } else if (msg.size () > 0 && (*msg_data == 0 || *msg_data == 1)) {
                data = msg_data + 1;
                size = msg.size () - 1;
                subscribe = *msg_data == 1;
                is_subscribe_or_cancel = true;
            }
        }

        if (first_part)
            _process_subscribe =
              !_only_first_subscribe || is_subscribe_or_cancel;

        if (is_subscribe_or_cancel) {
            if (_manual) {
                // Store manual subscription to use on termination
                if (!subscribe)
                    _manual_subscriptions.rm (data, size, pipe_);
                else
                    _manual_subscriptions.add (data, size, pipe_);

                _pending_pipes.push_back (pipe_);
            } else {
                if (!subscribe) {
                    const mtrie_t<pipe_t>::rm_result rm_result =
                      _subscriptions.rm (data, size, pipe_);
                    // TODO reconsider what to do if rm_result == mtrie_t::not_found
                    notify =
                      rm_result != mtrie_t<pipe_t>::values_remain || _verbose_unsubs;

                } else {
                    const bool first_added =
                      _subscriptions.add (data, size, pipe_);
                    notify = first_added || _verbose_subs;
                }
            }

            // If the request was a new subscription, or the subscription
            // was removed, or verbose mode or manual mode are enabled, store it
            // so that it can be passed to the user on next recv call
            if (_manual || (options.type == SL_XPUB && notify)) {
                // ZMTP 3.1 hack: we need to support sub/cancel commands, but
                // we can't give them back to userspace as it would be an API
                // breakage since the payload of the message is completely
                // different. Manually craft an old-style message instead.
                // Although with other transports it would be possible to simply
                // reuse the same buffer and prefix a 0/1 byte to the topic, with
                // inproc the subscribe/cancel command string is not present in
                // the message, so this optimization is not possible.
                // The pushback makes a copy of the data array anyway, so the
                // number of buffer copies does not change.
                blob_t notification (size + 1);
                if (subscribe)
                    *notification.data () = 1;
                else
                    *notification.data () = 0;
                memcpy (notification.data () + 1, data, size);

                _pending_data.push_back (std::move (notification));
                if (metadata)
                    metadata->add_ref ();
                _pending_metadata.push_back (metadata);
                _pending_flags.push_back (0);
            }
        } else if (options.type != SL_PUB) {
            // Process user message coming upstream from xsub socket,
            // but not if the type is PUB, which never processes user
            // messages
            _pending_data.push_back (blob_t (msg_data, msg.size ()));
            if (metadata)
                metadata->add_ref ();
            _pending_metadata.push_back (metadata);
            _pending_flags.push_back (msg.flags ());
        }

        msg.close ();
    }
}

void slk::xpub_t::xwrite_activated (pipe_t *pipe_)
{
    _dist.activated (pipe_);
}

int slk::xpub_t::xsetsockopt (int option_,
                              const void *optval_,
                              size_t optvallen_)
{
    if (option_ == SL_XPUB_VERBOSE || option_ == SL_XPUB_VERBOSER
        || option_ == SL_XPUB_MANUAL_LAST_VALUE || option_ == SL_XPUB_NODROP
        || option_ == SL_XPUB_MANUAL || option_ == SL_ONLY_FIRST_SUBSCRIBE) {
        if (optvallen_ != sizeof (int)
            || *static_cast<const int *> (optval_) < 0) {
            errno = EINVAL;
            return -1;
        }
        if (option_ == SL_XPUB_VERBOSE) {
            _verbose_subs = (*static_cast<const int *> (optval_) != 0);
            _verbose_unsubs = false;
        } else if (option_ == SL_XPUB_VERBOSER) {
            _verbose_subs = (*static_cast<const int *> (optval_) != 0);
            _verbose_unsubs = _verbose_subs;
        } else if (option_ == SL_XPUB_MANUAL_LAST_VALUE) {
            _manual = (*static_cast<const int *> (optval_) != 0);
            _send_last_pipe = _manual;
        } else if (option_ == SL_XPUB_NODROP)
            _lossy = (*static_cast<const int *> (optval_) == 0);
        else if (option_ == SL_XPUB_MANUAL)
            _manual = (*static_cast<const int *> (optval_) != 0);
        else if (option_ == SL_ONLY_FIRST_SUBSCRIBE)
            _only_first_subscribe = (*static_cast<const int *> (optval_) != 0);
    } else if (option_ == SL_SUBSCRIBE && _manual) {
        if (_last_pipe != NULL)
            _subscriptions.add ((unsigned char *) optval_, optvallen_,
                                _last_pipe);
    } else if (option_ == SL_UNSUBSCRIBE && _manual) {
        if (_last_pipe != NULL)
            _subscriptions.rm ((unsigned char *) optval_, optvallen_,
                               _last_pipe);
    } else if (option_ == SL_XPUB_WELCOME_MSG) {
        _welcome_msg.close ();

        if (optvallen_ > 0) {
            const int rc = _welcome_msg.init_size (optvallen_);
            errno_assert (rc == 0);

            unsigned char *data =
              static_cast<unsigned char *> (_welcome_msg.data ());
            memcpy (data, optval_, optvallen_);
        } else
            _welcome_msg.init ();
    } else {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int slk::xpub_t::xgetsockopt (int option_, void *optval_, size_t *optvallen_)
{
    if (option_ == SL_TOPICS_COUNT) {
        // Make sure to use a multi-thread safe function to avoid race conditions
        // with I/O threads where subscriptions are processed
        if (*optvallen_ < sizeof (int)) {
            errno = EINVAL;
            return -1;
        }

        // Process any pending commands (like 'bind' or 'activate_read') before
        // checking subscriptions. This ensures the subscription trie is up-to-date
        // with all messages that have been sent but not yet processed.
        //
        // NOTE: With the fix to fq_t::attach() removing the premature check_read()
        // call, the natural ypipe activation protocol should handle subscription
        // delivery correctly. However, we still need process_commands() here to
        // handle any pending attach/bind operations that may add new pipes.
        process_commands (0, false);

        // Return the current subscription count from the trie.
        *static_cast<int *> (optval_) =
            static_cast<int> (_subscriptions.num_prefixes ());
        *optvallen_ = sizeof (int);
        return 0;
    }

    if (option_ == SL_XPUB_NODROP) {
        if (*optvallen_ < sizeof (int)) {
            errno = EINVAL;
            return -1;
        }
        *static_cast<int *> (optval_) = _lossy ? 0 : 1;
        *optvallen_ = sizeof (int);
        return 0;
    }

    // Room for future options here

    errno = EINVAL;
    return -1;
}

// Stub function for rm without callback
static void stub (const unsigned char *data_, size_t size_, void *arg_)
{
    SL_UNUSED (data_);
    SL_UNUSED (size_);
    SL_UNUSED (arg_);
}

void slk::xpub_t::xpipe_terminated (pipe_t *pipe_)
{
    if (_manual) {
        // Remove the pipe from the trie and send corresponding manual
        // unsubscriptions upstream
        _manual_subscriptions.rm (pipe_, send_unsubscription, this, false);
        // Remove pipe without actually sending the message as it was taken
        // care of by the manual call above. subscriptions is the real mtrie,
        // so the pipe must be removed from there or it will be left over
        _subscriptions.rm (pipe_, stub, static_cast<void *> (NULL), false);

        // In case the pipe is currently set as last we must clear it to prevent
        // subscriptions from being re-added
        if (pipe_ == _last_pipe) {
            _last_pipe = NULL;
        }
    } else {
        // Remove the pipe from the trie. If there are topics that nobody
        // is interested in anymore, send corresponding unsubscriptions
        // upstream
        _subscriptions.rm (pipe_, send_unsubscription, this, !_verbose_unsubs);
    }

    _dist.pipe_terminated (pipe_);
}

void slk::xpub_t::mark_as_matching (pipe_t *pipe_, xpub_t *self_)
{
    self_->_dist.match (pipe_);
}

void slk::xpub_t::mark_last_pipe_as_matching (pipe_t *pipe_, xpub_t *self_)
{
    if (self_->_last_pipe == pipe_)
        self_->_dist.match (pipe_);
}

int slk::xpub_t::xsend (msg_t *msg_)
{
    const bool msg_more = (msg_->flags () & msg_t::more) != 0;

    // For the first part of multi-part message, find the matching pipes
    if (!_more_send) {
        // Ensure nothing from previous failed attempt to send is left matched
        _dist.unmatch ();

        if (_manual && _last_pipe && _send_last_pipe) {
            _subscriptions.match (static_cast<unsigned char *> (msg_->data ()),
                                  msg_->size (), mark_last_pipe_as_matching,
                                  this);
            _last_pipe = NULL;
        } else {
            _subscriptions.match (static_cast<unsigned char *> (msg_->data ()),
                                  msg_->size (), mark_as_matching, this);
        }
        // If inverted matching is used, reverse the selection now
        if (options.invert_matching) {
            _dist.reverse_match ();
        }
    }

    int rc = -1; // Assume we fail
    if (_lossy || _dist.check_hwm ()) {
        if (_dist.send_to_matching (msg_) == 0) {
            // If we are at the end of multi-part message we can mark
            // all the pipes as non-matching
            if (!msg_more)
                _dist.unmatch ();
            _more_send = msg_more;
            rc = 0; // Yay, sent successfully
        }
    } else
        errno = EAGAIN;
    return rc;
}

bool slk::xpub_t::xhas_out ()
{
    return _dist.has_out ();
}

int slk::xpub_t::xrecv (msg_t *msg_)
{
    // If there is at least one
    if (_pending_data.empty ()) {
        errno = EAGAIN;
        return -1;
    }

    // User is reading a message, set last_pipe and remove it from the deque
    if (_manual && !_pending_pipes.empty ()) {
        _last_pipe = _pending_pipes.front ();
        _pending_pipes.pop_front ();

        // If the distributor doesn't know about this pipe it must have already
        // been terminated and thus we can't allow manual subscriptions
        if (_last_pipe != NULL && !_dist.has_pipe (_last_pipe)) {
            _last_pipe = NULL;
        }
    }

    int rc = msg_->close ();
    errno_assert (rc == 0);
    rc = msg_->init_size (_pending_data.front ().size ());
    errno_assert (rc == 0);
    memcpy (msg_->data (), _pending_data.front ().data (),
            _pending_data.front ().size ());

    // Set metadata only if there is some
    if (metadata_t *metadata = _pending_metadata.front ()) {
        msg_->set_metadata (metadata);
        // Remove ref corresponding to vector placement
        metadata->drop_ref ();
    }

    msg_->set_flags (_pending_flags.front ());
    _pending_data.pop_front ();
    _pending_metadata.pop_front ();
    _pending_flags.pop_front ();
    return 0;
}

bool slk::xpub_t::xhas_in ()
{
    return !_pending_data.empty ();
}

void slk::xpub_t::send_unsubscription (const unsigned char *data_,
                                       size_t size_,
                                       xpub_t *self_)
{
    if (self_->options.type != SL_PUB) {
        // Place the unsubscription to the queue of pending (un)subscriptions
        // to be retrieved by the user later on
        blob_t unsub (size_ + 1);
        *unsub.data () = 0;
        if (size_ > 0)
            memcpy (unsub.data () + 1, data_, size_);
        self_->_pending_data.push_back (std::move (unsub));
        self_->_pending_metadata.push_back (NULL);
        self_->_pending_flags.push_back (0);

        if (self_->_manual) {
            self_->_last_pipe = NULL;
            self_->_pending_pipes.push_back (NULL);
        }
    }
}
