#include <flexseq/TwiDisplay.h>

#include <Arduino.h>

namespace {

// L'ecran est en ECRITURE SEULE : rien n'est jamais lu sur le bus. Le pilote se
// reduit donc a depart / adresse / octets / arret, scrutes. Ce qui disparait
// avec Wire n'est pas du confort mais du code sans emploi : la reception, le
// mode esclave, l'ISR et ses trois tampons de 32 octets.
constexpr uint32_t TWI_CLOCK_HZ = 400000UL;
constexpr uint8_t TWI_BITRATE =
    static_cast<uint8_t>(((F_CPU / TWI_CLOCK_HZ) - 16UL) / 2UL);

// Un pilote scrute SANS PLAFOND dependrait d'un etat qu'il ne controle pas :
// ecran absent ou bus tenu bas, et la boucle principale ne reviendrait jamais.
// Un octet a 400 kHz part en ~22,5 us, soit ~360 cycles ; 2000 tours laissent
// plus d'un ordre de grandeur de marge.
constexpr uint16_t TWI_WAIT_MAX = 2000;

bool waitReady() {
    uint16_t guard = TWI_WAIT_MAX;
    while ((TWCR & (1 << TWINT)) == 0) {
        if (--guard == 0) {
            return false;
        }
    }
    return true;
}

bool sendByte(uint8_t value) {
    TWDR = value;
    TWCR = (1 << TWINT) | (1 << TWEN);
    return waitReady();
}

void sendStop() {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

}  // namespace

extern "C" uint8_t flexseqTwiByteCb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int,
                                    void* arg_ptr) {
    switch (msg) {
        case U8X8_MSG_BYTE_SEND: {
            const uint8_t* data = static_cast<const uint8_t*>(arg_ptr);
            while (arg_int > 0) {
                if (!sendByte(*data++)) {
                    return 0;
                }
                --arg_int;
            }
            break;
        }
        case U8X8_MSG_BYTE_INIT:
            // Tirages internes actives comme le fait twi_init d'Arduino : le
            // module porte les siens, mais un bus flottant ne se diagnostique
            // pas facilement.
            digitalWrite(SDA, HIGH);
            digitalWrite(SCL, HIGH);
            TWSR = 0;  // prediviseur 1
            TWBR = TWI_BITRATE;
            TWCR = (1 << TWEN);
            break;
        case U8X8_MSG_BYTE_SET_DC:
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
            if (!waitReady()) {
                return 0;
            }
            if (!sendByte(u8x8_GetI2CAddress(u8x8))) {
                sendStop();
                return 0;
            }
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            sendStop();
            break;
        default:
            return 0;
    }
    return 1;
}
