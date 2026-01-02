/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Pattern trie for efficient pattern matching */

#include "pattern_trie.hpp"
#include <algorithm>
#include <cstring>

slk::pattern_trie_t::pattern_entry_t::pattern_entry_t (const std::string &pat) :
    pattern_str (pat), refcount (1)
{
    matcher.reset (new glob_pattern_t (pat));
}

slk::pattern_trie_t::pattern_trie_t ()
{
}

slk::pattern_trie_t::~pattern_trie_t ()
{
}

bool slk::pattern_trie_t::add (const std::string &pattern)
{
    std::lock_guard<std::mutex> lock (_mutex);

    // Check if pattern already exists
    auto it = find_pattern (pattern);
    if (it != _patterns.end ()) {
        // Pattern exists, increment refcount
        it->refcount++;
        return false;
    }

    // Add new pattern
    try {
        _patterns.push_back (pattern_entry_t (pattern));
        return true;
    } catch (const std::exception &) {
        // Pattern compilation failed
        return false;
    }
}

bool slk::pattern_trie_t::add (const unsigned char *pattern, size_t size)
{
    std::string pattern_str (reinterpret_cast<const char *> (pattern), size);
    return add (pattern_str);
}

bool slk::pattern_trie_t::rm (const std::string &pattern)
{
    std::lock_guard<std::mutex> lock (_mutex);

    auto it = find_pattern (pattern);
    if (it == _patterns.end ()) {
        return false;
    }

    // Decrement refcount
    it->refcount--;

    // Remove if refcount reaches zero
    if (it->refcount == 0) {
        _patterns.erase (it);
    }

    return true;
}

bool slk::pattern_trie_t::rm (const unsigned char *pattern, size_t size)
{
    std::string pattern_str (reinterpret_cast<const char *> (pattern), size);
    return rm (pattern_str);
}

bool slk::pattern_trie_t::check (const unsigned char *data, size_t size) const
{
    std::lock_guard<std::mutex> lock (_mutex);

    // Check against all patterns
    for (const auto &entry : _patterns) {
        if (entry.matcher && entry.matcher->match (data, size)) {
            return true;
        }
    }

    return false;
}

bool slk::pattern_trie_t::check (const std::string &str) const
{
    return check (reinterpret_cast<const unsigned char *> (str.data ()), str.size ());
}

size_t slk::pattern_trie_t::num_patterns () const
{
    std::lock_guard<std::mutex> lock (_mutex);
    return _patterns.size ();
}

void slk::pattern_trie_t::apply (
  void (*func) (const std::string &pattern, void *arg), void *arg) const
{
    std::lock_guard<std::mutex> lock (_mutex);

    for (const auto &entry : _patterns) {
        func (entry.pattern_str, arg);
    }
}

std::vector<slk::pattern_trie_t::pattern_entry_t>::iterator
slk::pattern_trie_t::find_pattern (const std::string &pattern)
{
    return std::find_if (_patterns.begin (), _patterns.end (),
                         [&pattern] (const pattern_entry_t &entry) {
                             return entry.pattern_str == pattern;
                         });
}

std::vector<slk::pattern_trie_t::pattern_entry_t>::const_iterator
slk::pattern_trie_t::find_pattern (const std::string &pattern) const
{
    return std::find_if (_patterns.begin (), _patterns.end (),
                         [&pattern] (const pattern_entry_t &entry) {
                             return entry.pattern_str == pattern;
                         });
}
