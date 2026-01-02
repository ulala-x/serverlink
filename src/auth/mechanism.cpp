/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "../precompiled.hpp"
#include <string.h>
#include <limits.h>

#include "mechanism.hpp"
#include "../core/options.hpp"
#include "../msg/msg.hpp"
#include "../util/err.hpp"
#include "../protocol/wire.hpp"

slk::mechanism_t::mechanism_t (const options_t &options_) : options (options_)
{
}

slk::mechanism_t::~mechanism_t ()
{
}

void slk::mechanism_t::set_peer_routing_id (const void *id_ptr_,
                                            size_t id_size_)
{
    _routing_id.set (static_cast<const unsigned char *> (id_ptr_), id_size_);
}

void slk::mechanism_t::peer_routing_id (msg_t *msg_)
{
    const int rc = msg_->init_size (_routing_id.size ());
    errno_assert (rc == 0);
    memcpy (msg_->data (), _routing_id.data (), _routing_id.size ());
    msg_->set_flags (msg_t::routing_id);
}

void slk::mechanism_t::set_user_id (const void *user_id_, size_t size_)
{
    _user_id.set (static_cast<const unsigned char *> (user_id_), size_);
    _zmtp_properties.insert (std::make_pair(
        std::string ("User-Id"),
        std::string (reinterpret_cast<const char *> (user_id_), size_)));
}

const slk::blob_t &slk::mechanism_t::get_user_id () const
{
    return _user_id;
}

const char socket_type_pair[] = "PAIR";
const char socket_type_pub[] = "PUB";
const char socket_type_sub[] = "SUB";
const char socket_type_router[] = "ROUTER";
const char socket_type_xpub[] = "XPUB";
const char socket_type_xsub[] = "XSUB";

const char *slk::mechanism_t::socket_type_string (int socket_type_)
{
    switch (socket_type_) {
        case SL_PAIR:
            return socket_type_pair;
        case SL_PUB:
            return socket_type_pub;
        case SL_SUB:
            return socket_type_sub;
        case SL_ROUTER:
            return socket_type_router;
        case SL_XPUB:
            return socket_type_xpub;
        case SL_XSUB:
            return socket_type_xsub;
        default:
            slk_assert (false);
            return "";
    }
}

const size_t name_len_size = sizeof (unsigned char);
const size_t value_len_size = sizeof (uint32_t);

static size_t property_len (size_t name_len_, size_t value_len_)
{
    return name_len_size + name_len_ + value_len_size + value_len_;
}

static size_t name_len (const char *name_)
{
    const size_t name_len = strlen (name_);
    slk_assert (name_len <= UCHAR_MAX);
    return name_len;
}

size_t slk::mechanism_t::add_property (unsigned char *ptr_,
                                       size_t ptr_capacity_,
                                       const char *name_,
                                       const void *value_,
                                       size_t value_len_)
{
    const size_t name_len = ::name_len (name_);
    const size_t total_len = ::property_len (name_len, value_len_);
    slk_assert (total_len <= ptr_capacity_);

    *ptr_ = static_cast<unsigned char> (name_len);
    ptr_ += name_len_size;
    memcpy (ptr_, name_, name_len);
    ptr_ += name_len;
    slk_assert (value_len_ <= 0x7FFFFFFF);
    put_uint32 (ptr_, static_cast<uint32_t> (value_len_));
    ptr_ += value_len_size;
    memcpy (ptr_, value_, value_len_);

    return total_len;
}

size_t slk::mechanism_t::property_len (const char *name_, size_t value_len_)
{
    return ::property_len (name_len (name_), value_len_);
}

#define ZMTP_PROPERTY_SOCKET_TYPE "Socket-Type"
#define ZMTP_PROPERTY_IDENTITY "Identity"

size_t slk::mechanism_t::add_basic_properties (unsigned char *ptr_,
                                               size_t ptr_capacity_) const
{
    unsigned char *ptr = ptr_;

    //  Add socket type property
    const char *socket_type = socket_type_string (options.type);
    ptr += add_property (ptr, ptr_capacity_, ZMTP_PROPERTY_SOCKET_TYPE,
                         socket_type, strlen (socket_type));

    //  Add identity (aka routing id) property for ROUTER socket
    if (options.type == SL_ROUTER) {
        ptr += add_property (ptr, ptr_capacity_ - (ptr - ptr_),
                             ZMTP_PROPERTY_IDENTITY, options.routing_id,
                             options.routing_id_size);
    }

    //  Add application metadata
    for (std::map<std::string, std::string>::const_iterator
           it = options.app_metadata.begin (),
           end = options.app_metadata.end ();
         it != end; ++it) {
        ptr +=
          add_property (ptr, ptr_capacity_ - (ptr - ptr_), it->first.c_str (),
                        it->second.c_str (), strlen (it->second.c_str ()));
    }

    return ptr - ptr_;
}

