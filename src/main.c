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
    // TCB1を0.83us(20クロック)のワンショットに設定
    // TCB1.CTRLB = TCB_ASYNC_bm | TCB_CNTMODE_SINGLE_gc;
    PORTA.DIRSET = PIN3_bm; // デバッグ用
    TCB1.CTRLB = TCB_ASYNC_bm | TCB_CCMPEN_bm | TCB_CNTMODE_SINGLE_gc; // PA3ピンに出力（デバッグ用）
    TCB1.EVCTRL = TCB_EDGE_bm | TCB_CAPTEI_bm;
    TCB1.CNT = 15;
    TCB1.CCMP = 15;
    TCB1.CTRLA = TCB_ENABLE_bm;
    // イベント1をTCB1の出力に設定
    EVSYS.CHANNEL1 = EVSYS_CHANNEL1_TCB1_CAPT_gc;

    // イベント1をTCB2の入力に設定
    EVSYS.USERTCB2CAPT = EVSYS_USER_CHANNEL1_gc;
    // TCB2を1.67us(40クロック)のワンショットに設定
    PORTC.DIRSET = PIN0_bm;
    TCB2.CTRLB = TCB_ASYNC_bm | TCB_CCMPEN_bm | TCB_CNTMODE_SINGLE_gc; // PC0ピンに出力
    TCB2.EVCTRL = TCB_CAPTEI_bm;
    TCB2.CNT = 38;
    TCB2.CCMP = 38;
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

    // CCL LUT1は常に0出力
    CCL.TRUTH1 = 0xFF;
    CCL.LUT1CTRLC = CCL_INSEL2_MASK_gc;
    CCL.LUT1CTRLB = CCL_INSEL1_MASK_gc | CCL_INSEL0_MASK_gc;
    CCL.LUT1CTRLA = CCL_CLKSRC_CLKPER_gc | CCL_ENABLE_bm;

    CCL.CTRLA = CCL_ENABLE_bm;

    for (;;) {
    }

}
