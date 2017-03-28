/*
 *
 * This example is configured for a Atmega32 at 16MHz
 */ 

#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "light_ws2812.h"

#define MAXPIX 16

struct cRGB _buf1[MAXPIX], _buf2[MAXPIX];
struct cRGB *buf1=_buf1, *buf2=_buf2;

int main(void) {
    /* use 250kBd */
    UBRR0H  = 0;
    UBRR0L  = 3;
    UCSR0B  = (1<<RXEN0)  | (1<<TXEN0) | (1<<RXCIE0);
    UCSR0C  = (1<<UCSZ01) | (1<<UCSZ00);

	DDRB  |= 1<<ws2812_pin;
		
    for (uint8_t i=MAXPIX; i--;) {    
        buf1[i].r=0; buf1[i].g=0; buf1[i].b=0;
        buf2[i].r=0; buf2[i].g=0; buf2[i].b=0;
    }
    sei();
		
	while(23) {
		 _delay_ms(50);
		 ws2812_sendarray((uint8_t *)buf1, MAXPIX*sizeof(struct cRGB));
    }
}

uint8_t hex_to_int(uint8_t ch) {
    if ('0' <= ch && ch <= '9')
        return ch-'0';
    else if ('a' <= ch && ch <= 'f')
        return ch-'a'+0xa;
    else if ('A' <= ch && ch <= 'F')
        return ch-'A'+0xA;
    return 255;
}

ISR (USART_RX_vect) {
    static uint8_t idx = 0;
    static uint8_t rxval = 0;
    char ch = UDR0;
    /* echo */
    while (! (UCSR0A & (1<<UDRE0))) ;
    UDR0 = ch;

    if (idx) {
        if (ch == '\r') {
            idx = 2;
            return;
        }

        uint8_t hv = hex_to_int(ch);
        if (hv == 255) {
            idx = 0;
            return;
        }

        if (idx&1)
            ((uint8_t *)buf2)[(idx>>1)-1] = rxval | hv;
        else
            rxval = hv<<4;

        if (idx++ == 2*MAXPIX*sizeof(struct cRGB)) {
            struct cRGB *tmp = buf2; buf2 = buf1; buf1 = tmp;
            idx = 0;
        }
    } else {
        if (ch == '\r')
            idx = 2;
    }
}


