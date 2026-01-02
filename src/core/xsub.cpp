/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include <string.h>

#include "../util/macros.hpp"
#include "../util/constants.hpp"
#include "xsub.hpp"
#include "../util/err.hpp"

slk::xsub_t::xsub_t (class ctx_t *parent_, uint32_t tid_, int sid_) :
    socket_base_t (parent_, tid_, sid_),
    _verbose_unsubs (false),
    _has_message (false),
    _more_send (false),
    _more_recv (false),
    _process_subscribe (false),
    _only_first_subscribe (false)
{
    options.type = SL_XSUB;

    // When socket is being closed down we don't want to wait till pending
    // subscription commands are sent to the wire
    options.linger.store (0);

    const int rc = _message.init ();
    errno_assert (rc == 0);
}

slk::xsub_t::~xsub_t ()
{
    const int rc = _message.close ();
    errno_assert (rc == 0);
}

void slk::xsub_t::xattach_pipe (pipe_t *pipe_,
                                bool subscribe_to_all_,
                                bool locally_initiated_)
{
    SL_UNUSED (subscribe_to_all_);
    SL_UNUSED (locally_initiated_);

    slk_assert (pipe_);
    _fq.attach (pipe_);
    _dist.attach (pipe_);

    // Send all the cached subscriptions to the new upstream peer
    _subscriptions.apply (send_subscription, pipe_);
    pipe_->flush ();
}

void slk::xsub_t::xread_activated (pipe_t *pipe_)
{
    _fq.activated (pipe_);
}

void slk::xsub_t::xwrite_activated (pipe_t *pipe_)
{
    _dist.activated (pipe_);
}

void slk::xsub_t::xpipe_terminated (pipe_t *pipe_)
{
    _fq.pipe_terminated (pipe_);
    _dist.pipe_terminated (pipe_);
}

void slk::xsub_t::xhiccuped (pipe_t *pipe_)
{
    // Send all the cached subscriptions to the hiccuped pipe
    _subscriptions.apply (send_subscription, pipe_);
    pipe_->flush ();
}

int slk::xsub_t::xsetsockopt (int option_,
                              const void *optval_,
                              size_t optvallen_)
{
    if (option_ == SL_ONLY_FIRST_SUBSCRIBE) {
        if (optvallen_ != sizeof (int)
            || *static_cast<const int *> (optval_) < 0) {
            errno = EINVAL;
            return -1;
        }
        _only_first_subscribe = (*static_cast<const int *> (optval_) != 0);
        return 0;
    }
    else if (option_ == SL_XSUB_VERBOSE_UNSUBSCRIBE) {
        if (optvallen_ != sizeof (int)
            || *static_cast<const int *> (optval_) < 0) {
            errno = EINVAL;
            return -1;
        }
        _verbose_unsubs = (*static_cast<const int *> (optval_) != 0);
        return 0;
    }
    else if (option_ == SL_PSUBSCRIBE) {
        // Add pattern subscription
        const unsigned char *pattern = static_cast<const unsigned char *> (optval_);
        bool added = _pattern_subscriptions.add (pattern, optvallen_);

        // When first pattern is added, subscribe to empty prefix (receive all messages)
        // This ensures we receive messages for pattern matching
        if (added && _pattern_subscriptions.num_patterns () == 1) {
            bool first_sub = _subscriptions.add (NULL, 0);
            if (first_sub) {
                // Send subscription message for empty prefix to all upstream peers
                msg_t msg;
                int rc = msg.init_subscribe (0, NULL);
                errno_assert (rc == 0);
                _dist.send_to_all (&msg);
                msg.close ();
            }
        }

        return 0;
    }
    else if (option_ == SL_PUNSUBSCRIBE) {
        // Remove pattern subscription
        const unsigned char *pattern = static_cast<const unsigned char *> (optval_);
        bool removed = _pattern_subscriptions.rm (pattern, optvallen_);

        // When last pattern is removed, unsubscribe from empty prefix
        if (removed && _pattern_subscriptions.num_patterns () == 0) {
            bool last_unsub = _subscriptions.rm (NULL, 0);
            if (last_unsub) {
                // Send unsubscription message for empty prefix to all upstream peers
                msg_t msg;
                int rc = msg.init_cancel (0, NULL);
                errno_assert (rc == 0);
                _dist.send_to_all (&msg);
                msg.close ();
            }
        }

        return 0;
    }
    errno = EINVAL;
    return -1;
}

int slk::xsub_t::xgetsockopt (int option_, void *optval_, size_t *optvallen_)
{
    if (option_ == SL_TOPICS_COUNT) {
        // Make sure to use a multi-thread safe function to avoid race conditions
        // with I/O threads where subscriptions are processed
        uint64_t num_subscriptions = _subscriptions.num_prefixes ();

        if (*optvallen_ < sizeof (int)) {
            errno = EINVAL;
            return -1;
        }
        *static_cast<int *> (optval_) = static_cast<int> (num_subscriptions);
        *optvallen_ = sizeof (int);
        return 0;
    }

    // Room for future options here

    errno = EINVAL;
    return -1;
}

