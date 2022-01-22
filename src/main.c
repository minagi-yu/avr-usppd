#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>
#include "uart.h"

static uint16_t len;

static int uart_putchar(char c, FILE *stream)
{
    uart_putc(c);
    return 0;
}
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

int main(void)
{
    // メインクロックを24MHzに
    _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, CLKCTRL_CLKOUT_bm | CLKCTRL_FRQSEL_24M_gc);

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
    PORTA.DIRSET = PIN3_bm;                                            // デバッグ用
    TCB1.CTRLB = TCB_ASYNC_bm | TCB_CCMPEN_bm | TCB_CNTMODE_SINGLE_gc; // PA3ピンに出力（デバッグ用）
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

    // CCL LUTの0と1をD-FFに設定
    CCL.SEQCTRL0 = CCL_SEQSEL_DFF_gc;

    // CCL LUT0のEVENTAにEVENT0を接続
    EVSYS.USERCCLLUT0A = EVSYS_USER_CHANNEL0_gc;
    // CCL LUT0の入力0が1の時に出力が1（入力0のバッファ）（入力1, 2はDon't care）
    CCL.TRUTH0 = 0xAA;
    // CCL LUT0の入力2にTCB2出力
    CCL.LUT0CTRLC = CCL_INSEL2_TCB2_gc;
    // CCL LUT0の入力0にEVENTA
    CCL.LUT0CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_EVENTA_gc;
    // CCL LUT0のクロック（D-FFのクロック）は入力2（TCB2出力）
    CCL.LUT0CTRLA = CCL_CLKSRC_IN2_gc | CCL_ENABLE_bm;
    // EVEVT2をCCL LUT0の出力（D-FFの出力）に設定
    EVSYS.CHANNEL2 = EVSYS_CHANNEL2_CCL_LUT0_gc;
    // イベント2をEVOUTC(PC2)ピンへ出力（デバッグ用）
    EVSYS.USEREVSYSEVOUTC = EVSYS_USER_CHANNEL2_gc;

    // CCL LUT1（D-FFのゲート）は常に1出力
    CCL.TRUTH1 = 0xFF;
    CCL.LUT1CTRLC = CCL_INSEL2_MASK_gc;
    CCL.LUT1CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_MASK_gc;
    CCL.LUT1CTRLA = CCL_CLKSRC_CLKPER_gc | CCL_ENABLE_bm;

    // CCL LUT2のEVENTAにEVENT0を接続
    EVSYS.USERCCLLUT2A = EVSYS_USER_CHANNEL0_gc;
    // CCL LUT2のEVENTBにEVENT2を接続
    EVSYS.USERCCLLUT2B = EVSYS_USER_CHANNEL2_gc;
    // 入力1と入力2のXOR
    CCL.TRUTH2 = 0x06;
    // CCL LUT2の入力1にEVENTB、入力2にEVENTA
    CCL.LUT2CTRLC = CCL_INSEL2_MASK_gc;
    CCL.LUT2CTRLB = CCL_INSEL1_EVENTB_gc | CCL_INSEL0_EVENTA_gc;
    // CCL LUT2の出力をPD3へ出力
    PORTD.DIRSET = PIN3_bm;
    CCL.LUT2CTRLA = CCL_CLKSRC_CLKPER_gc | CCL_OUTEN_bm | CCL_ENABLE_bm;

    CCL.CTRLA = CCL_ENABLE_bm;

    // 通信の終わり検知用タイマー
    // イベント0をTCB0の入力に設定
    EVSYS.USERTCB0CAPT = EVSYS_USER_CHANNEL0_gc;
    // 割り込み有効
    TCB0.EVCTRL = TCB_CAPTEI_bm;
    TCB0.INTCTRL = TCB_CAPT_bm;
    // 1bitぶんの時間以上の適当な時間
    TCB0.CCMP = 200;
    // タイムアウトモード
    TCB0.CTRLB = TCB_CNTMODE_TIMEOUT_gc;
    TCB0.CTRLA = TCB_ENABLE_bm;

    // SPIの設定
    SPI0.CTRLA = SPI_ENABLE_bm;
    SPI0.CTRLB = SPI_BUFEN_bm | SPI_MODE_1_gc;
    SPI0.INTCTRL = SPI_RXCIE_bm | SPI_IE_bm;

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
    uart_putc(d);
    len++;

    SPI0.INTFLAGS = SPI_RXCIF_bm;
}

ISR(TCB0_INT_vect)
{
    // タイムアウト
    if (len > 0) {
        SPI0.INTCTRL = 0;
        // 8ビットに満たない部分的データを受信
        do {
            // PC0(TCB2 OUT)はPA6(SPI0 SCK)に接続してあるのでSCKをトグル
            PORTC.PIN0CTRL = PORT_INVEN_bm;
            PORTC.PIN0CTRL = 0;
        } while (SPI0.INTFLAGS & SPI_RXCIF_bm);
        SPI0.INTFLAGS = SPI_RXCIF_bm;
        uart_putc(SPI0.DATA);
    }
    // SPIの初期化
    SPI0.CTRLA &= ~SPI_ENABLE_bm;
    SPI0.CTRLA |= SPI_ENABLE_bm;
    SPI0.INTCTRL = SPI_RXCIE_bm | SPI_IE_bm;
    len = 0;

    TCB0.INTFLAGS |= TCB_CAPT_bm;
}