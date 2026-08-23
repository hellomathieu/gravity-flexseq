#include <stdlib.h>
#include <stdint.h>

namespace {

constexpr size_t POOL_BYTES = 8;
constexpr size_t WORD = sizeof(uint32_t);

uint32_t pool[POOL_BYTES / WORD];
size_t used = 0;

}  // namespace

extern "C" void* malloc(size_t size) {
    const size_t need = (size + WORD - 1) & ~(WORD - 1);
    if (need == 0 || need > POOL_BYTES - used) {
        return nullptr;
    }
    void* block = reinterpret_cast<uint8_t*>(pool) + used;
    used += need;
    return block;
}

extern "C" void free(void*) {}
