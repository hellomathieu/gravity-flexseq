#include <stdint.h>
#include <unity.h>

#include "burst_policy.h"
#include "harness_burst_limits.h"

using burst::ACCEPT_SINGLE_BURST;
using burst::ACCEPT_SPLIT;
using burst::MAX_BURSTS;
using burst::MAX_DETENTS_REQUEST;
using burst::NO_EMPIRICAL_LIMIT;
using burst::REFUSE_EMPIRICAL_LIMIT;
using burst::REFUSE_INVALID_REQUEST;
using burst::REFUSE_PHYSICAL_LIMIT;
using burst::Request;
using burst::Verdict;

namespace {

const uint8_t PHYSICAL = 12;

Request ask(uint8_t detents, uint8_t empirical, bool allowSplit)
{
    Request request;
    request.detents = detents;
    request.physicalLimit = PHYSICAL;
    request.empiricalLimit = empirical;
    request.allowSplit = allowSplit;
    return request;
}

uint8_t sumOf(const uint8_t* bursts, uint8_t count)
{
    uint8_t total = 0;
    for (uint8_t i = 0; i < count; ++i) {
        total = static_cast<uint8_t>(total + bursts[i]);
    }
    return total;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_the_physical_limit_alone_governs_an_unmeasured_target(void)
{
    Verdict verdict = burst::decide(ask(12, NO_EMPIRICAL_LIMIT, false));
    TEST_ASSERT_EQUAL(ACCEPT_SINGLE_BURST, verdict.decision);
    TEST_ASSERT_EQUAL_UINT8(12, verdict.effectiveLimit);

    verdict = burst::decide(ask(13, NO_EMPIRICAL_LIMIT, false));
    TEST_ASSERT_EQUAL(REFUSE_PHYSICAL_LIMIT, verdict.decision);
    TEST_ASSERT_EQUAL_UINT8(12, verdict.bindingLimit);
}

void test_an_empirical_limit_binds_before_the_physical_one(void)
{
    const Verdict verdict = burst::decide(ask(30, 6, false));
    TEST_ASSERT_EQUAL(REFUSE_EMPIRICAL_LIMIT, verdict.decision);
    TEST_ASSERT_EQUAL_UINT8(6, verdict.effectiveLimit);
    TEST_ASSERT_EQUAL_UINT8(6, verdict.bindingLimit);
}

void test_an_empirical_limit_above_the_physical_one_has_no_effect(void)
{
    const Verdict verdict = burst::decide(ask(13, 20, false));
    TEST_ASSERT_EQUAL(REFUSE_PHYSICAL_LIMIT, verdict.decision);
    TEST_ASSERT_EQUAL_UINT8(12, verdict.effectiveLimit);
}

void test_the_length_boundary_accepts_six_and_refuses_seven(void)
{
    Verdict verdict = burst::decide(ask(6, harness::LENGTH_BURST_LIMIT, false));
    TEST_ASSERT_EQUAL(ACCEPT_SINGLE_BURST, verdict.decision);

    verdict = burst::decide(ask(7, harness::LENGTH_BURST_LIMIT, false));
    TEST_ASSERT_EQUAL(REFUSE_EMPIRICAL_LIMIT, verdict.decision);
    TEST_ASSERT_EQUAL_UINT8(6, verdict.bindingLimit);
}

void test_the_length_limit_is_six(void)
{
    TEST_ASSERT_EQUAL_UINT8(6, harness::LENGTH_BURST_LIMIT);
}

void test_a_target_without_an_empirical_limit_is_never_capped_at_six(void)
{
    const uint8_t sizes[3] = { 8, 10, 12 };
    for (uint8_t i = 0; i < 3; ++i) {
        const Verdict verdict =
            burst::decide(ask(sizes[i], harness::SUBDIV_BURST_LIMIT, false));
        TEST_ASSERT_EQUAL(ACCEPT_SINGLE_BURST, verdict.decision);
        TEST_ASSERT_EQUAL_UINT8(12, verdict.effectiveLimit);
    }
}

void test_the_subdiv_target_carries_no_empirical_limit(void)
{
    TEST_ASSERT_EQUAL_UINT8(NO_EMPIRICAL_LIMIT, harness::SUBDIV_BURST_LIMIT);
}

void test_a_refusal_yields_no_burst_at_all(void)
{
    uint8_t bursts[MAX_BURSTS];
    for (uint8_t i = 0; i < MAX_BURSTS; ++i) {
        bursts[i] = 0xAA;
    }
    const Verdict verdict = burst::decide(ask(30, 6, false));
    TEST_ASSERT_EQUAL(REFUSE_EMPIRICAL_LIMIT, verdict.decision);

    const uint8_t count = burst::split(0, verdict.effectiveLimit, bursts, MAX_BURSTS);
    TEST_ASSERT_EQUAL_UINT8(0, count);
    TEST_ASSERT_EQUAL_UINT8(0xAA, bursts[0]);
}

void test_thirty_detents_split_into_five_bursts_of_six(void)
{
    uint8_t bursts[MAX_BURSTS];
    const Verdict verdict = burst::decide(ask(30, 6, true));
    TEST_ASSERT_EQUAL(ACCEPT_SPLIT, verdict.decision);

    const uint8_t count = burst::split(30, verdict.effectiveLimit, bursts, MAX_BURSTS);
    TEST_ASSERT_EQUAL_UINT8(5, count);
    for (uint8_t i = 0; i < count; ++i) {
        TEST_ASSERT_EQUAL_UINT8(6, bursts[i]);
    }
    TEST_ASSERT_EQUAL_UINT8(30, sumOf(bursts, count));
}

void test_a_remainder_becomes_a_last_short_burst(void)
{
    uint8_t bursts[MAX_BURSTS];
    uint8_t count = burst::split(7, 6, bursts, MAX_BURSTS);
    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT8(6, bursts[0]);
    TEST_ASSERT_EQUAL_UINT8(1, bursts[1]);

    count = burst::split(50, 12, bursts, MAX_BURSTS);
    TEST_ASSERT_EQUAL_UINT8(5, count);
    TEST_ASSERT_EQUAL_UINT8(12, bursts[0]);
    TEST_ASSERT_EQUAL_UINT8(2, bursts[4]);
    TEST_ASSERT_EQUAL_UINT8(50, sumOf(bursts, count));
}

void test_no_burst_of_a_split_exceeds_the_effective_limit(void)
{
    uint8_t bursts[MAX_BURSTS];
    for (uint8_t detents = 1; detents <= MAX_DETENTS_REQUEST; ++detents) {
        const uint8_t count = burst::split(detents, 6, bursts, MAX_BURSTS);
        TEST_ASSERT_TRUE(count > 0);
        TEST_ASSERT_EQUAL_UINT8(detents, sumOf(bursts, count));
        for (uint8_t i = 0; i < count; ++i) {
            TEST_ASSERT_TRUE(bursts[i] >= 1);
            TEST_ASSERT_TRUE(bursts[i] <= 6);
        }
    }
}

void test_splitting_happens_only_when_the_caller_asks_for_it(void)
{
    const Verdict refused = burst::decide(ask(30, 6, false));
    const Verdict allowed = burst::decide(ask(30, 6, true));

    TEST_ASSERT_EQUAL(REFUSE_EMPIRICAL_LIMIT, refused.decision);
    TEST_ASSERT_EQUAL(ACCEPT_SPLIT, allowed.decision);
    TEST_ASSERT_EQUAL_UINT8(refused.effectiveLimit, allowed.effectiveLimit);
}

void test_allowing_a_split_never_turns_a_limit_into_a_refusal(void)
{
    for (uint8_t detents = 1; detents <= MAX_DETENTS_REQUEST; ++detents) {
        Verdict verdict = burst::decide(ask(detents, 6, true));
        TEST_ASSERT_TRUE(verdict.decision == ACCEPT_SINGLE_BURST
                         || verdict.decision == ACCEPT_SPLIT);
        verdict = burst::decide(ask(detents, NO_EMPIRICAL_LIMIT, true));
        TEST_ASSERT_TRUE(verdict.decision == ACCEPT_SINGLE_BURST
                         || verdict.decision == ACCEPT_SPLIT);
    }
}

void test_a_malformed_request_is_refused_on_its_form(void)
{
    Verdict verdict = burst::decide(ask(0, 6, false));
    TEST_ASSERT_EQUAL(REFUSE_INVALID_REQUEST, verdict.decision);

    Request noPhysical = ask(4, 6, false);
    noPhysical.physicalLimit = 0;
    verdict = burst::decide(noPhysical);
    TEST_ASSERT_EQUAL(REFUSE_INVALID_REQUEST, verdict.decision);
}

void test_sixty_five_detents_are_refused_on_their_form_even_with_a_split(void)
{
    const Verdict verdict = burst::decide(ask(65, NO_EMPIRICAL_LIMIT, true));
    TEST_ASSERT_EQUAL(REFUSE_INVALID_REQUEST, verdict.decision);
    TEST_ASSERT_EQUAL_UINT8(0, verdict.bindingLimit);
}

void test_split_writes_nothing_when_the_output_is_too_small(void)
{
    uint8_t bursts[2];
    bursts[0] = 0xAA;
    bursts[1] = 0xAA;
    const uint8_t count = burst::split(30, 6, bursts, 2);
    TEST_ASSERT_EQUAL_UINT8(0, count);
    TEST_ASSERT_EQUAL_UINT8(0xAA, bursts[0]);
}

void test_only_the_length_field_index_carries_an_empirical_limit(void)
{
    TEST_ASSERT_EQUAL_UINT8(6, harness::limitForFieldIndex(1));
    TEST_ASSERT_EQUAL_UINT8(NO_EMPIRICAL_LIMIT, harness::limitForFieldIndex(0));
    TEST_ASSERT_EQUAL_UINT8(NO_EMPIRICAL_LIMIT, harness::limitForFieldIndex(2));
    TEST_ASSERT_EQUAL_UINT8(NO_EMPIRICAL_LIMIT, harness::limitForFieldIndex(3));
    TEST_ASSERT_EQUAL_UINT8(NO_EMPIRICAL_LIMIT, harness::limitForFieldIndex(4));
}

void test_an_unknown_field_index_never_invents_an_empirical_limit(void)
{
    for (uint8_t index = 5; index < 200; ++index) {
        TEST_ASSERT_EQUAL_UINT8(NO_EMPIRICAL_LIMIT, harness::limitForFieldIndex(index));
    }
}

void test_the_field_index_resolution_agrees_with_the_named_limits(void)
{
    TEST_ASSERT_EQUAL_UINT8(harness::LENGTH_BURST_LIMIT, harness::limitForFieldIndex(1));
    TEST_ASSERT_EQUAL_UINT8(harness::SUBDIV_BURST_LIMIT, harness::limitForFieldIndex(2));
    TEST_ASSERT_EQUAL_UINT8(harness::BAR_LENGTH_BURST_LIMIT, harness::limitForFieldIndex(3));
    TEST_ASSERT_EQUAL_UINT8(harness::EDIT_ENTRY_BURST_LIMIT, harness::limitForFieldIndex(4));
}

int main(int, char**)
{
    UNITY_BEGIN();

    RUN_TEST(test_only_the_length_field_index_carries_an_empirical_limit);
    RUN_TEST(test_an_unknown_field_index_never_invents_an_empirical_limit);
    RUN_TEST(test_the_field_index_resolution_agrees_with_the_named_limits);

    RUN_TEST(test_the_physical_limit_alone_governs_an_unmeasured_target);
    RUN_TEST(test_an_empirical_limit_binds_before_the_physical_one);
    RUN_TEST(test_an_empirical_limit_above_the_physical_one_has_no_effect);
    RUN_TEST(test_the_length_boundary_accepts_six_and_refuses_seven);
    RUN_TEST(test_the_length_limit_is_six);
    RUN_TEST(test_a_target_without_an_empirical_limit_is_never_capped_at_six);
    RUN_TEST(test_the_subdiv_target_carries_no_empirical_limit);
    RUN_TEST(test_a_refusal_yields_no_burst_at_all);
    RUN_TEST(test_thirty_detents_split_into_five_bursts_of_six);
    RUN_TEST(test_a_remainder_becomes_a_last_short_burst);
    RUN_TEST(test_no_burst_of_a_split_exceeds_the_effective_limit);
    RUN_TEST(test_splitting_happens_only_when_the_caller_asks_for_it);
    RUN_TEST(test_allowing_a_split_never_turns_a_limit_into_a_refusal);
    RUN_TEST(test_a_malformed_request_is_refused_on_its_form);
    RUN_TEST(test_sixty_five_detents_are_refused_on_their_form_even_with_a_split);
    RUN_TEST(test_split_writes_nothing_when_the_output_is_too_small);

    return UNITY_END();
}
