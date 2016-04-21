/* Licence: GPLv3
 *
 * Copyright (c) 2009 Guy Weiler <weigu@weigu.lu>
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

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/delay.h>
#include <stdint.h>
#include "usb_srs_hid.h"

#define UNUSED(x) (void)(x)


volatile uint8_t ep1_buf[16]; /* TODO about 8 bytes should be sufficient here. Check USB FIFO handling and call sites. */
volatile uint8_t ep1_cnt;

volatile uint8_t ep2_buf[16];
volatile uint8_t ep2_cnt;

ISR(USB_GEN_vect) {
    if (UDINT & (1 << EORSTI)) { // End Of ReSeT?
        UDINT &= ~(1<<EORSTI);      // sperre EORSTI
        usb_init_endpoint(0, 0, 0, 0, 0); /* control out, 8 bytes, 1 bank */
        UEIENX |= (1<<RXSTPE);
    }
}

ISR(USB_COM_vect) {
    if (UEINT&1) { /* ep 0 */
        UENUM = 0;
        if (UEINTX & (1<<RXSTPI))
            usb_ep0_setup();
    }

    if (UEINT&2) { /* ep 1 */
        UENUM = 1;
        usb_ep_in(ep1_buf, &ep1_cnt);
    }

    if (UEINT&4) { /* ep 2 */
        UENUM = 2;
        usb_ep_out(ep2_buf, &ep2_cnt);
    }
}


void usb_init_device(void) {
    UHWCON |= (1<<UVREGE);

    /* start USB PLL */
    PLLCSR = (1<<PINDIV);
    PLLFRQ = (1<<PDIV2);
    PLLCSR |= (1<<PLLE);
    while (!(PLLCSR & (1<<PLOCK)))
        ; /* wait for PLL to lock */

    USBCON |= (1<<USBE);
    USBCON |= (1<<OTGPADE);
    USBCON &= ~(1<<FRZCLK); /* enable clock */
    UDCON |= (1<<DETACH);
    UDCON &= ~(1<<LSM);

    UDIEN   = (1<<EORSTE);  /* EndOfReSeT interrupt */

    usb_init_endpoint(0, 0, 0, 0, 0); /* control out, 8 bytes, 1 bank */
    UEIENX |= (1<<RXSTPE);

    while (!(USBSTA&(1<<VBUS)));
    UDCON &= ~(1<<DETACH);

    ep1_cnt = 0;
    ep2_cnt = 0;
}

void usb_init_endpoint(uint8_t ep, uint8_t type, uint8_t direction, uint8_t size, uint8_t banks) {
    UENUM = ep;
    UECONX |= (1<<EPEN);
    UECFG0X = ((type<<6) | direction);
    UECFG1X = ((size<<4) | (banks<<2));
    UECFG1X |= (1<<ALLOC);
}


static const uint8_t PROGMEM dev_des[] = {
    18,         // bLength
    0x01,       // bDescriptorType: device
    0x10,0x01,  // bcdUSB: 1.1
    0x00,       // bDeviceClass
    0x00,       // bDeviceSubClass
    0x00,       // bDeviceProtocoll
    8,          // bMaxPacketSize0
    0xeb,0x03,  // idVendor: Atmel
    0x02,0x00,  // idProduct
    0x00,0x01,  // bcdDevice (1.0)
    0x01,       // iManufacturer
    0x02,       // iProduct
    0x03,       // iSerialNumber
    0x01        // bNumConfigurations
};

static const uint8_t PROGMEM conf_des[] = {
    9,          // bLength
    0x02,       // bDescriptorType: configuration
    0x29,0x00,  // wTotalLength
    1,          // bNumInterfaces
    0x01,       // bConfigurationValue
    0,          // iConfiguration
    0x80,       // bmAttributes: bus-powered, no remote wakeup
    250,        // MaxPower: 500mA

    /* interface 0: HID keyboard */
    9,          // bLength
    0x04,       // bDescriptorType: interface
    0,          // bInterfaceNumber
    0,          // bAlternateSetting
    2,          // bNumEndpoints
    0x03,       // bInterfaceClass: HID
    0x00,       // bInterfaceSubClass: 0x00
    0x00,       // bInterfaceProtocol: 0x00
    0,          // iInterface: none

    /* HID */
    9,          // bLength
    0x21,       // bDescriptorType: HID
    0x10, 0x01, // bcdHID: 1.1
    0,          // bCountryCode: none
    0x01,       // bNumDescriptors
    0x22,       // bDescriptorType: report
    0x3b, 0x00, // wDescriptorLength (report)

    /* ep 1 */
    7,          // bLength
    0x05,       // bDescriptorType: Endpoint
    0x81,       // bEndpointAddress: IN
    0x03,       // bmAttributes: Interrupt
    32, 0,      // wMaxPacketSize
    20,         // bInterval: [ms]

    /* ep 2 */
    7,          // bLength
    0x05,       // bDescriptorType: Endpoint
    0x02,       // bEndpointAddress: OUT
    0x03,       // bmAttributes: Interrupt
    32, 0,      // wMaxPacketSize
    20,         // bInterval: [ms]
};

