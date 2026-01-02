/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Glob pattern matching implementation */

#include "glob_pattern.hpp"
#include <stdexcept>
#include <cstring>

slk::glob_pattern_t::glob_pattern_t () : _valid (false)
{
}

slk::glob_pattern_t::glob_pattern_t (const std::string &pattern) :
    _pattern (pattern), _valid (false)
{
    try {
        compile (pattern);
        _valid = true;
    } catch (const std::exception &) {
        _valid = false;
        throw;
    }
}

slk::glob_pattern_t::~glob_pattern_t ()
{
}

void slk::glob_pattern_t::compile (const std::string &pattern)
{
    _segments.clear ();

    for (size_t i = 0; i < pattern.size (); ++i) {
        unsigned char ch = static_cast<unsigned char> (pattern[i]);

        if (ch == '*') {
            segment_t seg;
            seg.type = STAR;
            _segments.push_back (seg);
        } else if (ch == '?') {
            segment_t seg;
            seg.type = QUESTION;
            _segments.push_back (seg);
        } else if (ch == '[') {
            segment_t seg;
            i = parse_char_class (pattern, i, seg);
            _segments.push_back (seg);
        } else if (ch == '\\' && i + 1 < pattern.size ()) {
            // Escape next character
            ++i;
            segment_t seg;
            seg.type = LITERAL;
            seg.literal_char = static_cast<unsigned char> (pattern[i]);
            _segments.push_back (seg);
        } else {
            // Literal character
            segment_t seg;
            seg.type = LITERAL;
            seg.literal_char = ch;
            _segments.push_back (seg);
        }
    }
}

size_t slk::glob_pattern_t::parse_char_class (const std::string &pattern,
                                               size_t i,
                                               segment_t &seg)
{
    seg.type = CHAR_CLASS;
    seg.negate = false;

    ++i; // Skip '['

    if (i >= pattern.size ()) {
        throw std::invalid_argument ("Unterminated character class");
    }

    // Check for negation
    if (pattern[i] == '^' || pattern[i] == '!') {
        seg.negate = true;
        ++i;
    }

    // Parse character class contents
    while (i < pattern.size () && pattern[i] != ']') {
        unsigned char ch = static_cast<unsigned char> (pattern[i]);

        // Check for range
        if (i + 2 < pattern.size () && pattern[i + 1] == '-'
            && pattern[i + 2] != ']') {
            unsigned char start = ch;
            unsigned char end = static_cast<unsigned char> (pattern[i + 2]);

            if (start > end) {
                throw std::invalid_argument ("Invalid character range");
            }

            seg.char_ranges.push_back (std::make_pair (start, end));
            i += 3;
        } else {
            // Single character
            seg.char_set.push_back (ch);
            ++i;
        }
    }

    if (i >= pattern.size ()) {
        throw std::invalid_argument ("Unterminated character class");
    }

    return i; // Returns position of ']'
}

bool slk::glob_pattern_t::match (const unsigned char *data, size_t size) const
{
    if (!_valid)
        return false;

    return match_impl (data, size, 0, 0);
}

bool slk::glob_pattern_t::match (const std::string &str) const
{
    return match (reinterpret_cast<const unsigned char *> (str.data ()),
                  str.size ());
}

bool slk::glob_pattern_t::match_impl (const unsigned char *data,
                                      size_t data_len,
                                      size_t data_pos,
                                      size_t seg_idx) const
{
    // End of pattern
    if (seg_idx >= _segments.size ()) {
        // Match only if we've consumed all data
        return data_pos >= data_len;
    }

    const segment_t &seg = _segments[seg_idx];

    switch (seg.type) {
    case STAR: {
        // Try matching zero or more characters
        // First, try matching zero characters (skip this segment)
        if (match_impl (data, data_len, data_pos, seg_idx + 1))
            return true;

        // Try matching one or more characters
        for (size_t i = data_pos; i < data_len; ++i) {
            if (match_impl (data, data_len, i + 1, seg_idx + 1))
                return true;
        }

        return false;
    }

    case QUESTION: {
        // Must have at least one character
        if (data_pos >= data_len)
            return false;

        // Match any single character and continue
        return match_impl (data, data_len, data_pos + 1, seg_idx + 1);
    }

    case CHAR_CLASS: {
        // Must have at least one character
        if (data_pos >= data_len)
            return false;

        unsigned char ch = data[data_pos];
        bool matched = char_class_match (ch, seg);

        if (!matched)
            return false;

        return match_impl (data, data_len, data_pos + 1, seg_idx + 1);
    }

    case LITERAL: {
        // Must have at least one character
        if (data_pos >= data_len)
            return false;

        if (data[data_pos] != seg.literal_char)
            return false;

        return match_impl (data, data_len, data_pos + 1, seg_idx + 1);
    }

    default:
        return false;
    }
}

bool slk::glob_pattern_t::char_class_match (unsigned char ch,
                                             const segment_t &seg) const
{
    bool matched = false;

    // Check character set
    for (size_t i = 0; i < seg.char_set.size (); ++i) {
        if (ch == seg.char_set[i]) {
            matched = true;
            break;
        }
    }

    // Check character ranges
    if (!matched) {
        for (size_t i = 0; i < seg.char_ranges.size (); ++i) {
            if (ch >= seg.char_ranges[i].first && ch <= seg.char_ranges[i].second) {
                matched = true;
                break;
            }
        }
    }

    // Apply negation if needed
    return seg.negate ? !matched : matched;
}
