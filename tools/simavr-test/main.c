#include <avr/io.h>
#include <util/delay.h>

#define F_CPU 16000000UL

int main(void)
{
    /* Gravity CH1 = Arduino D7 = ATmega328P PORTD bit 7 */
    DDRD |= _BV(DDD7);

    for (;;) {
        PORTD |= _BV(PORTD7);
        _delay_ms(100);

        PORTD &= (uint8_t)~_BV(PORTD7);
        _delay_ms(100);
    }
}