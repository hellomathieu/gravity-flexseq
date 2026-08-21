#include <Arduino.h>
#include <EEPROM.h>

namespace {

constexpr uint16_t EEPROM_BYTES = 1024;
constexpr uint8_t BYTES_PER_RECORD = 16;
constexpr uint32_t SERIAL_BAUD = 9600;
constexpr uint16_t SETTLE_MS = 2000;

void emitHexByte(uint8_t value) {
    static const char DIGITS[] = "0123456789ABCDEF";
    Serial.write(DIGITS[value >> 4]);
    Serial.write(DIGITS[value & 0x0F]);
}

void emitDataRecord(uint16_t address) {
    uint8_t checksum = BYTES_PER_RECORD;
    checksum = static_cast<uint8_t>(checksum + static_cast<uint8_t>(address >> 8));
    checksum = static_cast<uint8_t>(checksum + static_cast<uint8_t>(address));

    Serial.write(':');
    emitHexByte(BYTES_PER_RECORD);
    emitHexByte(static_cast<uint8_t>(address >> 8));
    emitHexByte(static_cast<uint8_t>(address));
    emitHexByte(0x00);

    for (uint8_t i = 0; i < BYTES_PER_RECORD; ++i) {
        const uint8_t value = EEPROM.read(static_cast<int>(address + i));
        checksum = static_cast<uint8_t>(checksum + value);
        emitHexByte(value);
    }

    emitHexByte(static_cast<uint8_t>(0u - checksum));
    Serial.write('\r');
    Serial.write('\n');
}

void emitEndRecord() {
    Serial.write(":00000001FF\r\n");
}

}  // namespace

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(SETTLE_MS);

    for (uint16_t address = 0; address < EEPROM_BYTES; address += BYTES_PER_RECORD) {
        emitDataRecord(address);
    }
    emitEndRecord();
    Serial.flush();
}

void loop() {
}
