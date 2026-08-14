#ifndef U8G2LIB_H
#define U8G2LIB_H

#include <stdint.h>

#define U8G2_R2 2
#define U8X8_PIN_NONE 255

class U8G2 {
public:
    U8G2(int, int, int, int) {}

    void begin() {}
};

class U8G2_SSD1306_128X64_NONAME_1_HW_I2C : public U8G2 {
public:
    U8G2_SSD1306_128X64_NONAME_1_HW_I2C(
        int rotation,
        int clock,
        int data,
        int reset
    )
        : U8G2(rotation, clock, data, reset) {}
};

#endif