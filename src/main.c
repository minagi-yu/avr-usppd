#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>
#include "uart.h"

#define ENABLE_TX() PORTD.OUTSET = PIN6_bm;
#define DISABLE_TX() PORTD.CLRSET = PIN6_bm;
#define ENABLE_RX() PORTD.OUTSET = PIN7_bm;
#define DISABLE_RX() PORTD.CLRSET = PIN7_bm;

enum k_code {
    K_CODE_SYNC1 = 0x11,
    K_CODE_SYNC2 = 0x12,
    K_CODE_SYNC3 = 0x13,
    K_CODE_RST1 = 0x21,
    K_CODE_RST2 = 0x22,
    K_CODE_EOP = 0x30,
    K_CODE_ERROR = 0xff
};

const uint8_t symbol[] = {
    K_CODE_ERROR,
    K_CODE_ERROR,
    K_CODE_ERROR,
    K_CODE_ERROR,
    K_CODE_ERROR,
    K_CODE_SYNC3,
    K_CODE_RST1,
    K_CODE_ERROR,
    0x1,
    0x4,
    0x5,
    K_CODE_ERROR,
    K_CODE_EOP,
    0x6,
    0x7,
    K_CODE_ERROR,
    K_CODE_SYNC2,
    0x8,
    0x9,
    0x2,
    0x3,
    0xa,
    0xb,
    K_CODE_SYNC1,
    K_CODE_RST2,
    0xc,
    0xd,
    0xe,
    0xf,
    0x0,
    K_CODE_ERROR
};

static uint16_t len;

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

