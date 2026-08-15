#include <unity.h>

#include <flexseq/PatternStore.h>

using flexseq::PATTERN_PER_CHANNEL;
using flexseq::CHANNEL_COUNT;
using flexseq::PatternStore;

namespace {

void assert_pattern_identity(
    PatternStore& store,
    uint8_t channel,
    uint8_t patternIndex
) {
    auto* pattern = store.getPattern(channel, patternIndex);

    TEST_ASSERT_NOT_NULL(pattern);

    const uint8_t expectedLength =
        static_cast<uint8_t>(
            1 + ((channel * PATTERN_PER_CHANNEL + patternIndex) % 24)
        );

    TEST_ASSERT_TRUE(pattern->setBaseLength(expectedLength));

    const uint8_t stepIndex =
        static_cast<uint8_t>(
            (channel * PATTERN_PER_CHANNEL + patternIndex) % 24
        );

    TEST_ASSERT_TRUE(pattern->writeStep(stepIndex, true));
}

}  // namespace

void test_all_96_patterns_are_independently_addressable() {
    PatternStore store;

    for (uint8_t channel = 0; channel < CHANNEL_COUNT; ++channel) {
        for (
            uint8_t patternIndex = 0;
            patternIndex < PATTERN_PER_CHANNEL;
            ++patternIndex
        ) {
            assert_pattern_identity(store, channel, patternIndex);
        }
    }

    for (uint8_t channel = 0; channel < CHANNEL_COUNT; ++channel) {
        for (
            uint8_t patternIndex = 0;
            patternIndex < PATTERN_PER_CHANNEL;
            ++patternIndex
        ) {
            auto* pattern = store.getPattern(channel, patternIndex);

            TEST_ASSERT_NOT_NULL(pattern);

            const uint8_t linearIndex =
                static_cast<uint8_t>(
                    channel * PATTERN_PER_CHANNEL + patternIndex
                );

            const uint8_t expectedLength =
                static_cast<uint8_t>(1 + (linearIndex % 24));

            const uint8_t expectedStep =
                static_cast<uint8_t>(linearIndex % 24);

            TEST_ASSERT_EQUAL_UINT8(
                expectedLength,
                pattern->getBaseLength()
            );

            bool active = false;

            TEST_ASSERT_TRUE(
                pattern->readStep(expectedStep, active)
            );

            TEST_ASSERT_TRUE(active);
        }
    }
}

void test_all_96_patterns_keep_unmodified_steps_off() {
    PatternStore store;

    for (uint8_t channel = 0; channel < CHANNEL_COUNT; ++channel) {
        for (
            uint8_t patternIndex = 0;
            patternIndex < PATTERN_PER_CHANNEL;
            ++patternIndex
        ) {
            auto* pattern = store.getPattern(channel, patternIndex);

            TEST_ASSERT_NOT_NULL(pattern);

            const uint8_t linearIndex =
                static_cast<uint8_t>(
                    channel * PATTERN_PER_CHANNEL + patternIndex
                );

            const uint8_t activeStep =
                static_cast<uint8_t>(linearIndex % 24);

            TEST_ASSERT_TRUE(
                pattern->writeStep(activeStep, true)
            );
        }
    }

    for (uint8_t channel = 0; channel < CHANNEL_COUNT; ++channel) {
        for (
            uint8_t patternIndex = 0;
            patternIndex < PATTERN_PER_CHANNEL;
            ++patternIndex
        ) {
            auto* pattern = store.getPattern(channel, patternIndex);

            TEST_ASSERT_NOT_NULL(pattern);

            const uint8_t linearIndex =
                static_cast<uint8_t>(
                    channel * PATTERN_PER_CHANNEL + patternIndex
                );

            const uint8_t activeStep =
                static_cast<uint8_t>(linearIndex % 24);

            for (uint8_t step = 0; step < 24; ++step) {
                bool active = false;

                TEST_ASSERT_TRUE(
                    pattern->readStep(step, active)
                );

                if (step == activeStep) {
                    TEST_ASSERT_TRUE(active);
                } else {
                    TEST_ASSERT_FALSE(active);
                }
            }
        }
    }
}

void test_all_96_patterns_preserve_steps_outside_base_length() {
    PatternStore store;

    for (uint8_t channel = 0; channel < CHANNEL_COUNT; ++channel) {
        for (
            uint8_t patternIndex = 0;
            patternIndex < PATTERN_PER_CHANNEL;
            ++patternIndex
        ) {
            auto* pattern = store.getPattern(channel, patternIndex);

            TEST_ASSERT_NOT_NULL(pattern);

            TEST_ASSERT_TRUE(pattern->setBaseLength(8));

            TEST_ASSERT_TRUE(pattern->writeStep(23, true));

            TEST_ASSERT_TRUE(pattern->setBaseLength(3));

            bool active = false;

            TEST_ASSERT_TRUE(pattern->readStep(23, active));
            TEST_ASSERT_TRUE(active);

            TEST_ASSERT_EQUAL_UINT8(3, pattern->getBaseLength());
        }
    }
}

void test_all_96_patterns_can_be_cleared_independently() {
    PatternStore store;

    for (uint8_t channel = 0; channel < CHANNEL_COUNT; ++channel) {
        for (
            uint8_t patternIndex = 0;
            patternIndex < PATTERN_PER_CHANNEL;
            ++patternIndex
        ) {
            auto* pattern = store.getPattern(channel, patternIndex);

            TEST_ASSERT_NOT_NULL(pattern);

            TEST_ASSERT_TRUE(pattern->setBaseLength(12));
            TEST_ASSERT_TRUE(pattern->writeStep(0, true));
            TEST_ASSERT_TRUE(pattern->writeStep(11, true));
            TEST_ASSERT_TRUE(pattern->writeStep(23, true));
        }
    }

    auto* first = store.getPattern(0, 0);

    TEST_ASSERT_NOT_NULL(first);
    first->clear();

    bool active = false;

    TEST_ASSERT_TRUE(first->readStep(0, active));
    TEST_ASSERT_FALSE(active);

    TEST_ASSERT_TRUE(first->readStep(11, active));
    TEST_ASSERT_FALSE(active);

    TEST_ASSERT_TRUE(first->readStep(23, active));
    TEST_ASSERT_FALSE(active);

    TEST_ASSERT_EQUAL_UINT8(12, first->getBaseLength());

    auto* second = store.getPattern(0, 1);

    TEST_ASSERT_NOT_NULL(second);

    TEST_ASSERT_TRUE(second->readStep(0, active));
    TEST_ASSERT_TRUE(active);

    TEST_ASSERT_TRUE(second->readStep(11, active));
    TEST_ASSERT_TRUE(active);

    TEST_ASSERT_TRUE(second->readStep(23, active));
    TEST_ASSERT_TRUE(active);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_all_96_patterns_are_independently_addressable);
    RUN_TEST(test_all_96_patterns_keep_unmodified_steps_off);
    RUN_TEST(test_all_96_patterns_preserve_steps_outside_base_length);
    RUN_TEST(test_all_96_patterns_can_be_cleared_independently);

    return UNITY_END();
}