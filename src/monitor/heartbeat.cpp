/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Monitoring System */

#include "../precompiled.hpp"
#include "heartbeat.hpp"
#include <cstring>

// Define the magic prefixes
// PING: [0x00 'S' 'L' 'K' 'P']
const unsigned char slk::heartbeat_t::PING_PREFIX[5] = {0x00, 0x53, 0x4C,
                                                          0x4B, 0x50};

// PONG: [0x00 'S' 'L' 'K' 'O']
const unsigned char slk::heartbeat_t::PONG_PREFIX[5] = {0x00, 0x53, 0x4C,
                                                          0x4B, 0x4F};

bool slk::heartbeat_t::create_ping (msg_t *msg, int64_t timestamp_us)
{
    if (!msg)
        return false;

    // Initialize message with appropriate size
    int rc = msg->init_size (HEARTBEAT_MSG_SIZE);
    if (rc != 0)
        return false;

    unsigned char *data = static_cast<unsigned char *> (msg->data ());

    // Copy PING prefix
    memcpy (data, PING_PREFIX, PREFIX_SIZE);

    // Encode timestamp
    encode_timestamp (data + PREFIX_SIZE, timestamp_us);

    return true;
}

bool slk::heartbeat_t::create_pong (msg_t *msg, int64_t ping_timestamp_us)
{
    if (!msg)
        return false;

    // Initialize message with appropriate size
    int rc = msg->init_size (HEARTBEAT_MSG_SIZE);
    if (rc != 0)
        return false;

    unsigned char *data = static_cast<unsigned char *> (msg->data ());

    // Copy PONG prefix
    memcpy (data, PONG_PREFIX, PREFIX_SIZE);

    // Encode timestamp (echo back the ping timestamp)
    encode_timestamp (data + PREFIX_SIZE, ping_timestamp_us);

    return true;
}

bool slk::heartbeat_t::is_ping (const msg_t *msg)
{
    return has_prefix (msg, PING_PREFIX);
}

bool slk::heartbeat_t::is_pong (const msg_t *msg)
{
    return has_prefix (msg, PONG_PREFIX);
}

bool slk::heartbeat_t::is_heartbeat (const msg_t *msg)
{
    return is_ping (msg) || is_pong (msg);
}

int64_t slk::heartbeat_t::extract_ping_timestamp (const msg_t *msg)
{
    if (!is_ping (msg))
        return 0;

    return extract_timestamp (msg);
}

int64_t slk::heartbeat_t::extract_pong_timestamp (const msg_t *msg)
{
    if (!is_pong (msg))
        return 0;

    return extract_timestamp (msg);
}

bool slk::heartbeat_t::has_prefix (const msg_t *msg,
                                    const unsigned char *prefix)
{
    if (!msg || !prefix)
        return false;

    if (msg->size () < HEARTBEAT_MSG_SIZE)
        return false;

    // msg_t::data() is non-const, so we need to cast away const
    const unsigned char *data = static_cast<const unsigned char *> (
        const_cast<msg_t *> (msg)->data ());

    return memcmp (data, prefix, PREFIX_SIZE) == 0;
}

int64_t slk::heartbeat_t::extract_timestamp (const msg_t *msg)
{
    if (!msg || msg->size () < HEARTBEAT_MSG_SIZE)
        return 0;

    // msg_t::data() is non-const, so we need to cast away const
    const unsigned char *data = static_cast<const unsigned char *> (
        const_cast<msg_t *> (msg)->data ());

    return decode_timestamp (data + PREFIX_SIZE);
}

void slk::heartbeat_t::encode_timestamp (unsigned char *buffer,
                                          int64_t timestamp_us)
{
    // Encode as big-endian 64-bit integer
    buffer[0] = static_cast<unsigned char> ((timestamp_us >> 56) & 0xFF);
    buffer[1] = static_cast<unsigned char> ((timestamp_us >> 48) & 0xFF);
    buffer[2] = static_cast<unsigned char> ((timestamp_us >> 40) & 0xFF);
    buffer[3] = static_cast<unsigned char> ((timestamp_us >> 32) & 0xFF);
    buffer[4] = static_cast<unsigned char> ((timestamp_us >> 24) & 0xFF);
    buffer[5] = static_cast<unsigned char> ((timestamp_us >> 16) & 0xFF);
    buffer[6] = static_cast<unsigned char> ((timestamp_us >> 8) & 0xFF);
    buffer[7] = static_cast<unsigned char> (timestamp_us & 0xFF);
}

int64_t slk::heartbeat_t::decode_timestamp (const unsigned char *buffer)
{
    // Decode from big-endian 64-bit integer
    int64_t timestamp = 0;

    timestamp |= static_cast<int64_t> (buffer[0]) << 56;
    timestamp |= static_cast<int64_t> (buffer[1]) << 48;
    timestamp |= static_cast<int64_t> (buffer[2]) << 40;
    timestamp |= static_cast<int64_t> (buffer[3]) << 32;
    timestamp |= static_cast<int64_t> (buffer[4]) << 24;
    timestamp |= static_cast<int64_t> (buffer[5]) << 16;
    timestamp |= static_cast<int64_t> (buffer[6]) << 8;
    timestamp |= static_cast<int64_t> (buffer[7]);

    return timestamp;
}
