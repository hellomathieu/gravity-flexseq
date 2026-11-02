#include <cstdio>

#include <flexseq/Persistence.h>

#if defined(ACTIVE_FORMAT_V4)
namespace active = flexseq::persist::v4;
static constexpr unsigned kScanSize = flexseq::persist::v4::IMAGE_SIZE;
static constexpr unsigned kBankOffset = flexseq::persist::v4::BANK_OFFSET;
#elif defined(ACTIVE_FORMAT_V2)
namespace active = flexseq::persist;
static constexpr unsigned kScanSize = flexseq::persist::TOTAL_SIZE;
static constexpr unsigned kBankOffset = flexseq::persist::PATTERNS_OFFSET;
#else
#error "the active format must be selected from the firmware ELF, never guessed"
#endif

int main() {
    std::printf("FORMAT_VERSION=%u\n", static_cast<unsigned>(active::FORMAT_VERSION));
    std::printf("IMAGE_SIZE=%u\n", static_cast<unsigned>(active::TOTAL_SIZE));
    std::printf("SCAN_SIZE=%u\n", kScanSize);
    std::printf("VERSION_OFFSET=%u\n", static_cast<unsigned>(active::HEADER_OFFSET));
    std::printf("BASE_ADDRESS=%u\n", static_cast<unsigned>(flexseq::persist::BASE_ADDRESS));
    std::printf("BANK_OFFSET=%u\n", kBankOffset);
    return 0;
}
