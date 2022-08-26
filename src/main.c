#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>
#include "uart.h"

enum k_code {
    K_CODE_SYNC1 = 0x10,
    K_CODE_SYNC2 = 0x11,
    K_CODE_RST1 = 0x12,
    K_CODE_RST2 = 0x13,
    K_CODE_EOP = 0x14,
    K_CODE_RESERVED15 = 0x15,
    K_CODE_RESERVED16 = 0x16,
    K_CODE_RESERVED17 = 0x17,
    K_CODE_RESERVED18 = 0x18,
    K_CODE_RESERVED19 = 0x19,
    K_CODE_RESERVED1A = 0x1A,
    K_CODE_SYNC3 = 0x1B,
    K_CODE_RESERVED1C = 0x1C,
    K_CODE_RESERVED1D = 0x1D,
    K_CODE_RESERVED1E = 0x1E,
    K_CODE_RESERVED1F = 0x1F,
};

static uint16_t len;
static volatile uint16_t msgid;

static int uart_putchar(char c, FILE *stream)
{
    uart_putc(c);
    return 0;
}
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

static const char *message[] = {
    "555555555555555518C71929EFEF56EEF52DB0",           //2
    "555555555555555518C712A9F25965D53FD25173AD79BAB0", //3
    "555555555555555518C71928AFF676FD75CAB0",           //6
    "555555555555555518C719294F2B52D76EEDB0",           //8
    "555555555555555518C712A8B24ADE5ABEA53E56E5A7A7B0", //9
    "555555555555555518C71929CF3A72B4EE4AB0",           //12
    "555555555555555518C719292FCA5477B9CDB0"            //14
};

// 外部回路で反転しているため、事前に反転しておく
static const uint8_t bmc[] = {
    ~0x33, ~0x32, ~0x34, ~0x35, ~0x2c, ~0x2d, ~0x2b, ~0x2a,
    ~0x4c, ~0x4d, ~0x4b, ~0x4a, ~0x53, ~0x52, ~0x54, ~0x55
};
static const uint8_t symbol4b5b[] = {
    0x0F, 0x12, 0x05, 0x15, 0x0A, 0x1A, 0x0E, 0x1E,
    0x09, 0x19, 0x0D, 0x1D, 0x0B, 0x1B, 0x07, 0x17,
    0x03, 0x11, 0x1C, 0x13, 0x16, 0x00, 0x10, 0x08,
    0x18, 0x04, 0x14, 0x0C, 0x02, 0x06, 0x01, 0x1F
};
const uint8_t symbol5b4b[] = {
    K_CODE_RESERVED15,
    K_CODE_RESERVED1E,
    K_CODE_RESERVED1C,
    K_CODE_SYNC1,
    K_CODE_RESERVED19,
    0x02,
    K_CODE_RESERVED1D,
    0x0E,
    K_CODE_RESERVED17,
    0x08,
    0x04,
    0x0C,
    K_CODE_SYNC3,
    0x0A,
    0x06,
    0x00,
    K_CODE_RESERVED16,
    K_CODE_SYNC2,
    0x01,
    K_CODE_RST2,
    K_CODE_RESERVED1A,
    0x03,
    K_CODE_EOP,
    0x0F,
    K_CODE_RESERVED18,
    0x09,
    0x05,
    0x0D,
    K_CODE_RST1,
    0x0B,
    0x07,
    K_CODE_RESERVED1F,
};

static void puthex(uint8_t val)
{
    uint8_t vh, vl;
    vh = val >> 4;
    vl = val & 0x0f;

    if (vh > 9) {
        uart_putc(vh - 10 + 'A');
    } else {
        uart_putc(vh + '0');
    }
    if (vl > 9) {
        uart_putc(vl - 10 + 'A');
    } else {
        uart_putc(vl + '0');
    }
}

void uart2_send(uint8_t data)
{
    while (!(USART2.STATUS & USART_DREIF_bm))
        ;
    USART2.TXDATAL = data;
}

void pd_phy_4bto5b(uint8_t *data, uint_fast8_t len, uint8_t **converted, uint_fast8_t *bitpos)
{
    uint8_t nibble;
    uint_fast8_t count;

    count = len * 2;
    do {
        nibble = (count % 2) ? (*data >> 4) : (*data & 0x0f);
        *bitpos %= 8;
        if (*bitpos < 3) {
            if (*bitpos == 0)
                **converted = 0;
            **converted |= symbol4b5b[nibble] << (3 - *bitpos);
        } else if (*bitpos == 3) {
            **converted |= symbol4b5b[nibble];
            *converted++;
        } else {
            **converted |= symbol4b5b[nibble] >> (*bitpos - 3);
            *converted++;
            **converted = 0;
            **converted |= symbol4b5b[nibble] << (*bitpos - 3);
        }
        *bitpos += 5;
    } while (--count);
}

