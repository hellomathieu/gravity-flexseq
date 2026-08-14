#pragma once

#include <stdint.h>

class MockNeoHWSerial {
public:
    using isr_t = bool (*)(uint8_t, uint8_t);
    using void_isr_t = void (*)(uint8_t, uint8_t);

    // API correspondant à NeoHWSerial
    void attachInterrupt(isr_t callback) {
        callback_ = callback;
    }

    // Compatibilité avec les callbacks réellement utilisés par
    // libGravity::Clock dans le commit 9be88be1f4.
    //
    // Le vrai NeoHWSerial accepte actuellement cette conversion
    // avec -fpermissive côté AVR.
    void attachInterrupt(void_isr_t callback) {
        void_callback_ = callback;
    }

    void detachInterrupt() {
        callback_ = nullptr;
        void_callback_ = nullptr;
    }

    void begin(unsigned long baud) {
        baud_ = baud;
    }

    void end() {
    }

    void write(uint8_t value) {
        last_written_ = value;
        write_count_++;
    }

    bool dispatch(uint8_t data, uint8_t status = 0) {
        if (callback_ != nullptr) {
            return callback_(data, status);
        }

        if (void_callback_ != nullptr) {
            void_callback_(data, status);
            return true;
        }

        return false;
    }

    isr_t callback() const {
        return callback_;
    }

    uint8_t lastWritten() const {
        return last_written_;
    }

    uint32_t writeCount() const {
        return write_count_;
    }

    void reset() {
        callback_ = nullptr;
        void_callback_ = nullptr;
        baud_ = 0;
        last_written_ = 0;
        write_count_ = 0;
    }

private:
    isr_t callback_ = nullptr;
    void_isr_t void_callback_ = nullptr;

    unsigned long baud_ = 0;
    uint8_t last_written_ = 0;
    uint32_t write_count_ = 0;
};

extern MockNeoHWSerial NeoSerial;