int slk::xsub_t::xsend (msg_t *msg_)
{
    size_t size = msg_->size ();
    unsigned char *data = static_cast<unsigned char *> (msg_->data ());

    const bool first_part = !_more_send;
    _more_send = (msg_->flags () & msg_t::more) != 0;

    if (first_part) {
        _process_subscribe = !_only_first_subscribe;
    } else if (!_process_subscribe) {
        // User message sent upstream to XPUB socket
        return _dist.send_to_all (msg_);
    }

    if (msg_->is_subscribe () || (size > 0 && *data == 1)) {
        // Process subscribe message
        // This used to filter out duplicate subscriptions,
        // however this is already done on the XPUB side and
        // doing it here as well breaks SLK_XPUB_VERBOSE
        // when there are forwarding devices involved
        if (!msg_->is_subscribe ()) {
            data = data + 1;
            size = size - 1;
        }
        _subscriptions.add (data, size);
        _process_subscribe = true;
        return _dist.send_to_all (msg_);
    }
    if (msg_->is_cancel () || (size > 0 && *data == 0)) {
        // Process unsubscribe message
        if (!msg_->is_cancel ()) {
            data = data + 1;
            size = size - 1;
        }
        _process_subscribe = true;
        const bool rm_result = _subscriptions.rm (data, size);
        if (rm_result || _verbose_unsubs)
            return _dist.send_to_all (msg_);
    } else
        // User message sent upstream to XPUB socket
        return _dist.send_to_all (msg_);

    int rc = msg_->close ();
    errno_assert (rc == 0);
    rc = msg_->init ();
    errno_assert (rc == 0);

    return 0;
}

bool slk::xsub_t::xhas_out ()
{
    // Subscription can be added/removed anytime
    return true;
}

int slk::xsub_t::xrecv (msg_t *msg_)
{
    // If there's already a message prepared by a previous call to poll,
    // return it straight ahead
    if (_has_message) {
        const int rc = msg_->move (_message);
        errno_assert (rc == 0);
        _has_message = false;
        _more_recv = (msg_->flags () & msg_t::more) != 0;
        return 0;
    }

    // TODO: This can result in infinite loop in the case of continuous
    // stream of non-matching messages which breaks the non-blocking recv
    // semantics
    while (true) {
        // Get a message using fair queueing algorithm
        int rc = _fq.recv (msg_);

        // If there's no message available, return immediately
        // The same when error occurs
        if (rc != 0)
            return -1;

        // Check whether the message matches at least one subscription
        // Non-initial parts of the message are passed
        if (_more_recv || !options.filter || match (msg_)) {
            _more_recv = (msg_->flags () & msg_t::more) != 0;
            return 0;
        }

        // Message doesn't match. Pop any remaining parts of the message
        // from the pipe
        while (msg_->flags () & msg_t::more) {
            rc = _fq.recv (msg_);
            errno_assert (rc == 0);
        }
    }
}

bool slk::xsub_t::xhas_in ()
{
    // There are subsequent parts of the partly-read message available
    if (_more_recv)
        return true;

    // If there's already a message prepared by a previous call to poll,
    // return straight ahead
    if (_has_message)
        return true;

    // TODO: This can result in infinite loop in the case of continuous
    // stream of non-matching messages
    while (true) {
        // Get a message using fair queueing algorithm
        int rc = _fq.recv (&_message);

        // If there's no message available, return immediately
        // The same when error occurs
        if (rc != 0) {
            errno_assert (errno == EAGAIN);
            return false;
        }

        // Check whether the message matches at least one subscription
        if (!options.filter || match (&_message)) {
            _has_message = true;
            return true;
        }

        // Message doesn't match. Pop any remaining parts of the message
        // from the pipe
        while (_message.flags () & msg_t::more) {
            rc = _fq.recv (&_message);
            errno_assert (rc == 0);
        }
    }
}

bool slk::xsub_t::match (msg_t *msg_)
{
    bool has_patterns = (_pattern_subscriptions.num_patterns () > 0);

    // Check prefix-based subscriptions first
    bool prefix_match = _subscriptions.check (
      static_cast<unsigned char *> (msg_->data ()), msg_->size ());

    // Check pattern subscriptions
    bool pattern_match = false;
    if (has_patterns) {
        pattern_match = _pattern_subscriptions.check (
          static_cast<unsigned char *> (msg_->data ()), msg_->size ());
    }

    // If we have pattern subscriptions and this message matches the empty prefix
    // (which we subscribe to for pattern matching), only accept if it matches a pattern
    bool matching;
    if (has_patterns && prefix_match) {
        // Check if this is matching due to empty prefix subscription
        // by checking if there are any non-empty prefix subscriptions
        bool has_non_empty_prefixes = (_subscriptions.num_prefixes () > 1);

        if (has_non_empty_prefixes) {
            // We have both prefix and pattern subscriptions
            // Accept if either matches
            matching = prefix_match || pattern_match;
        } else {
            // Only have empty prefix (for patterns)
            // Accept only if pattern matches
            matching = pattern_match;
        }
    } else {
        // No patterns, or no prefix match
        // Standard behavior: accept if prefix matches OR pattern matches
        matching = prefix_match || pattern_match;
    }

    return matching ^ options.invert_matching;
}

void slk::xsub_t::send_subscription (unsigned char *data_,
                                     size_t size_,
                                     void *arg_)
{
    pipe_t *pipe = static_cast<pipe_t *> (arg_);

    // Create the subscription message
    msg_t msg;
    const int rc = msg.init_subscribe (size_, data_);
    errno_assert (rc == 0);

    // Send it to the pipe
    const bool sent = pipe->write (&msg);
    // If we reached the SNDHWM, and thus cannot send the subscription, drop
    // the subscription message instead. This matches the behaviour of
    // slk_setsockopt(SLK_SUBSCRIBE, ...), which also drops subscriptions
    // when the SNDHWM is reached
    if (!sent)
        msg.close ();
}
