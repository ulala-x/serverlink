/* ServerLink Message Unit Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"
#include <string.h>

/* Test: Create and destroy empty message */
static void test_msg_create_destroy()
{
    slk_msg_t *msg = slk_msg_new();
    TEST_ASSERT_NOT_NULL(msg);

    /* Empty message should have size 0 */
    TEST_ASSERT_EQ(slk_msg_size(msg), 0);

    slk_msg_destroy(msg);
}

/* Test: Create message with data */
static void test_msg_create_with_data()
{
    const char *data = "Hello, World!";
    size_t size = strlen(data);

    slk_msg_t *msg = slk_msg_new_data(data, size);
    TEST_ASSERT_NOT_NULL(msg);

    /* Check size */
    TEST_ASSERT_EQ(slk_msg_size(msg), size);

    /* Check data */
    void *msg_data = slk_msg_data(msg);
    TEST_ASSERT_NOT_NULL(msg_data);
    TEST_ASSERT_MEM_EQ(msg_data, data, size);

    slk_msg_destroy(msg);
}

/* Test: Initialize message */
static void test_msg_init()
{
    /* Note: slk_msg_t is an opaque type, we use the new/destroy API */
    slk_msg_t *msg = slk_msg_new();
    TEST_ASSERT_NOT_NULL(msg);

    TEST_ASSERT_EQ(slk_msg_size(msg), 0);

    slk_msg_destroy(msg);
}

/* Test: Initialize message with data */
static void test_msg_init_data()
{
    const char *data = "Test data";
    size_t size = strlen(data);

    slk_msg_t *msg = slk_msg_new_data(data, size);
    TEST_ASSERT_NOT_NULL(msg);

    TEST_ASSERT_EQ(slk_msg_size(msg), size);

    void *msg_data = slk_msg_data(msg);
    TEST_ASSERT_NOT_NULL(msg_data);
    TEST_ASSERT_MEM_EQ(msg_data, data, size);

    slk_msg_destroy(msg);
}

/* Test: Copy message */
static void test_msg_copy()
{
    const char *data = "Copy test";
    size_t size = strlen(data);

    slk_msg_t *src = slk_msg_new_data(data, size);
    TEST_ASSERT_NOT_NULL(src);

    slk_msg_t *dst = slk_msg_new();
    TEST_ASSERT_NOT_NULL(dst);

    int rc = slk_msg_copy(dst, src);
    TEST_SUCCESS(rc);

    /* Both should have same size and data */
    TEST_ASSERT_EQ(slk_msg_size(dst), size);
    TEST_ASSERT_MEM_EQ(slk_msg_data(dst), data, size);

    /* Original should still be valid */
    TEST_ASSERT_EQ(slk_msg_size(src), size);
    TEST_ASSERT_MEM_EQ(slk_msg_data(src), data, size);

    slk_msg_destroy(src);
    slk_msg_destroy(dst);
}

/* Test: Move message */
static void test_msg_move()
{
    const char *data = "Move test";
    size_t size = strlen(data);

    slk_msg_t *src = slk_msg_new_data(data, size);
    TEST_ASSERT_NOT_NULL(src);

    slk_msg_t *dst = slk_msg_new();
    TEST_ASSERT_NOT_NULL(dst);

    int rc = slk_msg_move(dst, src);
    TEST_SUCCESS(rc);

    /* Destination should have the data */
    TEST_ASSERT_EQ(slk_msg_size(dst), size);
    TEST_ASSERT_MEM_EQ(slk_msg_data(dst), data, size);

    /* Source should be empty after move */
    TEST_ASSERT_EQ(slk_msg_size(src), 0);

    slk_msg_destroy(src);
    slk_msg_destroy(dst);
}

/* Test: Routing ID operations */
static void test_msg_routing_id()
{
    slk_msg_t *msg = slk_msg_new();
    TEST_ASSERT_NOT_NULL(msg);

    /* Routing IDs in ServerLink are uint32_t (not strings) */
    uint32_t routing_id = 0x12345678;

    /* Set routing ID */
    int rc = slk_msg_set_routing_id(msg, &routing_id, sizeof(uint32_t));
    TEST_SUCCESS(rc);

    /* Get routing ID */
    uint32_t retrieved_id = 0;
    size_t buffer_size = sizeof(uint32_t);
    rc = slk_msg_get_routing_id(msg, &retrieved_id, &buffer_size);
    TEST_SUCCESS(rc);

    TEST_ASSERT_EQ(buffer_size, sizeof(uint32_t));
    TEST_ASSERT_EQ(retrieved_id, routing_id);

    slk_msg_destroy(msg);
}

/* Test: Large message */
static void test_msg_large()
{
    const size_t large_size = 1024 * 1024; /* 1 MB */
    char *large_data = (char*)malloc(large_size);
    TEST_ASSERT_NOT_NULL(large_data);

    /* Fill with pattern */
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = (char)(i % 256);
    }

    slk_msg_t *msg = slk_msg_new_data(large_data, large_size);
    TEST_ASSERT_NOT_NULL(msg);

    TEST_ASSERT_EQ(slk_msg_size(msg), large_size);
    TEST_ASSERT_MEM_EQ(slk_msg_data(msg), large_data, large_size);

    slk_msg_destroy(msg);
    free(large_data);
}

/* Test: Zero-length message */
static void test_msg_zero_length()
{
    slk_msg_t *msg = slk_msg_new_data("", 0);
    TEST_ASSERT_NOT_NULL(msg);

    TEST_ASSERT_EQ(slk_msg_size(msg), 0);

    slk_msg_destroy(msg);
}

/* Test: Multiple operations on same message */
static void test_msg_reuse()
{
    /* Test creating, destroying, and recreating messages */
    slk_msg_t *msg1 = slk_msg_new_data("First", 5);
    TEST_ASSERT_NOT_NULL(msg1);
    TEST_ASSERT_EQ(slk_msg_size(msg1), 5);
    slk_msg_destroy(msg1);

    /* Create new message */
    slk_msg_t *msg2 = slk_msg_new_data("Second", 6);
    TEST_ASSERT_NOT_NULL(msg2);
    TEST_ASSERT_EQ(slk_msg_size(msg2), 6);
    TEST_ASSERT_MEM_EQ(slk_msg_data(msg2), "Second", 6);
    slk_msg_destroy(msg2);
}

int main()
{
    printf("=== ServerLink Message Unit Tests ===\n\n");

    RUN_TEST(test_msg_create_destroy);
    RUN_TEST(test_msg_create_with_data);
    RUN_TEST(test_msg_init);
    RUN_TEST(test_msg_init_data);
    RUN_TEST(test_msg_copy);
    RUN_TEST(test_msg_move);
    RUN_TEST(test_msg_routing_id);
    RUN_TEST(test_msg_large);
    RUN_TEST(test_msg_zero_length);
    RUN_TEST(test_msg_reuse);

    printf("\n=== All Message Tests Passed ===\n");
    return 0;
}
