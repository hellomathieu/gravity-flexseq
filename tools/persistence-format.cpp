#include <cstdio>

#include <flexseq/Persistence.h>

#if defined(ACTIVE_FORMAT_V3)
namespace active = flexseq::persist::v3;
static constexpr unsigned kScanSize = flexseq::persist::v3::IMAGE_SIZE;
#elif defined(ACTIVE_FORMAT_V2)
namespace active = flexseq::persist;
static constexpr unsigned kScanSize = flexseq::persist::TOTAL_SIZE;
#else
#error "the active format must be selected from the firmware ELF, never guessed"
#endif

int main() {
    std::printf("FORMAT_VERSION=%u\n", static_cast<unsigned>(active::FORMAT_VERSION));
    std::printf("IMAGE_SIZE=%u\n", static_cast<unsigned>(active::TOTAL_SIZE));
    std::printf("SCAN_SIZE=%u\n", kScanSize);
    std::printf("VERSION_OFFSET=%u\n", static_cast<unsigned>(active::HEADER_OFFSET));
    std::printf("BASE_ADDRESS=%u\n", static_cast<unsigned>(flexseq::persist::BASE_ADDRESS));
    return 0;
}
