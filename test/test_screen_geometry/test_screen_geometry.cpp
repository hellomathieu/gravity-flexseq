#include <unity.h>

#include <flexseq/MainScreen.h>
#include <flexseq/PatternScreen.h>

#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef FLEXSEQ_GEOMETRY_VECTORS
#error "FLEXSEQ_GEOMETRY_VECTORS must name the geometry vector file (see platformio.ini)"
#endif

namespace ms = flexseq::mainscreen;
namespace screen = flexseq::screen;

void setUp() {}
void tearDown() {}

namespace {

struct Vector {
    char family;
    std::string id;
    int index;
    bool ownedByCpp;
    std::string cppName;
    int expected;
};

std::vector<Vector> vectors;
std::string loadError;

bool wholeNumber(const std::string& raw, int& out) {
    if (raw.empty() || raw == "-") {
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
    out = std::atoi(raw.c_str());
    return true;
}

bool load() {
    std::ifstream in(FLEXSEQ_GEOMETRY_VECTORS);
    if (!in.is_open()) {
        loadError = "geometry vector file cannot be opened";
        return false;
    }
    std::string line;
    if (!std::getline(in, line)) {
        loadError = "geometry vector file is empty";
        return false;
    }
    std::set<std::string> seen;
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
        if (v.family != 'M' && v.family != 'E') {
            loadError = "unknown family: " + line;
            return false;
        }
        v.id = cell[1];
        if (v.id.empty() || v.id == "-") {
            loadError = "the id is a needed column: " + line;
            return false;
        }
        if (!seen.insert(v.id).second) {
            loadError = "duplicated id: " + line;
            return false;
        }
        if (!wholeNumber(cell[2], v.index)) {
            loadError = "the index is a needed column, and a whole number: " + line;
            return false;
        }
        if (cell[3] == "both") {
            v.ownedByCpp = true;
        } else if (cell[3] == "cpp") {
            v.ownedByCpp = true;
        } else if (cell[3] == "ts") {
            v.ownedByCpp = false;
        } else {
            loadError = "owners is not both, cpp or ts: " + line;
            return false;
        }
        v.cppName = cell[4];
        if (v.ownedByCpp && (v.cppName.empty() || v.cppName == "-")) {
            loadError = "a line owned by cpp must name its C++ identifier: " + line;
            return false;
        }
        if (!wholeNumber(cell[5], v.expected)) {
            loadError = "the expected value is a needed column: " + line;
            return false;
        }
        vectors.push_back(v);
    }
    if (vectors.empty()) {
        loadError = "geometry vector file holds no case";
        return false;
    }
    return true;
}

// La production, et rien d'autre. Une paire (famille, index) que cette fonction
// ne connait pas rend false : une ligne sautee en silence serait un trou.
bool production(char family, int index, int& out) {
    if (family == 'M') {
        switch (index) {
            case  0: out = screen::WIDTH; return true;
            case  1: out = screen::HEIGHT; return true;
            case  2: out = ms::TAB_COUNT; return true;
            case  3: out = ms::TAB_SLOT_W; return true;
            case  4: out = ms::TAB_BASELINE_Y; return true;
            case  5: out = ms::TAB_TOP_Y; return true;
            case  6: out = ms::TAB_BOX_Y; return true;
            case  7: out = ms::TAB_BOX_H; return true;
            case  8: out = ms::RULE_Y; return true;
            case  9: out = ms::RULE_X; return true;
            case 10: out = ms::RULE_W; return true;
            case 11: out = ms::HEADLINE_BOX_X; return true;
            case 12: out = ms::HEADLINE_BOX_Y; return true;
            case 13: out = ms::HEADLINE_BOX_W; return true;
            case 14: out = ms::HEADLINE_BOX_H; return true;
            case 15: out = ms::HEADLINE_BASELINE_Y; return true;
            case 16: out = ms::ROW_A_BOX_Y; return true;
            case 17: out = ms::ROW_B_BOX_Y; return true;
            case 18: out = ms::ROW_BOX_H; return true;
            case 19: out = ms::COL_LEFT_X; return true;
            case 20: out = ms::COL_RIGHT_X; return true;
            case 21: out = ms::COL_W; return true;
            case 22: out = ms::TEXT_INSET; return true;
            case 23: out = ms::GLYPH_SIZE; return true;
            case 24: out = ms::ROW_A_BASELINE_Y; return true;
            case 25: out = ms::ROW_B_BASELINE_Y; return true;
            default: return false;
        }
    }
    if (family == 'E') {
        switch (index) {
            case  0: out = screen::PER_ROW; return true;
            case  1: out = screen::GRID_ROWS; return true;
            case  2: out = screen::GRID_STEPS; return true;
            case  3: out = screen::COL_SPACING; return true;
            case  4: out = screen::GRID_WIDTH; return true;
            case  5: out = screen::COL_X0; return true;
            case  6: out = screen::ROW_CY_0; return true;
            case  7: out = screen::ROW_SPACING; return true;
            case  8: out = screen::GLYPH_HALF; return true;
            case  9: out = screen::SELECT_SIZE; return true;
            case 10: out = screen::DIGIT_W; return true;
            case 11: out = screen::DIGIT_H; return true;
            case 12: out = screen::DIGIT_DY; return true;
            case 13: out = screen::BAR_HEIGHT; return true;
            case 14: out = screen::BAR_HALF_H; return true;
            case 15: out = screen::TITLE_BASELINE_Y; return true;
            case 16: out = screen::HEADER_LINE_X; return true;
            case 17: out = screen::HEADER_LINE_Y; return true;
            case 18: out = screen::HEADER_LINE_W; return true;
            case 19: out = screen::LAST_ROW_CY; return true;
            case 20: out = screen::GRID_BOTTOM_Y; return true;
            case 21: out = screen::GLYPH_ASCENT; return true;
            default: return false;
        }
    }
    return false;
}

}  // namespace

