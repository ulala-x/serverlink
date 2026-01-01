/* ServerLink XPUB VERBOSE/VERBOSER Tests */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "../testutil.hpp"

static const uint8_t unsubscribe_a_msg[] = {0, 'A'};
static const uint8_t subscribe_a_msg[] = {1, 'A'};
static const uint8_t subscribe_b_msg[] = {1, 'B'};

static const char test_endpoint[] = "inproc://test_xpub_verbose";
static const char topic_a[] = "A";
static const char topic_b[] = "B";

/* Helper: receive and verify subscription message */
static void recv_subscription(slk_socket_t *socket, const uint8_t *expected, size_t len)
{
    char buff[32];
    int rc = slk_recv(socket, buff, sizeof(buff), 0);
    TEST_ASSERT_EQ(rc, (int)len);
    TEST_ASSERT_MEM_EQ(buff, expected, len);
}

/* Helper: send string with success check */
static void send_string(slk_socket_t *socket, const char *str, int flags)
{
    int rc = slk_send(socket, str, strlen(str), flags);
    TEST_ASSERT(rc >= 0);
}

/* Helper: receive and verify string */
static void recv_string(slk_socket_t *socket, const char *expected, int flags)
{
    char buff[32];
    int rc = slk_recv(socket, buff, sizeof(buff), flags);
    TEST_ASSERT_EQ(rc, (int)strlen(expected));
    TEST_ASSERT_MEM_EQ(buff, expected, rc);
}