size_t slk::mechanism_t::basic_properties_len () const
{
    const char *socket_type = socket_type_string (options.type);
    size_t meta_len = 0;

    for (std::map<std::string, std::string>::const_iterator
           it = options.app_metadata.begin (),
           end = options.app_metadata.end ();
         it != end; ++it) {
        meta_len +=
          property_len (it->first.c_str (), strlen (it->second.c_str ()));
    }

    return property_len (ZMTP_PROPERTY_SOCKET_TYPE, strlen (socket_type))
           + meta_len
           + (options.type == SL_ROUTER
                ? property_len (ZMTP_PROPERTY_IDENTITY, options.routing_id_size)
                : 0);
}

void slk::mechanism_t::make_command_with_basic_properties (
  msg_t *msg_, const char *prefix_, size_t prefix_len_) const
{
    const size_t command_size = prefix_len_ + basic_properties_len ();
    const int rc = msg_->init_size (command_size);
    errno_assert (rc == 0);

    unsigned char *ptr = static_cast<unsigned char *> (msg_->data ());

    //  Add prefix
    memcpy (ptr, prefix_, prefix_len_);
    ptr += prefix_len_;

    add_basic_properties (
      ptr, command_size - (ptr - static_cast<unsigned char *> (msg_->data ())));
}

int slk::mechanism_t::parse_metadata (const unsigned char *ptr_,
                                      size_t length_)
{
    size_t bytes_left = length_;

    while (bytes_left > 1) {
        const size_t name_length = static_cast<size_t> (*ptr_);
        ptr_ += name_len_size;
        bytes_left -= name_len_size;
        if (bytes_left < name_length)
            break;

        const std::string name =
          std::string (reinterpret_cast<const char *> (ptr_), name_length);
        ptr_ += name_length;
        bytes_left -= name_length;
        if (bytes_left < value_len_size)
            break;

        const size_t value_length = static_cast<size_t> (get_uint32 (ptr_));
        ptr_ += value_len_size;
        bytes_left -= value_len_size;
        if (bytes_left < value_length)
            break;

        const uint8_t *value = ptr_;
        ptr_ += value_length;
        bytes_left -= value_length;

        if (name == ZMTP_PROPERTY_IDENTITY && options.recv_routing_id)
            set_peer_routing_id (value, value_length);
        else if (name == ZMTP_PROPERTY_SOCKET_TYPE) {
            if (!check_socket_type (reinterpret_cast<const char *> (value),
                                    value_length)) {
                errno = EINVAL;
                return -1;
            }
        } else {
            const int rc = property (name, value, value_length);
            if (rc == -1)
                return -1;
        }
        _zmtp_properties.insert (std::make_pair(
            name,
            std::string (reinterpret_cast<const char *> (value), value_length)));
    }
    if (bytes_left > 0) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int slk::mechanism_t::property (const std::string & /* name_ */,
                                const void * /* value_ */,
                                size_t /* length_ */)
{
    //  Default implementation does not check
    //  property values and returns 0 to signal success.
    return 0;
}

template <size_t N>
static bool strequals (const char *actual_type_,
                       const size_t actual_len_,
                       const char (&expected_type_)[N])
{
    return actual_len_ == N - 1
           && memcmp (actual_type_, expected_type_, N - 1) == 0;
}

bool slk::mechanism_t::check_socket_type (const char *type_,
                                          const size_t len_) const
{
    switch (options.type) {
        case SL_ROUTER:
            // ROUTER accepts DEALER, ROUTER, REQ
            // For now accept any peer type - validation at higher layers
            return true;
        case SL_PUB:
            return strequals (type_, len_, socket_type_sub)
                   || strequals (type_, len_, socket_type_xsub);
        case SL_SUB:
            return strequals (type_, len_, socket_type_pub)
                   || strequals (type_, len_, socket_type_xpub);
        case SL_XPUB:
            return strequals (type_, len_, socket_type_sub)
                   || strequals (type_, len_, socket_type_xsub);
        case SL_XSUB:
            return strequals (type_, len_, socket_type_pub)
                   || strequals (type_, len_, socket_type_xpub);
        default:
            return false;
    }
}
