#include "pd_dev.h"

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

static void pd_dev_init_recv(void)
{
    VREF.ACREF = VREF_REFSEL_2V048_gc;
    AC1.MUXCTRL = AC_INITVAL_HIGH_gc | AC_MUXPOS_AINP0_gc | AC_MUXNEG_DACREF_gc;
    AC1.DACREF = 44; // Vth = 0.35v
    AC1.CTRLA = AC_HYSMODE_SMALL_gc | AC_ENABLE_bm;
    EVSYS.CHANNEL0 = EVSYS_CHANNEL0_AC1_OUT_gc;
#ifdef DEBUG
    EVSYS.USEREVSYSEVOUTC = EVSYS_USER_CHANNEL0_gc;
#endif

    EVSYS.USERTCB0CAPT = EVSYS_USER_CHANNEL0_gc;
    PORTA.DIRSET = PIN2_bm;
    TCB0.CTRLB = TCB_ASYNC_bm | TCB_CCMPEN_bm | TCB_CNTMODE_SINGLE_gc;
    TCB0.EVCTRL = TCB_EDGE_bm | TCB_CAPTEI_bm;
    TCB0.CNT = 48; // 2.0us
    TCB0.CCMP = 48;
    TCB0.CTRLA = TCB_ENABLE_bm;
    EVSYS.CHANNEL1 = EVSYS_CHANNEL1_TCB0_CAPT_gc;
}

static void pd_dev_init_send(void)
{
}

void pd_dev_init(void)
{
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
}
