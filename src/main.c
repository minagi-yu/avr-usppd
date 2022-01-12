#include <avr/io.h>

int main(void)
{
    // メインクロックを24MHzに
    _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, CLKCTRL_CLKOUT_bm | CLKCTRL_FRQSEL_24M_gc);

    // アナログコンパレーターの初期化
    VREF.ACREF = VREF_REFSEL_2V048_gc;
    AC0.MUXCTRL = AC_INITVAL_HIGH_gc | AC_MUXPOS_AINP0_gc | AC_MUXNEG_DACREF_gc;
    AC0.DACREF = 40; // Vth = 0.32v
    AC0.CTRLA = AC_ENABLE_bm;
    // イベント0にAC0の出力を接続
    EVSYS.CHANNEL0 = EVSYS_CHANNEL0_AC0_OUT_gc;
    // イベント0をEVOUTA(PA2)ピンに出力（デバッグ用）
    EVSYS.USEREVSYSEVOUTA = EVSYS_USER_CHANNEL0_gc;

    // イベント0をTCB1の入力に設定
    EVSYS.USERTCB1CAPT = EVSYS_USER_CHANNEL0_gc;
    // TCB1を0.83usのワンショットに設定
    // TCB1.CTRLB = TCB_ASYNC_bm | TCB_CNTMODE_SINGLE_gc;
    PORTA.DIRSET = PIN3_bm; // デバッグ用
    TCB1.CTRLB = TCB_ASYNC_bm | TCB_CCMPEN_bm | TCB_CNTMODE_SINGLE_gc; // PA3ピンに出力（デバッグ用）
    TCB1.EVCTRL = TCB_EDGE_bm | TCB_CAPTEI_bm;
    TCB1.CNT = 18;
    TCB1.CCMP = 18;
    TCB1.CTRLA = TCB_ENABLE_bm;
    // イベント1をTCB1の出力に設定
    EVSYS.CHANNEL1 = EVSYS_CHANNEL1_TCB1_CAPT_gc;
}