#ifndef FLEXSEQ_EEPROM_STORAGE_H
#define FLEXSEQ_EEPROM_STORAGE_H

#include <stdint.h>

#ifdef __AVR__
#include <avr/eeprom.h>
#endif

namespace flexseq {

struct EepromStorage {
    uint8_t read(uint16_t address) const {
#ifdef __AVR__
        return eeprom_read_byte(reinterpret_cast<const uint8_t*>(address));
#else
        (void)address;
        return 0xFF;
#endif
    }

    void write(uint16_t address, uint8_t value) {
#ifdef __AVR__
        eeprom_update_byte(reinterpret_cast<uint8_t*>(address), value);
#else
        (void)address;
        (void)value;
#endif
    }

    bool busy() const {
#ifdef __AVR__
        return !eeprom_is_ready();
#else
        return false;
#endif
    }
};

}  // namespace flexseq

#endif // FLEXSEQ_EEPROM_STORAGE_H
