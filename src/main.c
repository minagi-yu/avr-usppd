#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>
#include "uart.h"

#define ENABLE_TX() PORTD.OUTSET = PIN0_bm;
#define DISABLE_TX() PORTD.OUTCLR = PIN0_bm;
#define ENABLE_RX()                   \
    do {                              \
        /* PORTD.OUTSET = PIN1_bm; */ \
        SPI0.CTRLA |= SPI_ENABLE_bm;  \
    } while (0)
#define DISABLE_RX()                  \
    do {                              \
        /* PORTD.OUTCLR = PIN1_bm; */ \
        SPI0.CTRLA &= ~SPI_ENABLE_bm; \
    } while (0)

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

static struct pd_packet {
    uint8_t sop[4];
    uint8_t message[(16 + (32 * 8)) / 8];
    uint32_t crc;
} pd_packet;

static bool reset_signal;
static uint_fast8_t shift = 0;

enum pd_phy_decode_state {
    SOP_DETECT,
    SOP_DECODE,
    MSG_DECODE,
    EOP_DECODE,
};

static enum pd_phy_decode_state state;

static void
pd_recv(uint8_t d)
{
    static uint8_t od;
    static uint_fast8_t numsop;
    // len++;
    switch (state) {
        case SOP_DETECT:
            for (shift = 0; shift < 8; shift++) {
                uint8_t tmp = (d << (8 - shift)) | (od >> shift);
                tmp &= 0xf8;
                if (tmp == 0xc0) { // Sync-1
                    reset_signal = false;
                    break;
                }
                if (tmp == 0x38) { // RST-1
                    reset_signal = true;
                    break;
                }
            }
            numsop = 0;
            /* Falls through. */
        case SOP_DECODE:
            pd_packet.sop[numsop++] = (d << (8 - shift)) | (od >> shift);
            if (numsop > 3) {
                state = MSG_DECODE;
            }
            break;
        case MSG_DECODE:
            break;
        case EOP_DECODE:
            break;
        default:
            break;
    }
    od = d;
}

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

    /* 受信設定 ---------------------------------------------------------------- */
    // アナログコンパレーターの初期化
    VREF.ACREF = VREF_REFSEL_2V048_gc;
    AC1.MUXCTRL = AC_INITVAL_HIGH_gc | AC_MUXPOS_AINP0_gc | AC_MUXNEG_DACREF_gc;
    AC1.DACREF = 44; // Vth = 0.35v
    AC1.CTRLA = AC_HYSMODE_SMALL_gc | AC_ENABLE_bm;
    // イベント0にAC1の出力を接続
    EVSYS.CHANNEL0 = EVSYS_CHANNEL0_AC1_OUT_gc;
    // イベント0をEVOUTC(PC2)ピンに出力（デバッグ用）
    EVSYS.USEREVSYSEVOUTC = EVSYS_USER_CHANNEL0_gc;

    // // イベント0をTCB1の入力に設定
    // EVSYS.USERTCB1CAPT = EVSYS_USER_CHANNEL0_gc;
    // // TCB1を0.4usのワンショットに設定
    // // TCB1.CTRLB = TCB_ASYNC_bm | TCB_CNTMODE_SINGLE_gc;
    // TCB1.CTRLB = TCB_ASYNC_bm | TCB_CNTMODE_SINGLE_gc;
    // TCB1.EVCTRL = TCB_EDGE_bm | TCB_CAPTEI_bm;
    // TCB1.CNT = 20;
    // TCB1.CCMP = 20;
    // TCB1.CTRLA = TCB_ENABLE_bm;
    // // イベント1をTCB1の出力に設定
    // EVSYS.CHANNEL1 = EVSYS_CHANNEL1_TCB1_CAPT_gc;

    // イベント0をTCB0の入力に設定
    EVSYS.USERTCB0CAPT = EVSYS_USER_CHANNEL0_gc;
    // TCB0を2usのワンショットに設定
    PORTA.DIRSET = PIN2_bm;
    TCB0.CTRLB = TCB_ASYNC_bm | TCB_CCMPEN_bm | TCB_CNTMODE_SINGLE_gc; // PC0ピンに出力
    TCB0.EVCTRL = TCB_EDGE_bm | TCB_CAPTEI_bm;
    TCB0.CNT = 48;
    TCB0.CCMP = 48;
    TCB0.CTRLA = TCB_ENABLE_bm;
    // TCB0出力をイベント1へ
    EVSYS.CHANNEL1 = EVSYS_CHANNEL1_TCB0_CAPT_gc;

    // CCL LUTの2と3をD-FFに設定
    CCL.SEQCTRL1 = CCL_SEQSEL_DFF_gc;

    // CCL LUT2のEVENTAにEVENT0を接続
    EVSYS.USERCCLLUT2A = EVSYS_USER_CHANNEL0_gc;
    // CCL LUT2の入力0が1の時に出力が1（入力0のバッファ）（入力1, 2はDon't care）
    CCL.TRUTH2 = 0xAA;
    // CCL LUT2の入力2にTCB2出力
    EVSYS.USERCCLLUT2B = EVSYS_USER_CHANNEL1_gc;
    CCL.LUT2CTRLC = CCL_INSEL2_EVENTB_gc;
    // CCL LUT2の入力0にEVENTA
    CCL.LUT2CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_EVENTA_gc;
    // CCL LUT2のクロック（D-FFのクロック）は入力2（TCB2出力）
    CCL.LUT2CTRLA = CCL_CLKSRC_IN2_gc | CCL_ENABLE_bm;
    // EVEVT2をCCL LUT2の出力（D-FFの出力）に設定
    EVSYS.CHANNEL2 = EVSYS_CHANNEL2_CCL_LUT2_gc;
    // イベント2をEVOUTC(PC2)ピンへ出力（デバッグ用）
    // EVSYS.USEREVSYSEVOUTC = EVSYS_USER_CHANNEL2_gc;

    // CCL LUT3（D-FFのゲート）は常に1出力
    CCL.TRUTH3 = 0xFF;
    CCL.LUT3CTRLC = CCL_INSEL2_MASK_gc;
    CCL.LUT3CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_MASK_gc;
    CCL.LUT3CTRLA = CCL_CLKSRC_CLKPER_gc | CCL_ENABLE_bm;

    // CCL LUT0のEVENTAにEVENT0を接続
    EVSYS.USERCCLLUT0A = EVSYS_USER_CHANNEL0_gc;
    // CCL LUT0のEVENTBにEVENT2を接続
    EVSYS.USERCCLLUT0B = EVSYS_USER_CHANNEL2_gc;
    // 入力1と入力2のXNOR
    CCL.TRUTH0 = 0x09;
    // CCL LUT0の入力1にEVENTB、入力2にEVENTA
    CCL.LUT0CTRLC = CCL_INSEL2_MASK_gc;
    CCL.LUT0CTRLB = CCL_INSEL1_EVENTB_gc | CCL_INSEL0_EVENTA_gc;
    // CCL LUT0の出力をPA3へ出力
    PORTA.DIRSET = PIN3_bm;
    CCL.LUT0CTRLA = CCL_CLKSRC_CLKPER_gc | CCL_OUTEN_bm | CCL_ENABLE_bm;

    // 通信の終わり検知用タイマー
    // イベント0をTCB1の入力に設定
    EVSYS.USERTCB1CAPT = EVSYS_USER_CHANNEL0_gc;
    // 割り込み有効
    TCB1.EVCTRL = TCB_CAPTEI_bm;
    TCB1.INTCTRL = TCB_CAPT_bm;
    // 1bitぶんの時間以上の適当な時間
    TCB1.CCMP = 100;
    // タイムアウトモード
    TCB1.CTRLB = TCB_CNTMODE_TIMEOUT_gc;
    TCB1.CTRLA = TCB_CLKSEL_DIV2_gc | TCB_ENABLE_bm; // オーバーフロー割り込みの頻度低減のためDIV2

    // SPIの設定
    SPI0.CTRLA = SPI_ENABLE_bm;
    SPI0.CTRLB = SPI_BUFEN_bm | SPI_MODE_1_gc;
    SPI0.CTRLA = SPI_ENABLE_bm;
    SPI0.INTCTRL = SPI_RXCIE_bm | SPI_IE_bm;

    // PORTC.DIRSET = PIN1_bm; // SS（デバッグ用）
    // PORTC.OUTSET = PIN1_bm;
    // PORTC.OUTCLR = PIN1_bm;
    /* 受信設定ここまで --------------------------------------------------------- */

    /* 送信設定 ---------------------------------------------------------------- */
    // Not(USART2 TXD)
    CCL.TRUTH1 = 0x0f;
    CCL.LUT1CTRLC = CCL_INSEL2_USART2_gc;
    // CCL.LUT3CTRLB = CCL_INSEL0_MASK_gc | CCL_INSEL1_MASK_gc;
    PORTC.DIRSET = PIN3_bm;
    CCL.LUT1CTRLA = CCL_CLKSRC_CLKPER_gc | CCL_OUTEN_bm | CCL_ENABLE_bm;

    USART2.BAUD = (F_CPU / 2 / 600000) << 6;
    // USART2.CTRLA = 0;
    USART2.CTRLB = USART_TXEN_bm;
    USART2.CTRLC = USART_CMODE_MSPI_gc; // MSB first
    PORTF.DIRSET = PIN0_bm;
    // USART2.BAUD = ((float)(F_CPU * 64.0f / (16.0f * (float)460800)) + 0.5f);
    // USART2.CTRLC = USART_CHSIZE_8BIT_gc;
    // USART2.CTRLB = (USART_RXEN_bm | USART_TXEN_bm);

    VREF.DAC0REF = VREF_REFSEL_2V048_gc;
    PORTD.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
    DAC0.CTRLA = DAC_OUTEN_bm | DAC_ENABLE_bm;
    // DATAレジスタは上位詰めなので注意！
    DAC0.DATA = (1200UL * 1024 / 2048) << 6; // 1.2V (1.2 * 1024 / 2.048)
    /* 送信設定ここまで --------------------------------------------------------- */

    CCL.CTRLA = CCL_ENABLE_bm;

    PORTD.DIRSET = PIN0_bm | PIN1_bm;
    ENABLE_RX();

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