static const uint8_t bmc[] = {
    0x33, 0x32, 0x34, 0x35, 0x2c, 0x2d, 0x2b, 0x2a,
    0x8c, 0x8d, 0x8b, 0x8a, 0x53, 0x52, 0x54, 0x55
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

int main(void)
{
    // メインクロックを24MHzに
    _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, CLKCTRL_CLKOUT_bm | CLKCTRL_FRQSEL_24M_gc);

    /* 受信設定 ---------------------------------------------------------------- */
    // アナログコンパレーターの初期化
    VREF.ACREF = VREF_REFSEL_2V048_gc;
    AC0.MUXCTRL = AC_INITVAL_HIGH_gc | AC_MUXPOS_AINP0_gc | AC_MUXNEG_DACREF_gc;
    AC0.DACREF = 44; // Vth = 0.35v
    AC0.CTRLA = AC_HYSMODE_SMALL_gc | AC_ENABLE_bm;
    // イベント0にAC0の出力を接続
    EVSYS.CHANNEL0 = EVSYS_CHANNEL0_AC0_OUT_gc;
    // イベント0をEVOUTA(PA2)ピンに出力（デバッグ用）
    EVSYS.USEREVSYSEVOUTA = EVSYS_USER_CHANNEL0_gc;

    // イベント0をTCB1の入力に設定
    EVSYS.USERTCB1CAPT = EVSYS_USER_CHANNEL0_gc;
    // TCB1を0.4usのワンショットに設定
    // TCB1.CTRLB = TCB_ASYNC_bm | TCB_CNTMODE_SINGLE_gc;
    TCB1.CTRLB = TCB_ASYNC_bm | TCB_CNTMODE_SINGLE_gc;
    TCB1.EVCTRL = TCB_EDGE_bm | TCB_CAPTEI_bm;
    TCB1.CNT = 10;
    TCB1.CCMP = 10;
    TCB1.CTRLA = TCB_ENABLE_bm;
    // イベント1をTCB1の出力に設定
    EVSYS.CHANNEL1 = EVSYS_CHANNEL1_TCB1_CAPT_gc;

    // イベント1をTCB2の入力に設定
    EVSYS.USERTCB2CAPT = EVSYS_USER_CHANNEL1_gc;
    // TCB2を2.2usのワンショットに設定
    PORTC.DIRSET = PIN0_bm;
    TCB2.CTRLB = TCB_ASYNC_bm | TCB_CCMPEN_bm | TCB_CNTMODE_SINGLE_gc; // PC0ピンに出力
    TCB2.EVCTRL = TCB_CAPTEI_bm;
    TCB2.CNT = 53;
    TCB2.CCMP = 53;
    TCB2.CTRLA = TCB_ENABLE_bm;

    // CCL LUTの2と3をD-FFに設定
    CCL.SEQCTRL1 = CCL_SEQSEL_DFF_gc;

    // CCL LUT2のEVENTAにEVENT0を接続
    EVSYS.USERCCLLUT2A = EVSYS_USER_CHANNEL0_gc;
    // CCL LUT2の入力0が1の時に出力が1（入力0のバッファ）（入力1, 2はDon't care）
    CCL.TRUTH2 = 0xAA;
    // CCL LUT2の入力2にTCB2出力
    CCL.LUT2CTRLC = CCL_INSEL2_TCB2_gc;
    // CCL LUT2の入力0にEVENTA
    CCL.LUT2CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_EVENTA_gc;
    // CCL LUT2のクロック（D-FFのクロック）は入力2（TCB2出力）
    CCL.LUT2CTRLA = CCL_CLKSRC_IN2_gc | CCL_ENABLE_bm;
    // EVEVT2をCCL LUT2の出力（D-FFの出力）に設定
    EVSYS.CHANNEL2 = EVSYS_CHANNEL2_CCL_LUT2_gc;
    // イベント2をEVOUTC(PC2)ピンへ出力（デバッグ用）
    EVSYS.USEREVSYSEVOUTC = EVSYS_USER_CHANNEL2_gc;

    // CCL LUT3（D-FFのゲート）は常に1出力
    CCL.TRUTH3 = 0xFF;
    CCL.LUT3CTRLC = CCL_INSEL2_MASK_gc;
    CCL.LUT3CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_MASK_gc;
    CCL.LUT3CTRLA = CCL_CLKSRC_CLKPER_gc | CCL_ENABLE_bm;

    // CCL LUT0のEVENTAにEVENT0を接続
    EVSYS.USERCCLLUT0A = EVSYS_USER_CHANNEL0_gc;
    // CCL LUT0のEVENTBにEVENT2を接続
    EVSYS.USERCCLLUT0B = EVSYS_USER_CHANNEL2_gc;
    // 入力1と入力2のXOR
    CCL.TRUTH0 = 0x06;
    // CCL LUT0の入力1にEVENTB、入力2にEVENTA
    CCL.LUT0CTRLC = CCL_INSEL2_MASK_gc;
    CCL.LUT0CTRLB = CCL_INSEL1_EVENTB_gc | CCL_INSEL0_EVENTA_gc;
    // CCL LUT0の出力をPA3へ出力
    PORTA.DIRSET = PIN3_bm;
    CCL.LUT0CTRLA = CCL_CLKSRC_CLKPER_gc | CCL_OUTEN_bm | CCL_ENABLE_bm;

    // 通信の終わり検知用タイマー
    // イベント0をTCB0の入力に設定
    EVSYS.USERTCB0CAPT = EVSYS_USER_CHANNEL0_gc;
    // 割り込み有効
    TCB0.EVCTRL = TCB_CAPTEI_bm;
    TCB0.INTCTRL = TCB_CAPT_bm;
    // 1bitぶんの時間以上の適当な時間
    TCB0.CCMP = 100;
    // タイムアウトモード
    TCB0.CTRLB = TCB_CNTMODE_TIMEOUT_gc;
    TCB0.CTRLA = TCB_CLKSEL_DIV2_gc | TCB_ENABLE_bm; // オーバーフロー割り込みの頻度低減のためDIV2

    // SPIの設定
    SPI0.CTRLA = SPI_ENABLE_bm;
    SPI0.CTRLB = SPI_BUFEN_bm | SPI_MODE_1_gc;
    SPI0.CTRLA = SPI_ENABLE_bm;
    SPI0.INTCTRL = SPI_RXCIE_bm | SPI_IE_bm;

    PORTC.DIRSET = PIN1_bm; // SS（デバッグ用）
    PORTC.OUTSET = PIN1_bm;
    PORTC.OUTCLR = PIN1_bm;
    /* 受信設定ここまで --------------------------------------------------------- */

    /* 送信設定 ---------------------------------------------------------------- */
    // Not(USART2 TXD)
    CCL.TRUTH3 = 0x0f;
    CCL.LUT3CTRLC = CCL_INSEL2_USART2_gc;
    // CCL.LUT3CTRLB = CCL_INSEL0_MASK_gc | CCL_INSEL1_MASK_gc;
    PORTC.DIRSET = PIN3_bm;
    CCL.LUT0CTRLA = CCL_CLKSRC_CLKPER_gc | CCL_OUTEN_bm | CCL_ENABLE_bm;

    /* 送信設定ここまで --------------------------------------------------------- */

    CCL.CTRLA = CCL_ENABLE_bm;

    PORTD.DIRSET = PIN6_bm | PIN7_bm;
    ENABLE_RX();

    len = 0;

    stdout = &mystdout;
    uart_init();

    for (;;) {
        // puts("Hello World");
        // _delay_ms(500);
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

ISR(TCB0_INT_vect)
{
    uint8_t count;
    count = 0;
    // タイムアウト
    if (len > 0) {
        SPI0.INTCTRL = 0;
        // 8ビットに満たない部分的データを受信
        while (!(SPI0.INTFLAGS & SPI_RXCIF_bm)) {
            // PC0(TCB2 OUT)はPA6(SPI0 SCK)に接続してあるのでSCKをトグル
            PORTC.PIN0CTRL = PORT_INVEN_bm;
            PORTC.PIN0CTRL = 0;
            _delay_us(1); // delay入れてないとINTFLAGSが立つのが間に合わない（別になくても動作に影響はない）
            count++;
        }
        SPI0.INTFLAGS = SPI_RXCIF_bm;
        if (count < 8)
            puthex(SPI0.DATA);
        else
            SPI0.DATA; // 空読み
        uart_putc('\n');
    }
    // SPIの初期化
    SPI0.CTRLA &= ~SPI_ENABLE_bm;
    SPI0.CTRLA |= SPI_ENABLE_bm;
    PORTC.OUTSET = PIN1_bm;
    PORTC.OUTCLR = PIN1_bm;
    SPI0.INTCTRL = SPI_RXCIE_bm | SPI_IE_bm;
    len = 0;

    TCB0.INTFLAGS |= TCB_CAPT_bm;
}