/* Test: XPUB_VERBOSE with one subscriber */
static void test_xpub_verbose_one_sub()
{
    slk_ctx_t *ctx = test_context_new();

    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);
    int rc = slk_bind(pub, test_endpoint);
    TEST_SUCCESS(rc);

    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub, test_endpoint);
    TEST_SUCCESS(rc);

    /* Subscribe for A */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Receive subscriptions from subscriber */
    recv_subscription(pub, subscribe_a_msg, sizeof(subscribe_a_msg));

    /* Subscribe socket for B instead */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, topic_b, 1);
    TEST_SUCCESS(rc);

    /* Receive subscriptions from subscriber */
    recv_subscription(pub, subscribe_b_msg, sizeof(subscribe_b_msg));

    /* Subscribe again for A */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* This time it is duplicated, so it will be filtered out */
    char buff[32];
    rc = slk_recv(pub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Enable VERBOSE mode */
    int verbose = 1;
    rc = slk_setsockopt(pub, SLK_XPUB_VERBOSE, &verbose, sizeof(int));
    TEST_SUCCESS(rc);

    /* Subscribe socket for A again */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* This time with VERBOSE the duplicated sub will be received */
    recv_subscription(pub, subscribe_a_msg, sizeof(subscribe_a_msg));

    /* Sending A message and B Message */
    send_string(pub, topic_a, 0);
    send_string(pub, topic_b, 0);

    recv_string(sub, topic_a, 0);
    recv_string(sub, topic_b, 0);

    /* Clean up */
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Helper: create XPUB with 2 subscribers */
static void create_xpub_with_2_subs(slk_ctx_t *ctx, slk_socket_t **pub,
                                     slk_socket_t **sub0, slk_socket_t **sub1)
{
    *pub = test_socket_new(ctx, SLK_XPUB);
    int rc = slk_bind(*pub, test_endpoint);
    TEST_SUCCESS(rc);

    *sub0 = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(*sub0, test_endpoint);
    TEST_SUCCESS(rc);

    *sub1 = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(*sub1, test_endpoint);
    TEST_SUCCESS(rc);
}

/* Helper: create duplicate subscription */
static void create_duplicate_subscription(slk_socket_t *pub, slk_socket_t *sub0,
                                          slk_socket_t *sub1)
{
    /* Subscribe for A */
    int rc = slk_setsockopt(sub0, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Receive subscriptions from subscriber */
    recv_subscription(pub, subscribe_a_msg, sizeof(subscribe_a_msg));

    /* Subscribe again for A on the other socket */
    rc = slk_setsockopt(sub1, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* This time it is duplicated, so it will be filtered out by XPUB */
    char buff[32];
    rc = slk_recv(pub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);
}

/* Test: XPUB_VERBOSE with two subscribers */
static void test_xpub_verbose_two_subs()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *pub, *sub0, *sub1;

    create_xpub_with_2_subs(ctx, &pub, &sub0, &sub1);
    create_duplicate_subscription(pub, sub0, sub1);

    /* Subscribe socket for B instead */
    int rc = slk_setsockopt(sub0, SLK_SUBSCRIBE, topic_b, 1);
    TEST_SUCCESS(rc);

    /* Receive subscriptions from subscriber */
    recv_subscription(pub, subscribe_b_msg, sizeof(subscribe_b_msg));

    /* Enable VERBOSE mode */
    int verbose = 1;
    rc = slk_setsockopt(pub, SLK_XPUB_VERBOSE, &verbose, sizeof(int));
    TEST_SUCCESS(rc);

    /* Subscribe socket for A again */
    rc = slk_setsockopt(sub1, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* This time with VERBOSE the duplicated sub will be received */
    recv_subscription(pub, subscribe_a_msg, sizeof(subscribe_a_msg));

    /* Sending A message and B Message */
    send_string(pub, topic_a, 0);
    send_string(pub, topic_b, 0);

    recv_string(sub0, topic_a, 0);
    recv_string(sub1, topic_a, 0);
    recv_string(sub0, topic_b, 0);

    /* Clean up */
    test_socket_close(pub);
    test_socket_close(sub0);
    test_socket_close(sub1);
    test_context_destroy(ctx);
}

/* Test: XPUB_VERBOSER with one subscriber */
static void test_xpub_verboser_one_sub()
{
    slk_ctx_t *ctx = test_context_new();

    /* Create a publisher */
    slk_socket_t *pub = test_socket_new(ctx, SLK_XPUB);
    int rc = slk_bind(pub, test_endpoint);
    TEST_SUCCESS(rc);

    /* Create a subscriber */
    slk_socket_t *sub = test_socket_new(ctx, SLK_SUB);
    rc = slk_connect(sub, test_endpoint);
    TEST_SUCCESS(rc);

    /* Unsubscribe for A, does not exist yet */
    rc = slk_setsockopt(sub, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Does not exist, so it will be filtered out by XSUB */
    char buff[32];
    rc = slk_recv(pub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Subscribe for A */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Receive subscriptions from subscriber */
    recv_subscription(pub, subscribe_a_msg, sizeof(subscribe_a_msg));

    /* Subscribe again for A, XSUB will increase refcount */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* This time it is duplicated, so it will be filtered out by XPUB */
    rc = slk_recv(pub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Unsubscribe for A, this time it exists in XPUB */
    rc = slk_setsockopt(sub, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* XSUB refcounts and will not actually send unsub to PUB until the number
     * of unsubs match the earlier subs */
    rc = slk_setsockopt(sub, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Receive unsubscriptions from subscriber */
    recv_subscription(pub, unsubscribe_a_msg, sizeof(unsubscribe_a_msg));

    /* XSUB only sends the last and final unsub, so XPUB will only receive 1 */
    rc = slk_recv(pub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Unsubscribe for A, does not exist anymore */
    rc = slk_setsockopt(sub, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Does not exist, so it will be filtered out by XSUB */
    rc = slk_recv(pub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Enable VERBOSER mode */
    int verbose = 1;
    rc = slk_setsockopt(pub, SLK_XPUB_VERBOSER, &verbose, sizeof(int));
    TEST_SUCCESS(rc);

    /* Subscribe socket for A again */
    rc = slk_setsockopt(sub, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Receive subscriptions from subscriber, did not exist anymore */
    recv_subscription(pub, subscribe_a_msg, sizeof(subscribe_a_msg));

    /* Sending A message to make sure everything still works */
    send_string(pub, topic_a, 0);
    recv_string(sub, topic_a, 0);

    /* Unsubscribe for A, this time it exists */
    rc = slk_setsockopt(sub, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Receive unsubscriptions from subscriber */
    recv_subscription(pub, unsubscribe_a_msg, sizeof(unsubscribe_a_msg));

    /* Unsubscribe for A again, it does not exist anymore so XSUB will filter */
    rc = slk_setsockopt(sub, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* XSUB only sends unsub if it matched it in its trie */
    rc = slk_recv(pub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Clean up */
    test_socket_close(pub);
    test_socket_close(sub);
    test_context_destroy(ctx);
}

/* Test: XPUB_VERBOSER with two subscribers */
static void test_xpub_verboser_two_subs()
{
    slk_ctx_t *ctx = test_context_new();
    slk_socket_t *pub, *sub0, *sub1;

    create_xpub_with_2_subs(ctx, &pub, &sub0, &sub1);
    create_duplicate_subscription(pub, sub0, sub1);

    /* Unsubscribe for A, this time it exists in XPUB */
    int rc = slk_setsockopt(sub0, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* sub1 is still subscribed, so no notification */
    char buff[32];
    rc = slk_recv(pub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Unsubscribe the second socket to trigger the notification */
    rc = slk_setsockopt(sub1, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Receive unsubscriptions since all sockets are gone */
    recv_subscription(pub, unsubscribe_a_msg, sizeof(unsubscribe_a_msg));

    /* Make really sure there is only one notification */
    rc = slk_recv(pub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Enable VERBOSER mode */
    int verbose = 1;
    rc = slk_setsockopt(pub, SLK_XPUB_VERBOSER, &verbose, sizeof(int));
    TEST_SUCCESS(rc);

    /* Subscribe socket for A again */
    rc = slk_setsockopt(sub0, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Subscribe socket for A again */
    rc = slk_setsockopt(sub1, SLK_SUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Receive subscriptions from subscriber, did not exist anymore */
    recv_subscription(pub, subscribe_a_msg, sizeof(subscribe_a_msg));

    /* VERBOSER is set, so subs from both sockets are received */
    recv_subscription(pub, subscribe_a_msg, sizeof(subscribe_a_msg));

    /* Sending A message to make sure everything still works */
    send_string(pub, topic_a, 0);

    recv_string(sub0, topic_a, 0);
    recv_string(sub1, topic_a, 0);

    /* Unsubscribe for A */
    rc = slk_setsockopt(sub1, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Receive unsubscriptions from first subscriber due to VERBOSER */
    recv_subscription(pub, unsubscribe_a_msg, sizeof(unsubscribe_a_msg));

    /* Unsubscribe for A again from the other socket */
    rc = slk_setsockopt(sub0, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Receive unsubscriptions from first subscriber due to VERBOSER */
    recv_subscription(pub, unsubscribe_a_msg, sizeof(unsubscribe_a_msg));

    /* Unsubscribe again to make sure it gets filtered now */
    rc = slk_setsockopt(sub1, SLK_UNSUBSCRIBE, topic_a, 1);
    TEST_SUCCESS(rc);

    /* Unmatched, so XSUB filters even with VERBOSER */
    rc = slk_recv(pub, buff, sizeof(buff), SLK_DONTWAIT);
    TEST_ASSERT(rc < 0);
    TEST_ASSERT_EQ(slk_errno(), SLK_EAGAIN);

    /* Clean up */
    test_socket_close(pub);
    test_socket_close(sub0);
    test_socket_close(sub1);
    test_context_destroy(ctx);
}

int main()
{
    printf("=== ServerLink XPUB VERBOSE/VERBOSER Tests ===\n\n");

    RUN_TEST(test_xpub_verbose_one_sub);
    RUN_TEST(test_xpub_verbose_two_subs);
    RUN_TEST(test_xpub_verboser_one_sub);
    RUN_TEST(test_xpub_verboser_two_subs);

    printf("\n=== All XPUB VERBOSE Tests Passed ===\n");
    return 0;
}
