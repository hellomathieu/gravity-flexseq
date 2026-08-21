#ifndef FLEXSEQ_SIMAVR_UART_QUIET_H
#define FLEXSEQ_SIMAVR_UART_QUIET_H

/*
 * uart_quiet — desarme le journal de console de l'UART de simavr.
 *
 * A APPELER JUSTE APRES avr_load_firmware(), dans tout harnais qui fait tourner
 * un firmware ecrivant sur l'UART. Le notre en ecrit : MIDI passe par le port
 * serie (NeoHWSerial), donc chaque octet emis atteint UDR.
 *
 * CE QU'ON EVITE, ET CE N'EST PAS UNE PRECAUTION. simavr arme
 * AVR_UART_FLAG_STDIO par defaut : les octets ecrits dans UDR sont accumules
 * dans un tampon de 256 octets, puis passes a son journal — donc a vfprintf,
 * en %s. Quand le tampon est exactement plein, il n'est PAS termine par un zero
 * et la lecture depasse d'un octet. AddressSanitizer le donne mot pour mot :
 *
 *   READ of size 257 at ... 0 bytes after 256-byte region
 *     #0 printf_common
 *     #2 avr_global_logger
 *     #3 avr_uart_udr_write
 *     #4 _avr_set_ram          <- le firmware ecrit dans UDR
 *
 * Le defaut est donc DANS simavr, une lecture hors bornes d'un octet, et non
 * « un tas corrompu » comme on l'a d'abord cru : la faute tombait dans
 * l'allocateur parce que la lecture traversait ses metadonnees, pas parce
 * qu'une de nos allocations etait fautive. Mesure avant/apres sur
 * blocking_probe : 2 SIGSEGV sur 5 executions et 3 rapports ASan sur 3, contre
 * 0 sur 5 et 0 sur 3.
 *
 * On ne touche pas a la dependance : simavr expose lui-meme l'interrupteur, ce
 * chemin est OPTIONNEL, et un harnais de mesure n'a aucun besoin d'un journal
 * de console. Aucune donnee de mesure ne passe par la — la sortie du firmware
 * qui nous interesse est lue en RAM simulee ou sur le bus I2C.
 *
 * BENEFICE SECOND, mesure aussi : ces 256 octets etaient DEVERSES dans la
 * sortie standard du harnais, au milieu de son rapport. C'est pour eux que les
 * scripts relisaient leur log en errors='replace'.
 *
 * Un harnais qui voudrait observer le trafic UART ne perd rien : les IRQ
 * UART_IRQ_OUTPUT / UART_IRQ_INPUT restent le moyen normal de le faire, et
 * elles sont independantes de ce drapeau.
 */

#include <stdint.h>

#include <avr_uart.h>
#include <sim_avr.h>

static inline void uart_quiet(avr_t *avr, char port)
{
    uint32_t flags = 0;
    avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS(port), &flags);
    flags &= ~AVR_UART_FLAG_STDIO;
    avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS(port), &flags);
}

#endif /* FLEXSEQ_SIMAVR_UART_QUIET_H */
