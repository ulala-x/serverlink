/* SPDX-License-Identifier: MPL-2.0 */

#ifndef SL_MSG_HPP_INCLUDED
#define SL_MSG_HPP_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "../util/config.hpp"
#include "../util/err.hpp"
#include "../util/atomic_counter.hpp"
#include "../util/macros.hpp"

#define CMD_TYPE_MASK 0x1c

extern "C" {
typedef void (msg_free_fn) (void *data_, void *hint_);
}

namespace slk
{
class metadata_t;

#define SL_GROUP_MAX_LENGTH 255

class alignas(64) msg_t
{
  public:
    struct content_t {
        void *data;
        size_t size;
        msg_free_fn *ffn;
        void *hint;
        slk::atomic_counter_t refcnt;
    };

    enum { more = 1, routing_id = 64, shared = 128, command = 2 };
    enum { ping = 4, pong = 8, subscribe = 12, cancel = 16, close_cmd = 20 };

    bool check () const;
    int init ();
    int init (void *data_, size_t size_, msg_free_fn *ffn_, void *hint_, content_t *content_ = NULL);
    int init_size (size_t size_);
    int init_buffer (const void *buf_, size_t size_);
    int init_data (void *data_, size_t size_, msg_free_fn *ffn_, void *hint_);
    int init_external_storage (content_t *content_, void *data_, size_t size_, msg_free_fn *ffn_, void *hint_);
    int init_delimiter ();
    int init_subscribe (size_t size_, const unsigned char *topic_);
    int init_cancel (size_t size_, const unsigned char *topic_);
    int close ();
    int move (msg_t *src_);
    int copy (msg_t *src_);
    void *data ();
    const void *data () const { return const_cast<msg_t*>(this)->data(); }
    size_t size () const;
    unsigned char flags () const;
    void set_flags (unsigned char flags_);
    void reset_flags (unsigned char flags_);
    metadata_t *metadata () const;
    void set_metadata (metadata_t *metadata_);
    void reset_metadata ();
    bool is_routing_id () const;
    bool is_delimiter () const;
    bool is_credential () const { return false; }
    bool is_subscribe () const { return (_u.base.flags & CMD_TYPE_MASK) == subscribe; }
    bool is_cancel () const { return (_u.base.flags & CMD_TYPE_MASK) == cancel; }
    bool is_vsm () const;
    bool is_lmsg () const;
    bool is_zcmsg () const;
    uint32_t get_routing_id () const;
    int set_routing_id (uint32_t routing_id_);
    const char *group () const;
    int set_group (const char *group_);
    int set_group (const char *, size_t length_);
    void add_refs (int refs_);
    bool rm_refs (int refs_);
    void shrink (size_t new_size_);

    enum { msg_t_size = 64 };
    enum { max_vsm_size = msg_t_size - (sizeof (metadata_t *) + 3 + 16 + sizeof (uint32_t)) };

  private:
    slk::atomic_counter_t *refcnt ();
    enum type_t { type_min = 101, type_vsm = 101, type_lmsg = 102, type_delimiter = 103, type_cmsg = 104, type_zclmsg = 105 };
    
    enum group_type_t { group_type_short, group_type_long };
    struct group_t {
        unsigned char type;
        union {
            char sgroup[15];
            struct { atomic_counter_t *refcnt; char *content; } lgroup;
        } u;
    };

    union {
        struct { metadata_t *metadata; unsigned char unused[msg_t_size - (sizeof (metadata_t *) + 2 + sizeof (uint32_t) + sizeof (group_t))]; unsigned char type; unsigned char flags; uint32_t routing_id; group_t group; } base;
        struct { metadata_t *metadata; unsigned char data[max_vsm_size]; unsigned char size; unsigned char type; unsigned char flags; uint32_t routing_id; group_t group; } vsm;
        struct { metadata_t *metadata; content_t *content; unsigned char unused[msg_t_size - (sizeof (metadata_t *) + sizeof (content_t *) + 2 + sizeof (uint32_t) + sizeof (group_t))]; unsigned char type; unsigned char flags; uint32_t routing_id; group_t group; } lmsg;
    } _u;
};

inline int close_and_return (slk::msg_t *msg_, int echo_) {
    int rc = msg_->close (); (void)rc; return echo_;
}

} // namespace slk

#endif