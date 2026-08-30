#ifndef FLEXSEQ_TEST_MOCK_AVR_PGMSPACE_H
#define FLEXSEQ_TEST_MOCK_AVR_PGMSPACE_H

#include <stdint.h>

#define PROGMEM
#define PSTR(s) (s)

inline uint8_t pgm_read_byte(const void* address) {
    return *static_cast<const uint8_t*>(address);
}

inline uint8_t pgm_read_byte(const char* address) {
    return static_cast<uint8_t>(*address);
}

#endif
