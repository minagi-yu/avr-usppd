#include <avr/io.h>

int main(void)
{
    // メインクロックを24MHzに
    _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, CLKCTRL_FRQSEL_24M_gc);

    // アナログコンパレーターの初期化
    VREF.ACREF = VREF_REFSEL_2V048_gc;
    AC0.MUXCTRL = AC_INITVAL_HIGH_gc | AC_MUXPOS_AINP0_gc | AC_MUXNEG_DACREF_gc;
    AC0.DACREF = 40; // Vth = 0.32v
    AC0.CTRLA = AC_ENABLE_bm;
    // イベント0にAC0の出力を接続
    EVSYS.CHANNEL0 = EVSYS_CHANNEL0_AC0_OUT_gc;
    // イベント0をEVOUTA(PA2)ピンに出力（デバッグ用）
    EVSYS.USEREVSYSEVOUTA = EVSYS_USER_CHANNEL0_gc;
}