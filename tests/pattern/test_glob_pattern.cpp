/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Glob pattern matching unit tests */

#include "../testutil.hpp"
#include "../../src/pattern/glob_pattern.hpp"
#include <string.h>

// Test basic literal matching
static void test_literal_match()
{
    slk::glob_pattern_t pattern("hello");

    TEST_ASSERT(pattern.match("hello"));
    TEST_ASSERT(!pattern.match("hallo"));
    TEST_ASSERT(!pattern.match("hello world"));
    TEST_ASSERT(!pattern.match("world"));
}

// Test star wildcard
static void test_star_wildcard()
{
    slk::glob_pattern_t pattern("news.*");

    TEST_ASSERT(pattern.match("news.sports"));
    TEST_ASSERT(pattern.match("news.tech"));
    TEST_ASSERT(pattern.match("news."));
    TEST_ASSERT(!pattern.match("news"));
    TEST_ASSERT(!pattern.match("sports.news"));
}

// Test star at beginning
static void test_star_at_beginning()
{
    slk::glob_pattern_t pattern("*.log");

    TEST_ASSERT(pattern.match("error.log"));
    TEST_ASSERT(pattern.match(".log"));
    TEST_ASSERT(pattern.match("debug.error.log"));
    TEST_ASSERT(!pattern.match("log"));
    TEST_ASSERT(!pattern.match("error.txt"));
}

// Test star matches empty string
static void test_star_empty()
{
    slk::glob_pattern_t pattern("a*b");

    TEST_ASSERT(pattern.match("ab"));
    TEST_ASSERT(pattern.match("axxxb"));
    TEST_ASSERT(pattern.match("a123b"));
    TEST_ASSERT(!pattern.match("a"));
    TEST_ASSERT(!pattern.match("b"));
}

// Test multiple stars
static void test_multiple_stars()
{
    slk::glob_pattern_t pattern("a*b*c");

    TEST_ASSERT(pattern.match("abc"));
    TEST_ASSERT(pattern.match("axbxc"));
    TEST_ASSERT(pattern.match("axxxbxxxc"));
    TEST_ASSERT(!pattern.match("ab"));
    TEST_ASSERT(!pattern.match("bc"));
}

// Test question mark wildcard
static void test_question_wildcard()
{
    slk::glob_pattern_t pattern("user.?");

    TEST_ASSERT(pattern.match("user.1"));
    TEST_ASSERT(pattern.match("user.a"));
    TEST_ASSERT(pattern.match("user.x"));
    TEST_ASSERT(!pattern.match("user."));
    TEST_ASSERT(!pattern.match("user.12"));
    TEST_ASSERT(!pattern.match("user"));
}

// Test multiple question marks
static void test_multiple_questions()
{
    slk::glob_pattern_t pattern("id.??");

    TEST_ASSERT(pattern.match("id.12"));
    TEST_ASSERT(pattern.match("id.ab"));
    TEST_ASSERT(!pattern.match("id.1"));
    TEST_ASSERT(!pattern.match("id.123"));
}

// Test character class basic
static void test_char_class_basic()
{
    slk::glob_pattern_t pattern("[abc]def");

    TEST_ASSERT(pattern.match("adef"));
    TEST_ASSERT(pattern.match("bdef"));
    TEST_ASSERT(pattern.match("cdef"));
    TEST_ASSERT(!pattern.match("ddef"));
    TEST_ASSERT(!pattern.match("xdef"));
}

// Test character class range
static void test_char_class_range()
{
    slk::glob_pattern_t pattern("id.[0-9]");

    TEST_ASSERT(pattern.match("id.0"));
    TEST_ASSERT(pattern.match("id.5"));
    TEST_ASSERT(pattern.match("id.9"));
    TEST_ASSERT(!pattern.match("id.a"));
    TEST_ASSERT(!pattern.match("id.A"));
}

// Test character class with multiple ranges
static void test_char_class_multiple_ranges()
{
    slk::glob_pattern_t pattern("[a-zA-Z]");

    TEST_ASSERT(pattern.match("a"));
    TEST_ASSERT(pattern.match("z"));
    TEST_ASSERT(pattern.match("A"));
    TEST_ASSERT(pattern.match("Z"));
    TEST_ASSERT(!pattern.match("0"));
    TEST_ASSERT(!pattern.match("-"));
}

// Test negated character class
static void test_char_class_negated()
{
    slk::glob_pattern_t pattern("[^0-9]");

    TEST_ASSERT(pattern.match("a"));
    TEST_ASSERT(pattern.match("Z"));
    TEST_ASSERT(pattern.match("_"));
    TEST_ASSERT(!pattern.match("0"));
    TEST_ASSERT(!pattern.match("9"));
}

// Test escape sequences
static void test_escape()
{
    slk::glob_pattern_t pattern("a\\*b");

    TEST_ASSERT(pattern.match("a*b"));
    TEST_ASSERT(!pattern.match("ab"));
    TEST_ASSERT(!pattern.match("axxxb"));
}

// Test complex pattern
static void test_complex_pattern()
{
    slk::glob_pattern_t pattern("event.[a-z]*.user.?");

    TEST_ASSERT(pattern.match("event.abc.user.1"));
    TEST_ASSERT(pattern.match("event.xyz123.user.x"));
    TEST_ASSERT(!pattern.match("event.ABC.user.1"));
    TEST_ASSERT(!pattern.match("event.abc.user"));
    TEST_ASSERT(!pattern.match("event.abc.user.12"));
}

// Test empty pattern
static void test_empty_pattern()
{
    slk::glob_pattern_t pattern("");

    TEST_ASSERT(pattern.match(""));
    TEST_ASSERT(!pattern.match("a"));
}

// Test wildcard only
static void test_wildcard_only()
{
    slk::glob_pattern_t pattern("*");

    TEST_ASSERT(pattern.match(""));
    TEST_ASSERT(pattern.match("anything"));
    TEST_ASSERT(pattern.match("123"));
}

// Test with binary data
static void test_binary_data()
{
    slk::glob_pattern_t pattern("data.*");

    unsigned char data1[] = {'d', 'a', 't', 'a', '.', 0x00, 0xFF};
    unsigned char data2[] = {'d', 'a', 't', 'a', 0x00};

    TEST_ASSERT(pattern.match(data1, sizeof(data1)));
    TEST_ASSERT(!pattern.match(data2, sizeof(data2)));
}

// Test invalid patterns
static void test_invalid_patterns()
{
    bool caught_exception = false;
    try {
        slk::glob_pattern_t pattern("[abc");
    } catch (const std::exception &e) {
        (void)e; // Suppress unused variable warning
        caught_exception = true;
    }
    TEST_ASSERT(caught_exception);
}

int main()
{
    printf("Running glob_pattern tests...\n");

    test_literal_match();
    test_star_wildcard();
    test_star_at_beginning();
    test_star_empty();
    test_multiple_stars();
    test_question_wildcard();
    test_multiple_questions();
    test_char_class_basic();
    test_char_class_range();
    test_char_class_multiple_ranges();
    test_char_class_negated();
    test_escape();
    test_complex_pattern();
    test_empty_pattern();
    test_wildcard_only();
    test_binary_data();
    test_invalid_patterns();

    printf("All glob_pattern tests passed!\n");
    return 0;
}