static const uint8_t PROGMEM lang_des[] = {
    4,         // bLength
    0x03,      // bDescriptorType: String
    0x09, 0x04 // wLANGID[0]: english (USA) (Supported Lang. Code 0)
};

static const uint8_t PROGMEM manu_des[] = {
    20,   // bLength = 0x04, Groesse Deskriptor in Byte
    0x03, // bDescriptorType = 0x03, Konstante String = 3
    'j', 0, 'a', 0, 's', 0, 'e', 0, 'g', 0, '.', 0, 'n', 0, 'e', 0, 't', 0
};

static const uint8_t PROGMEM prod_des[] = {
    24,   // bLength
    0x03, // bDescriptorType: String
    'U', 0, 'S', 0, 'B', 0, ' ', 0, 'H', 0, 'I', 0, 'D', 0, ' ', 0, 'f', 0, 'o', 0, 'o', 0
};

static const uint8_t PROGMEM seri_des[] = {
    10,   // bLength
    0x03, // bDescriptorType: string
    '1', 0, '3', 0, '3', 0, '7', 0
};

static const uint8_t PROGMEM rep_des[] = {
    0x05, 0x01, // Usage_Page (Generic Desktop)
    0x09, 0x06, // Usage (Keyboard)
    0xA1, 0x01, // Collection (Application)
    0x05, 0x07, // Usage page (Key Codes)
    0x19, 0xE0, // Usage_Minimum (224)
    0x29, 0xE7, // Usage_Maximum (231)
    0x15, 0x00, // Logical_Minimum (0)
    0x25, 0x01, // Logical_Maximum (1)
    0x75, 0x01, // Report_Size (1)
    0x95, 0x08, // Report_Count (8)
    0x81, 0x02, // Input (Data,Var,Abs) = Modifier Byte
    0x81, 0x01, // Input (Constant) = Reserved Byte
    0x19, 0x00, // Usage_Minimum (0)
    0x29, 0x65, // Usage_Maximum (101)
    0x15, 0x00, // Logical_Minimum (0)
    0x25, 0x65, // Logical_Maximum (101)
    0x75, 0x08, // Report_Size (8)
    0x95, 0x06, // Report_Count (6)
    0x81, 0x00, // Input (Data,Array) = Keycode Bytes(6)
    0x05, 0x08, // Usage Page (LEDs)
    0x19, 0x01, // Usage_Minimum (1)
    0x29, 0x05, // Usage_Maximum (3)
    0x15, 0x00, // Logical_Minimum (0)
    0x25, 0x01, // Logical_Maximum (1)
    0x75, 0x01, // Report_Size (1)
    0x95, 0x03, // Report_Count (3)
    0x91, 0x02, // Output (Data,Var,Abs) = LEDs (3 bits)
    0x95, 0x03, // Report_Count (3)
    0x91, 0x01, // Output (Constant) = Pad (3 bits)
    0xC0        // End_Collection
};

