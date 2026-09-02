#include <unity.h>

#include <flexseq/LengthCv.h>
#include <flexseq/SequencerEngine.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef FLEXSEQ_VECTORS
#error "FLEXSEQ_VECTORS must name the golden vector file (see platformio.ini)"
#endif

namespace {

struct Vector {
    char family;
    std::string id;
    bool hasA, hasB, hasC;
    int a, b, c, expected;
};

std::vector<Vector> vectors;
std::string loadError;

bool field(const std::string& raw, int& out, bool& present) {
    if (raw == "-") {
        present = false;
        out = 0;
        return true;
    }
    if (raw.empty()) {
        return false;
    }
    const size_t start = (raw[0] == '-' || raw[0] == '+') ? 1u : 0u;
    if (start >= raw.size()) {
        return false;
    }
    for (size_t i = start; i < raw.size(); ++i) {
        if (raw[i] < '0' || raw[i] > '9') {
            return false;
        }
    }
    present = true;
    out = std::atoi(raw.c_str());
    return true;
}

bool load() {
    std::ifstream in(FLEXSEQ_VECTORS);
    if (!in.is_open()) {
        loadError = "vector file cannot be opened";
        return false;
    }
    std::string line;
    if (!std::getline(in, line)) {
        loadError = "vector file is empty";
        return false;
    }
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream ls(line);
        std::string cell[6];
        size_t count = 0;
        while (count < 6 && std::getline(ls, cell[count], '\t')) {
            ++count;
        }
        std::string extra;
        if (count != 6 || std::getline(ls, extra, '\t')) {
            loadError = "line does not hold exactly six fields: " + line;
            return false;
        }
        Vector v;
        if (cell[0].size() != 1) {
            loadError = "family is not one character: " + line;
            return false;
        }
        v.family = cell[0][0];
        if (v.family != 'A' && v.family != 'L' && v.family != 'P' && v.family != 'S') {
            loadError = "unknown family: " + line;
            return false;
        }
        v.id = cell[1];
        bool expectedPresent = false;
        if (!field(cell[2], v.a, v.hasA) || !field(cell[3], v.b, v.hasB)
            || !field(cell[4], v.c, v.hasC)
            || !field(cell[5], v.expected, expectedPresent) || !expectedPresent) {
            loadError = "malformed number: " + line;
            return false;
        }
        const bool needsC = (v.family != 'A');
        if (!v.hasA || !v.hasB || (needsC && !v.hasC)) {
            loadError = "a needed column holds '-': " + line;
            return false;
        }
        vectors.push_back(v);
    }
    if (vectors.empty()) {
        loadError = "vector file holds no case";
        return false;
    }
    return true;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_the_vector_file_is_loaded_and_holds_cases() {
    TEST_ASSERT_TRUE_MESSAGE(loadError.empty(), loadError.c_str());
    TEST_ASSERT_TRUE(vectors.size() >= 40);
}

void test_every_family_is_represented() {
    size_t a = 0, l = 0, p = 0, s = 0;
    for (size_t i = 0; i < vectors.size(); ++i) {
        switch (vectors[i].family) {
            case 'A': ++a; break;
            case 'L': ++l; break;
            case 'P': ++p; break;
            case 'S': ++s; break;
            default: break;
        }
    }
    TEST_ASSERT_TRUE(a > 0);
    TEST_ASSERT_TRUE(l > 0);
    TEST_ASSERT_TRUE(p > 0);
    TEST_ASSERT_TRUE(s > 0);
}

void test_every_identifier_is_unique() {
    for (size_t i = 0; i < vectors.size(); ++i) {
        for (size_t j = i + 1; j < vectors.size(); ++j) {
            TEST_ASSERT_TRUE_MESSAGE(vectors[i].id != vectors[j].id,
                                     vectors[i].id.c_str());
        }
    }
}

void test_family_A_matches_the_production_quantiser() {
    size_t seen = 0;
    for (size_t i = 0; i < vectors.size(); ++i) {
        const Vector& v = vectors[i];
        if (v.family != 'A') {
            continue;
        }
        ++seen;
        const int8_t got = flexseq::lengthcv::zoneWithHysteresis(
            static_cast<int16_t>(v.a), static_cast<int8_t>(v.b));
        TEST_ASSERT_EQUAL_INT_MESSAGE(v.expected, got, v.id.c_str());
    }
    TEST_ASSERT_TRUE(seen > 0);
}

void test_family_L_matches_the_production_clamp() {
    size_t seen = 0;
    for (size_t i = 0; i < vectors.size(); ++i) {
        const Vector& v = vectors[i];
        if (v.family != 'L') {
            continue;
        }
        ++seen;
        const uint8_t got = flexseq::lengthcv::effectiveLengthFor(
            static_cast<uint8_t>(v.a), static_cast<int8_t>(v.b + v.c));
        TEST_ASSERT_EQUAL_INT_MESSAGE(v.expected, got, v.id.c_str());
    }
    TEST_ASSERT_TRUE(seen > 0);
}

void test_family_P_matches_the_production_clamp() {
    size_t seen = 0;
    for (size_t i = 0; i < vectors.size(); ++i) {
        const Vector& v = vectors[i];
        if (v.family != 'P') {
            continue;
        }
        ++seen;
        const uint8_t got = flexseq::lengthcv::patternIndexFor(
            static_cast<uint8_t>(v.a), static_cast<int8_t>(v.b + v.c));
        TEST_ASSERT_EQUAL_INT_MESSAGE(v.expected, got, v.id.c_str());
    }
    TEST_ASSERT_TRUE(seen > 0);
}

void test_family_S_matches_the_production_read_step() {
    size_t seen = 0;
    for (size_t i = 0; i < vectors.size(); ++i) {
        const Vector& v = vectors[i];
        if (v.family != 'S') {
            continue;
        }
        ++seen;
        TEST_ASSERT_EQUAL_INT_MESSAGE(v.expected,
                                      flexseq::lengthcv::readStepFor(static_cast<uint8_t>(v.a), static_cast<int8_t>(v.b), static_cast<uint8_t>(v.c)),
                                      v.id.c_str());
    }
    TEST_ASSERT_TRUE(seen > 0);
}

void test_the_read_step_stays_inside_the_length_for_every_input() {
    for (int length = 1; length <= 36; ++length) {
        for (int local = 0; local < length; ++local) {
            for (int sum = -30; sum <= 30; ++sum) {
                const int got = flexseq::lengthcv::readStepFor(static_cast<uint8_t>(local), static_cast<int8_t>(sum), static_cast<uint8_t>(length));
                TEST_ASSERT_TRUE(got >= 0);
                TEST_ASSERT_TRUE(got < length);
            }
        }
    }
}

void test_a_length_of_one_always_reads_the_first_step() {
    for (int sum = -30; sum <= 30; ++sum) {
        TEST_ASSERT_EQUAL_INT(0, flexseq::lengthcv::readStepFor(static_cast<uint8_t>(0), static_cast<int8_t>(sum), static_cast<uint8_t>(1)));
    }
}

void test_a_null_offset_reads_the_local_step() {
    for (int length = 1; length <= 36; ++length) {
        for (int local = 0; local < length; ++local) {
            TEST_ASSERT_EQUAL_INT(local, flexseq::lengthcv::readStepFor(static_cast<uint8_t>(local), static_cast<int8_t>(0), static_cast<uint8_t>(length)));
        }
    }
}

void test_the_two_sources_commute_on_the_pattern() {
    for (int base = 0; base <= 15; ++base) {
        for (int one = -15; one <= 15; ++one) {
            for (int two = -15; two <= 15; ++two) {
                const uint8_t direct = flexseq::lengthcv::patternIndexFor(
                    static_cast<uint8_t>(base), static_cast<int8_t>(one + two));
                const uint8_t swapped = flexseq::lengthcv::patternIndexFor(
                    static_cast<uint8_t>(base), static_cast<int8_t>(two + one));
                TEST_ASSERT_EQUAL_INT(direct, swapped);
            }
        }
    }
}

void test_the_two_sources_commute_on_the_length() {
    for (int base = 1; base <= 36; ++base) {
        for (int one = -15; one <= 15; ++one) {
            for (int two = -15; two <= 15; ++two) {
                const uint8_t direct = flexseq::lengthcv::effectiveLengthFor(
                    static_cast<uint8_t>(base), static_cast<int8_t>(one + two));
                const uint8_t swapped = flexseq::lengthcv::effectiveLengthFor(
                    static_cast<uint8_t>(base), static_cast<int8_t>(two + one));
                TEST_ASSERT_EQUAL_UINT8(direct, swapped);
            }
        }
    }
}

void test_two_opposite_offsets_give_the_base_back() {
    for (int base = 1; base <= 36; ++base) {
        for (int amount = 0; amount <= 15; ++amount) {
            TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(base),
                                    flexseq::lengthcv::effectiveLengthFor(
                                        static_cast<uint8_t>(base),
                                        static_cast<int8_t>(amount - amount)));
        }
    }
    for (int base = 0; base <= 15; ++base) {
        for (int amount = 0; amount <= 15; ++amount) {
            TEST_ASSERT_EQUAL_INT(base, flexseq::lengthcv::patternIndexFor(
                                            static_cast<uint8_t>(base),
                                            static_cast<int8_t>(amount - amount)));
        }
    }
}

int main() {
    const bool loaded = load();
    UNITY_BEGIN();
    RUN_TEST(test_the_vector_file_is_loaded_and_holds_cases);
    if (loaded) {
        RUN_TEST(test_every_family_is_represented);
        RUN_TEST(test_every_identifier_is_unique);
        RUN_TEST(test_family_A_matches_the_production_quantiser);
        RUN_TEST(test_family_L_matches_the_production_clamp);
        RUN_TEST(test_family_P_matches_the_production_clamp);
        RUN_TEST(test_family_S_matches_the_production_read_step);
    }
    RUN_TEST(test_the_read_step_stays_inside_the_length_for_every_input);
    RUN_TEST(test_a_length_of_one_always_reads_the_first_step);
    RUN_TEST(test_a_null_offset_reads_the_local_step);
    RUN_TEST(test_the_two_sources_commute_on_the_pattern);
    RUN_TEST(test_the_two_sources_commute_on_the_length);
    RUN_TEST(test_two_opposite_offsets_give_the_base_back);
    return UNITY_END();
}
