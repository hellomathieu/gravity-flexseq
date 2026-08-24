#ifndef FLEXSEQ_TWI_DISPLAY_H
#define FLEXSEQ_TWI_DISPLAY_H

#include <U8g2lib.h>

extern "C" uint8_t flexseqTwiByteCb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int,
                                    void* arg_ptr);

namespace flexseq {

// Meme ecran, meme rendu, autre transport. La signature du constructeur reprend
// celle de U8G2_SSD1306_128X64_NONAME_1_HW_I2C pour que libGravity construise
// l'objet sans changer sa ligne.
class Ssd1306Twi : public U8G2 {
public:
    explicit Ssd1306Twi(const u8g2_cb_t* rotation,
                        uint8_t = U8X8_PIN_NONE,
                        uint8_t = U8X8_PIN_NONE,
                        uint8_t = U8X8_PIN_NONE)
        : U8G2() {
        u8g2_Setup_ssd1306_i2c_128x64_noname_1(&u8g2, rotation, flexseqTwiByteCb,
                                               u8x8_gpio_and_delay_arduino);
    }
};

}  // namespace flexseq

#endif // FLEXSEQ_TWI_DISPLAY_H
