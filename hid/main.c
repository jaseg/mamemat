/* Licence: GPLv3
 *
 * Copyright (c) 2013 Guy Weiler <weigu@weigu.lu>
 * Copyright (c) 2015 Sebastian Götte <jaseg@jaseg.net>
 *
 * This work modified from weigu www.weigu.lu
 * inspired by the lib from S. Salewski (http://http://www.ssalewski.de)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include "usb_srs_hid.h"
#include <avr/wdt.h>

#define NONE 0x00
#define LCTR 0xe0 /* left control */
#define LSHT 0xe1 /* left shift */
#define LALT 0xe2 /* left alt */
#define LGUI 0xe3 /* left gui */
#define RCTR 0xe4 /* right control */
#define RSHT 0xe5 /* right shift */
#define RALT 0xe6 /* right alt */
#define RGUI 0xe7 /* right gui */

struct kbd_hid_rep {
    uint8_t mod;
    uint8_t _pad;
    uint8_t keys[6];
};

volatile struct kbd_hid_rep *rep = (volatile struct kbd_hid_rep *)ep1_buf;

int main(void) {
    /* A4, A5 unused */

    DDRB = 0; DDRC = 0; DDRD = 0; DDRE = 0; DDRF = 0;

    /* enable pullups */
    /* right joystick */
    PORTF |= 0xF0; /* A0-3 */
    /* left joystick */
    PORTB |= 0xC0; /* D10, 11 */
    PORTD |= 0x40; /* D12 */
    PORTC |= 0x80; /* D13 */
    /* buttons */
    PORTD |= 0x93; /* D2, 3, 4, 6 */
    PORTC |= 0x40; /* D5 */
    PORTE |= 0x40; /* D7 */
    PORTB |= 0x30; /* D8, 9 */

    /* DEBUG LED */
    DDRC |= 0x80;
    PORTC &= 0x7F;

    usb_init_device();
    memset((void *)ep1_buf, 0, sizeof(ep1_buf));
    sei();

    struct { uint8_t b, c, d, e, f; } pinstate = {0};

    while (23) {
        /* check whether any pins have changed */
        uint8_t b=PINB, c=PINC, d=PIND, e=PINE, f=PINF;
        if (pinstate.b == b && pinstate.c == c && pinstate.d == d && pinstate.e == e && pinstate.f == f)
            continue;
        pinstate.b = b; pinstate.c = c; pinstate.d = d; pinstate.e = e; pinstate.f = f;

        /* set modbyte. we use this for the action buttons, since all of them may be pressed at once. */
        rep->mod = (!((PIND&0x02)<<7)) /* D2 buttons left*/
                 | (!((PIND&0x01)<<6)) /* D3 */
                 | (!((PIND&0x10)<<5)) /* D4 */
                 | (!((PINC&0x40)<<4)) /* D5 I/II player buttons */
                 | (!((PINE&0x40)<<3)) /* D7 buttons right */
                 | (!((PINB&0x10)<<2)) /* D8 */
                 | (!((PINB&0x20)<<1)) /* D9 */
                 | (!((PIND&0x80)<<0));/* D6 I/II player buttons */

        /* handle joysticks.
         * it should be physically impossible to press more than two of one joystick's switches at once */
        uint8_t i=0;

        /* left joystick */
        if (! (PINF&0x10))
            rep->keys[i++] = 0x04;
        if (! (PINF&0x20))
            rep->keys[i++] = 0x05;
        if (! (PINF&0x40))
            rep->keys[i++] = 0x06;
        if (! (PINF&0x80))
            rep->keys[i++] = 0x07;

        /* right joystick */
        if (! PINB&0x40)
            rep->keys[i++] = 0x08;
        if (! PINB&0x80)
            rep->keys[i++] = 0x09;
        if (! PIND&0x40)
            rep->keys[i++] = 0x0a;
        if (! PINC&0x80)
            rep->keys[i++] = 0x0b;

        //if (i>6) { /* at least one of the joysticks is broken, and we did an out-of-bounds write. */
            /* soft-reset the device */
        //    wdt_enable(WDTO_15MS);
        //    while (42);
        //}

        ep1_cnt = i+2; /* mod (1b) + _pad (1b) + partial array (0-6b) */
    }
}