void usb_ep0_setup(void) {
    uint8_t bmRequestType = UEDATX;
    uint8_t bRequest      = UEDATX;
    uint8_t wValue_l      = UEDATX;
    uint8_t wValue_h      = UEDATX;
    uint8_t wIndex_l      = UEDATX;
    uint8_t wIndex_h      = UEDATX;
    uint8_t wLength_l     = UEDATX;
    uint8_t wLength_h     = UEDATX;

    UNUSED(wIndex_h);
    UNUSED(wIndex_l);

    UEINTX &= ~(1<<RXSTPI); /* transmit ACK */

//    if (wIndex_h) { /* sanity check, we only have less than 256 interfaces. */
//        UECONX |= (1<<STALLRQ);
//        return;
//    }

    switch (bRequest) {
    case 0x00: /* GET_STATUS (3-phase); FIXME: No proper handling here yet, works for me though */
        UEDATX  = 0;
        UEDATX  = 0;
        UEINTX &= ~(1<<TXINI); /* transmit ACK */
        while (!(UEINTX & (1 << RXOUTI)))
            ; /* wait for ZLP */
        UEINTX &= ~(1<<RXOUTI);
        break;

    case 0x05: /* SET_ADDRESS (2-phase; no data phase) */
        if (bmRequestType != 0x00) /* host-to-device standard device */
            break;

        UDADDR  = (wValue_l & 0x7F);
        UEINTX &= ~(1<<TXINI); /* transmit ZLP */
        while (!(UEINTX & (1 << TXINI)))
            ; /* wait for bank release */
        UDADDR |= (1<<ADDEN);
        break;

    case 0x06: /* GET_DESCRIPTOR (3-phase transfer) */
        if (bmRequestType == 0x80) { /* device-to-host standard device */
            switch (wValue_h) {
            case 1: /* device */
                usb_send_descriptor(dev_des, sizeof(dev_des));
                break;

            case 2: /* configuration */
                /* Manchmal erfragt Windows unsinnig hohe Werte (groesser als
                 * 256 Byte). Dadurch steht dann im Lowbyte ev. ein falscher
                 * Wert Es wird hier davon ausgegangen, dass nicht mehr als 256
                 * Byte angefragt werden können. */
                if (wLength_h || (wLength_l > sizeof(conf_des)) || !wLength_l)
                    wLength_l = sizeof(conf_des);
                usb_send_descriptor(conf_des, wLength_l);
                break;

            case 3: /* string */
                switch (wValue_l) {
                    case 0:
                        usb_send_descriptor(lang_des, sizeof(lang_des));
                        break;
                    case 1:
                        usb_send_descriptor(prod_des, sizeof(prod_des));
                        break;
                    case 2:
                        usb_send_descriptor(manu_des, sizeof(manu_des));
                        break;
                    case 3:
                        usb_send_descriptor(seri_des, sizeof(seri_des));
                        break;
                }
                break;
            }
        } else if (bmRequestType == 0x81) { /* device-to-host standard interface */
            if (wValue_h == 0x22) /* HID report */
                usb_send_descriptor(rep_des, (wLength_l > sizeof(rep_des)) ? sizeof(rep_des) : wLength_l);
        }
        break;

    case 0x09: /* SET_CONFIGURATION (2-phase; no data phase) */
        if (bmRequestType != 0x00) /* host-to-device standard device */
            break;

        for (uint8_t i=4; i; i--) { /* backwards due to memory assignment */
            UENUM    = i;
            UECONX  &= ~(1<<EPEN);
            UECFG1X &= ~(1<<ALLOC);
        }

        usb_init_endpoint(1, 3, 1, 1, 0); /* interrupt in, 16 bytes, 1 bank */
        UEIENX |= (1<<TXINE);
        usb_init_endpoint(2, 3, 0, 1, 0); /* interrupt out, 16 bytes, 1 bank */
        UEIENX |= (1<<RXOUTE);

        UENUM   = 0;
        UEINTX &= ~(1<<TXINI); // sende ZLP (Erfolg, löscht EP-Bank)
        while (!(UEINTX & (1 << TXINI)))
            ;
        break;

    default:
        UECONX |= (1<<STALLRQ);
        break;
    }
}

void usb_send_descriptor(const uint8_t *d, uint8_t len) {
    uint8_t oldlen = len;

    while (len) {
        while (!(UEINTX & (1<<TXINI)))
            if (UEINTX & (1<<NAKOUTI))
                break; /* host wants to cancel */

        uint8_t j = (len > 8 ? 8 : len);
        while (j--)
            UEDATX = pgm_read_byte(d++);
        len = (len > 8) ? len-8 : 0;

        if (UEINTX & (1<<NAKOUTI))
            break;

        UEINTX &= ~(1<<TXINI);
    }

    if (((oldlen&0x7) == 0) && !(UEINTX & (1<<NAKOUTI))) {
        while (!(UEINTX & (1<<TXINI)))
            ; /* wait */
        UEINTX &= ~(1<<TXINI);
    }

    while (!(UEINTX & (1<<NAKOUTI)))
        ; /* wait for host ack */

    UEINTX &= ~(1<<NAKOUTI);
    UEINTX &= ~(1<<RXOUTI);
}

void usb_ep_in(volatile uint8_t *buf, volatile uint8_t *count) {
    if (!(UEINTX & (1<<TXINI)))
        return;
    UEINTX &= ~(1<<TXINI);

    uint8_t n = *count;
    for (int i=0; i<n; i++)
        UEDATX = buf[i];
    *count = 0;

    UEINTX &= ~(1<<FIFOCON); /* release fifo */
}

void usb_ep_out(volatile uint8_t *buf, volatile uint8_t *count) {
    if (!(UEINTX & (1<<RXOUTI)))
        return;
    UEINTX &= ~(1<<RXOUTI); /* send ACK */

    uint8_t i = UEBCLX, n = i;
    while (i--)
        *buf++ = UEDATX;
    *count = n;

    UEINTX &= ~(1<<FIFOCON); /* release fifo */
}
