/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Pattern trie for efficient pattern matching */

#ifndef SL_PATTERN_TRIE_HPP_INCLUDED
#define SL_PATTERN_TRIE_HPP_INCLUDED

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "glob_pattern.hpp"
#include "../util/macros.hpp"

namespace slk
{
// Thread-safe trie for storing and matching glob patterns
// Uses std::shared_mutex for reader-writer lock pattern
// - Multiple threads can check patterns concurrently (shared lock)
// - Pattern add/remove operations use exclusive lock
class pattern_trie_t
{
  public:
    pattern_trie_t ();
    ~pattern_trie_t ();

    // Add a glob pattern to the trie
    // Returns true if pattern was newly added, false if it already existed
    // Thread-safe: uses exclusive lock
    bool add (const std::string &pattern);
    bool add (const unsigned char *pattern, size_t size);

    // Remove a glob pattern from the trie
    // Returns true if pattern was removed, false if it didn't exist
    // Thread-safe: uses exclusive lock
    bool rm (const std::string &pattern);
    bool rm (const unsigned char *pattern, size_t size);

    // Check if data matches any stored pattern
    // Returns true if at least one pattern matches
    // Thread-safe: uses shared lock (multiple readers allowed)
    bool check (const unsigned char *data, size_t size) const;
    bool check (const std::string &str) const;

    // Get number of patterns stored
    // Thread-safe: uses shared lock
    size_t num_patterns () const;

    // Apply a function to all patterns
    // Thread-safe: uses shared lock
    // func(pattern_string, arg)
    void apply (void (*func) (const std::string &pattern, void *arg),
                void *arg) const;

  private:
    // Pattern entry with compiled matcher
    struct pattern_entry_t
    {
        std::string pattern_str;
        std::unique_ptr<glob_pattern_t> matcher;
        uint32_t refcount; // For duplicate tracking

        pattern_entry_t (const std::string &pat);
    };

    // Storage for patterns
    std::vector<pattern_entry_t> _patterns;

    // Mutex for thread safety
    // Using regular mutex (C++11 compatible)
    // For C++17, we would use std::shared_mutex for better read concurrency
    mutable std::mutex _mutex;

    // Find pattern by string (caller must hold lock)
    std::vector<pattern_entry_t>::iterator find_pattern (const std::string &pattern);
    std::vector<pattern_entry_t>::const_iterator
    find_pattern (const std::string &pattern) const;

    SL_NON_COPYABLE_NOR_MOVABLE (pattern_trie_t)
};

}

#endif