void pd_phy_send(uint8_t *data, uint_fast8_t len)
{
    static uint8_t buffer[(20 + 20 + (40 * 7) + 40) / 8];
    uint8_t *p = buffer;
    uint_fast8_t bitpos = 0;
    uint_fast8_t count;
    bool inverce = true;

    // // Copy SOP
    // memcpy(&p, (uint8_t[]){ 0x18, 0xc7, 0x10 }, 3);
    // p += 4 * 5 / 8;
    // bitpos = 4 * 5 % 8;

    // // Copy data
    // pd_phy_4bto5b(data, len, &p, &bitpos);

    // // Calc CRC

    // // Copy CRC

    memcpy(buffer, data, 5 + len * 5 + 5);

    DISABLE_RX();
    cli();

    USART2.STATUS = USART_TXCIF_bm;

    // USART2.TXDATAL = 0xff;
    USART2.TXDATAL = ~0x2d;

    _delay_us(1);
    ENABLE_TX();

    // Send Preamble
    count = 64 * 2 / 8 - 1;
    do {
        uart2_send(~0x2d);
    } while (--count);

    // Send Payload
    count = (20 + (8 / 4 * 5 * len) + 40) / 8;
    p = buffer;
    do {
        uint8_t d;
        d = bmc[*p >> 4];
        uart2_send(inverce ? d : ~d);
        inverce ^= d & 0x01;
        d = bmc[*p & 0x0f];
        uart2_send(inverce ? d : ~d);
        inverce ^= d & 0x01;
        p++;
    } while (--count);

    // Send EOP
    if (inverce) {
        uart2_send(~0x4a);
        uart2_send(~0xc0);
    } else {
        uart2_send(0x4a);
        uart2_send(0xcf);
    }

    while (!(USART2.STATUS & USART_TXCIF_bm))
        ;
    DISABLE_TX();
    ENABLE_RX();
    sei();
}

int main(void)
{
    // メインクロックを24MHzに
    _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, CLKCTRL_CLKOUT_bm | CLKCTRL_FRQSEL_24M_gc);

    PORTD.DIRSET = PIN7_bm; // デバッグ用

    len = 0;
    msgid = 0;

    stdout = &mystdout;
    uart_init();

    for (;;) {
        // puts("Hello World");
        // _delay_ms(500);
        while (msgid != 1)
            ;
        pd_phy_send((uint8_t[]){ 0x18, 0xC7, 0x19, 0x29, 0xEF, 0xEF, 0x56, 0xEE, 0xF5, 0x2D }, 2);
        _delay_us(20);
        pd_phy_send((uint8_t[]){ 0x18, 0xC7, 0x12, 0xA9, 0xF2, 0x59, 0x65, 0xD5, 0x3F, 0xD2, 0x51, 0x73, 0xAD, 0x79, 0xBA }, 6);
        while (msgid != 3)
            ;
        pd_phy_send((uint8_t[]){ 0x18, 0xC7, 0x19, 0x28, 0xAF, 0xF6, 0x76, 0xFD, 0x75, 0xCA }, 2);
        while (msgid != 4)
            ;
        pd_phy_send((uint8_t[]){ 0x18, 0xC7, 0x19, 0x29, 0x4F, 0x2B, 0x52, 0xD7, 0x6E, 0xED }, 2);
        _delay_us(50);
        pd_phy_send((uint8_t[]){ 0x18, 0xC7, 0x12, 0xA8, 0xB2, 0x4A, 0xDE, 0x5A, 0xBE, 0xA5, 0x3E, 0x56, 0xE5, 0xA7, 0xA7 }, 6);
        while (msgid != 6)
            ;
        pd_phy_send((uint8_t[]){ 0x18, 0xC7, 0x19, 0x29, 0xCF, 0x3A, 0x72, 0xB4, 0xEE, 0x4A }, 2);
        while (msgid != 7)
            ;
        pd_phy_send((uint8_t[]){ 0x18, 0xC7, 0x19, 0x29, 0x2F, 0xCA, 0x54, 0x77, 0xB9, 0xCD }, 2);
        for (;;)
            ;
    }
}

ISR(SPI0_INT_vect)
{
    uint8_t d;

    d = SPI0.DATA;
    puthex(d);
    len++;

    SPI0.INTFLAGS = SPI_RXCIF_bm;
}

ISR(TCB1_INT_vect)
{
    uint8_t count;
    count = 0;
    // タイムアウト
    if (len > 0) {
        SPI0.INTCTRL = 0;
        // 8ビットに満たない部分的データを受信
        while (!(SPI0.INTFLAGS & SPI_RXCIF_bm)) {
            // PC0(TCB2 OUT)はPA6(SPI0 SCK)に接続してあるのでSCKをトグル
            PORTA.PIN2CTRL = PORT_INVEN_bm;
            PORTA.PIN2CTRL = 0;
            _delay_us(1); // delay入れてないとINTFLAGSが立つのが間に合わない（別になくても動作に影響はない）
            count++;
        }
        SPI0.INTFLAGS = SPI_RXCIF_bm;
        if (count < 8)
            puthex(SPI0.DATA);
        else
            SPI0.DATA; // 空読み
        // uart_putc('\t');
        msgid++;
        // uart_putc(msgid + '0');
        uart_putc('\n');
    }
    // SPIの初期化
    SPI0.CTRLA &= ~SPI_ENABLE_bm;
    SPI0.CTRLA |= SPI_ENABLE_bm;
    PORTC.OUTSET = PIN1_bm;
    PORTC.OUTCLR = PIN1_bm;
    SPI0.INTCTRL = SPI_RXCIE_bm | SPI_IE_bm;
    len = 0;

    TCB1.INTFLAGS |= TCB_CAPT_bm;
}
