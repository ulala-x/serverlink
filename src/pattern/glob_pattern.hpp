/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Glob pattern matching implementation */

#ifndef SL_GLOB_PATTERN_HPP_INCLUDED
#define SL_GLOB_PATTERN_HPP_INCLUDED

#include <string>
#include <vector>
#include <serverlink/serverlink_export.h>
#include "../util/macros.hpp"

namespace slk
{
// Glob pattern matcher for Redis-style pattern subscriptions
// Supports:
//   * - matches any sequence of characters
//   ? - matches exactly one character
//   [abc] - matches one character from the set
//   [a-z] - matches one character from the range
//   \x - escapes special characters
//
// Immutable design (no locks needed):
// - Pattern is compiled once during construction
// - All methods are const and thread-safe
class SL_EXPORT glob_pattern_t
{
  public:
    // Construct a glob pattern from a string
    // Throws std::invalid_argument if pattern is invalid
    explicit glob_pattern_t (const std::string &pattern);

    // Default constructor for empty pattern (matches nothing)
    glob_pattern_t ();

    ~glob_pattern_t ();

    // Check if data matches this pattern
    // Returns true if match, false otherwise
    bool match (const unsigned char *data, size_t size) const;
    bool match (const std::string &str) const;

    // Get the original pattern string
    const std::string &pattern () const { return _pattern; }

    // Check if pattern is valid (compiled successfully)
    bool is_valid () const { return _valid; }

  private:
    // Pattern segment types
    enum segment_type_t
    {
        LITERAL,      // Exact character match
        STAR,         // * wildcard (zero or more chars)
        QUESTION,     // ? wildcard (exactly one char)
        CHAR_CLASS    // [...] character class
    };

    // A segment of the compiled pattern
    struct segment_t
    {
        segment_type_t type;

        // For LITERAL: the character to match
        unsigned char literal_char;

        // For CHAR_CLASS: the set of matching characters
        std::vector<unsigned char> char_set;

        // For CHAR_CLASS with ranges: pairs of (start, end)
        std::vector<std::pair<unsigned char, unsigned char>> char_ranges;

        // If true, negate the character class [^...]
        bool negate;

        segment_t () : type (LITERAL), literal_char (0), negate (false) {}
    };

    // Compile the pattern string into segments
    void compile (const std::string &pattern);

    // Parse a character class [...] starting at position i
    // Returns the position after the closing ]
    size_t parse_char_class (const std::string &pattern, size_t i, segment_t &seg);

    // Match helper using backtracking
    bool match_impl (const unsigned char *data,
                     size_t data_len,
                     size_t data_pos,
                     size_t seg_idx) const;

    // Check if a character matches a character class segment
    bool char_class_match (unsigned char ch, const segment_t &seg) const;

    std::string _pattern;
    std::vector<segment_t> _segments;
    bool _valid;

    SL_NON_COPYABLE_NOR_MOVABLE (glob_pattern_t)
};

}

#endif
