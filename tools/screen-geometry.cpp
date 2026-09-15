#include <cstdio>

#include <flexseq/MainScreen.h>

int main() {
    namespace ms = flexseq::mainscreen;
    std::printf("TAB_COUNT=%u\n", static_cast<unsigned>(ms::TAB_COUNT));
    std::printf("TAB_SLOT_W=%u\n", static_cast<unsigned>(ms::TAB_SLOT_W));
    std::printf("TAB_CLOCK=%u\n", static_cast<unsigned>(ms::TAB_CLOCK));
    std::printf("TAB_FIRST_CHANNEL=%u\n", static_cast<unsigned>(ms::TAB_FIRST_CHANNEL));
    std::printf("TAB_LAST_CHANNEL=%u\n", static_cast<unsigned>(ms::TAB_LAST_CHANNEL));
    std::printf("TAB_SETTINGS=%u\n", static_cast<unsigned>(ms::TAB_SETTINGS));
    return 0;
}