void test_the_geometry_file_loads() {
    TEST_ASSERT_TRUE_MESSAGE(load(), loadError.c_str());
    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, vectors.size(),
        "a suite that loads zero vector must fail");
}

void test_every_line_owned_by_cpp_matches_the_production_constant() {
    unsigned confronted = 0;
    for (const Vector& v : vectors) {
        if (!v.ownedByCpp) {
            continue;
        }
        int actual = 0;
        char message[160];
        if (!production(v.family, v.index, actual)) {
            snprintf(message, sizeof(message),
                     "the reader knows no constant for family %c index %d (%s):"
                     " a skipped line is a hole",
                     v.family, v.index, v.id.c_str());
            TEST_FAIL_MESSAGE(message);
        }
        snprintf(message, sizeof(message), "%s (family %c index %d)",
                 v.id.c_str(), v.family, v.index);
        TEST_ASSERT_EQUAL_INT_MESSAGE(v.expected, actual, message);
        ++confronted;
    }
    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, confronted,
        "no line was confronted: the guard would be empty");
}

void test_the_file_covers_every_constant_the_reader_knows() {
    // Le miroir de la propriete precedente : une constante que le lecteur
    // connait mais que le fichier ne nomme pas serait non gardee.
    for (char family : {'M', 'E'}) {
        int probe = 0;
        for (int index = 0; production(family, index, probe); ++index) {
            bool named = false;
            for (const Vector& v : vectors) {
                if (v.family == family && v.index == index) {
                    named = true;
                    break;
                }
            }
            char message[160];
            snprintf(message, sizeof(message),
                     "family %c index %d exists in the reader and the file does"
                     " not name it: that constant is unguarded", family, index);
            TEST_ASSERT_TRUE_MESSAGE(named, message);
        }
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_the_geometry_file_loads);
    RUN_TEST(test_every_line_owned_by_cpp_matches_the_production_constant);
    RUN_TEST(test_the_file_covers_every_constant_the_reader_knows);
    return UNITY_END();
}
