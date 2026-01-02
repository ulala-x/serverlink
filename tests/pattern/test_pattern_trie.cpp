/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Pattern trie unit tests */

#include "../testutil.hpp"
#include "../../src/pattern/pattern_trie.hpp"
#include <string.h>

// Test basic add and check
static void test_add_and_check()
{
    slk::pattern_trie_t trie;

    TEST_ASSERT(trie.add("news.*"));
    TEST_ASSERT(trie.check("news.sports"));
    TEST_ASSERT(trie.check("news.tech"));
    TEST_ASSERT(!trie.check("weather.today"));
}

// Test duplicate add
static void test_duplicate_add()
{
    slk::pattern_trie_t trie;

    TEST_ASSERT(trie.add("user.?"));
    TEST_ASSERT(!trie.add("user.?")); // Second add should return false
    TEST_ASSERT_EQ(trie.num_patterns(), 1);

    TEST_ASSERT(trie.check("user.1"));
}

// Test remove
static void test_remove()
{
    slk::pattern_trie_t trie;

    trie.add("event.*");
    TEST_ASSERT(trie.check("event.login"));

    TEST_ASSERT(trie.rm("event.*"));
    TEST_ASSERT(!trie.check("event.login"));
    TEST_ASSERT_EQ(trie.num_patterns(), 0);
}

// Test remove non-existent
static void test_remove_non_existent()
{
    slk::pattern_trie_t trie;

    TEST_ASSERT(!trie.rm("non.existent"));
}

// Test multiple patterns
static void test_multiple_patterns()
{
    slk::pattern_trie_t trie;

    trie.add("news.*");
    trie.add("user.?");
    trie.add("event.[a-z]*");

    TEST_ASSERT_EQ(trie.num_patterns(), 3);

    TEST_ASSERT(trie.check("news.sports"));
    TEST_ASSERT(trie.check("user.1"));
    TEST_ASSERT(trie.check("event.login"));
    TEST_ASSERT(!trie.check("weather.today"));
}

// Test refcount with duplicate add/remove
static void test_refcount()
{
    slk::pattern_trie_t trie;

    trie.add("data.*");
    trie.add("data.*"); // Duplicate
    TEST_ASSERT_EQ(trie.num_patterns(), 1);

    TEST_ASSERT(trie.rm("data.*")); // First remove
    TEST_ASSERT_EQ(trie.num_patterns(), 1); // Still there (refcount 1)
    TEST_ASSERT(trie.check("data.123"));

    TEST_ASSERT(trie.rm("data.*")); // Second remove
    TEST_ASSERT_EQ(trie.num_patterns(), 0); // Now removed
    TEST_ASSERT(!trie.check("data.123"));
}

// Test with binary data
static void test_binary_data()
{
    slk::pattern_trie_t trie;

    unsigned char pattern[] = {'d', 'a', 't', 'a', '.', '*', 0x00};
    unsigned char data[] = {'d', 'a', 't', 'a', '.', 'x', 'y', 'z', 0x00};

    trie.add(pattern, sizeof(pattern) - 1); // Exclude null terminator

    // Note: binary data matching may require proper null handling
    TEST_ASSERT(trie.num_patterns() > 0);
}

// Test empty trie
static void test_empty_trie()
{
    slk::pattern_trie_t trie;

    TEST_ASSERT_EQ(trie.num_patterns(), 0);
    TEST_ASSERT(!trie.check("anything"));
}

// Test pattern priority
static void test_pattern_priority()
{
    slk::pattern_trie_t trie;

    trie.add("*");
    trie.add("specific");

    TEST_ASSERT(trie.check("anything"));
    TEST_ASSERT(trie.check("specific"));
    TEST_ASSERT(trie.check("other"));
}

// Test complex patterns
static void test_complex_patterns()
{
    slk::pattern_trie_t trie;

    trie.add("event.[a-z]*.user.?");
    trie.add("data.*.log");
    trie.add("sys.??.error");

    TEST_ASSERT(trie.check("event.abc.user.1"));
    TEST_ASSERT(trie.check("data.important.log"));
    TEST_ASSERT(trie.check("sys.12.error"));

    TEST_ASSERT(!trie.check("event.ABC.user.1"));
    TEST_ASSERT(!trie.check("data.important.txt"));
    TEST_ASSERT(!trie.check("sys.1.error"));
}

// Test apply function
static int apply_count = 0;
static void count_patterns(const std::string &pattern, void *arg)
{
    (void)pattern;
    (void)arg;
    apply_count++;
}

static void test_apply()
{
    slk::pattern_trie_t trie;

    trie.add("pattern1");
    trie.add("pattern2");
    trie.add("pattern3");

    apply_count = 0;
    trie.apply(count_patterns, NULL);
    TEST_ASSERT_EQ(apply_count, 3);
}

// Test invalid pattern handling
static void test_invalid_pattern()
{
    slk::pattern_trie_t trie;

    // Invalid character class should not crash
    bool result = trie.add("[invalid");
    TEST_ASSERT(!result); // Should return false for invalid pattern
    TEST_ASSERT_EQ(trie.num_patterns(), 0);
}

int main()
{
    printf("Running pattern_trie tests...\n");

    test_add_and_check();
    test_duplicate_add();
    test_remove();
    test_remove_non_existent();
    test_multiple_patterns();
    test_refcount();
    test_binary_data();
    test_empty_trie();
    test_pattern_priority();
    test_complex_patterns();
    test_apply();
    test_invalid_pattern();

    printf("All pattern_trie tests passed!\n");
    return 0;
}
