/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Monitoring System */

#ifndef SL_HEARTBEAT_HPP_INCLUDED
#define SL_HEARTBEAT_HPP_INCLUDED

#include <cstdint>
#include <cstring>
#include "../msg/msg.hpp"
#include "../msg/blob.hpp"
#include "../util/macros.hpp"

namespace slk
{
// Application-level heartbeat protocol
// Uses internal message markers to distinguish from user messages

// Message type markers (5 bytes):
// PING: [0x00 0x53 0x4C 0x4B 0x50] "SLKP" (ServerLink Ping)
// PONG: [0x00 0x53 0x4C 0x4B 0x4F] "SLKO" (ServerLink pOng)

class heartbeat_t
{
  public:
    // Magic prefixes for heartbeat messages
    static const unsigned char PING_PREFIX[5];
    static const unsigned char PONG_PREFIX[5];
    // C++20: Use static constexpr for compile-time size constants
    static constexpr size_t PREFIX_SIZE = 5;
    static constexpr size_t TIMESTAMP_SIZE = 8;
    static constexpr size_t HEARTBEAT_MSG_SIZE = PREFIX_SIZE + TIMESTAMP_SIZE;

    // Create a PING message with current timestamp
    static bool create_ping (msg_t *msg, int64_t timestamp_us);

    // Create a PONG message (echo back the timestamp from PING)
    static bool create_pong (msg_t *msg, int64_t ping_timestamp_us);

    // Check if message is a PING
    static bool is_ping (const msg_t *msg);

    // Check if message is a PONG
    static bool is_pong (const msg_t *msg);

    // Check if message is any heartbeat message
    static bool is_heartbeat (const msg_t *msg);

    // Extract timestamp from PING message
    static int64_t extract_ping_timestamp (const msg_t *msg);

    // Extract timestamp from PONG message
    static int64_t extract_pong_timestamp (const msg_t *msg);

  private:
    // Helper: check if message starts with given prefix
    static bool has_prefix (const msg_t *msg, const unsigned char *prefix);

    // Helper: extract timestamp from message payload
    static int64_t extract_timestamp (const msg_t *msg);

    // Helper: encode timestamp to message
    static void encode_timestamp (unsigned char *buffer, int64_t timestamp_us);

    // Helper: decode timestamp from buffer
    static int64_t decode_timestamp (const unsigned char *buffer);
};

}

#endif
