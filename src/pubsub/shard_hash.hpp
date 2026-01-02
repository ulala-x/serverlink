/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Redis-style Sharded Pub/Sub Hash */

#ifndef SL_SHARD_HASH_HPP_INCLUDED
#define SL_SHARD_HASH_HPP_INCLUDED

#include <cstdint>
#include <cstddef>
#include <string>

namespace slk
{

/**
 * CRC16 hash for Redis Cluster-compatible slot calculation
 *
 * This class provides CRC16-XMODEM hashing for determining which shard
 * a channel belongs to, using the same algorithm as Redis Cluster.
 *
 * Features:
 * - 16384 hash slots (Redis Cluster standard)
 * - Hash tag support: {tag}channel extracts "tag" for hashing
 * - Thread-safe: all methods are const or static
 * - Immutable: no state, purely functional
 */
class shard_hash_t
{
  public:
    // Redis Cluster standard: 16384 slots
    static constexpr int SLOT_COUNT = 16384;

    /**
     * Calculate CRC16-XMODEM hash
     *
     * This is the standard CRC16 algorithm used by Redis Cluster.
     * Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
     * Initial value: 0x0000
     *
     * @param data Pointer to data buffer
     * @param len Length of data in bytes
     * @return CRC16 hash value (0-65535)
     */
    static uint16_t crc16 (const char *data_, size_t len_);

    /**
     * Get hash slot for a channel name
     *
     * Calculates which slot (0-16383) a channel belongs to.
     * Supports hash tags: {tag}channel → hashes "tag" only
     *
     * @param channel Channel name
     * @return Slot number (0-16383)
     */
    static int get_slot (const std::string &channel_);

    /**
     * Extract hash tag from channel name
     *
     * Hash tags allow grouping related channels into the same slot.
     * Format: {tag}channel → returns "tag"
     *         channel       → returns "channel" (no tag)
     *
     * Rules:
     * - Tag must be enclosed in {}
     * - First {} pair is used
     * - Empty tags are ignored: {}channel → "channel"
     * - Nested tags not supported: {{tag}} → "tag"
     *
     * Examples:
     *   "{user}messages" → "user"
     *   "{room:1}chat"   → "room:1"
     *   "news"           → "news"
     *   "{}empty"        → "empty"
     *
     * @param channel Channel name with optional hash tag
     * @return Hash tag string (or full channel if no tag)
     */
    static std::string extract_hash_tag (const std::string &channel_);

  private:
    // CRC16 lookup table (generated at compile time if possible)
    static const uint16_t crc16_tab[256];

    // Initialize CRC16 lookup table
    static void init_crc16_table ();
};

} // namespace slk

#endif
