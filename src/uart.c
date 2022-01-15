#include "uart.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include "fifo.h"

#define UART_CLK_PER F_CPU
#define UART_BAUD 115200

#define UART_TX_PORT PORTA
#define UART_TX_PIN 0
#define UART_RX_PORT PORTA
#define UART_RX_PIN 1

static FIFO rx_buff, tx_buff;

void uart_init(void)
{
    fifo_init(&rx_buff);
    fifo_init(&tx_buff);

    cli();

    UART_TX_PORT.DIRSET = (1 << UART_TX_PIN);

    UART_RX_PORT.DIRCLR = (1 << UART_RX_PIN);
    *((uint8_t *)&UART_RX_PORT.PIN0CTRL + UART_RX_PIN) &= ~PORT_PULLUPEN_bm;

    USART0.BAUD = ((float)(UART_CLK_PER * 64.0f / (16.0f * (float)UART_BAUD)) + 0.5f);

    USART0.CTRLA = USART_RXCIE_bm;
    USART0.CTRLB = (USART_RXEN_bm | USART_TXEN_bm);
    USART0.CTRLC = USART_CHSIZE_8BIT_gc;

    sei();
}

void uart_putc(uint8_t c)
{
    while (fifo_put(&tx_buff, c))
        ;

    USART0.CTRLA |= USART_DREIE_bm;
}

uint8_t uart_getc(void)
{
    uint8_t d;

    while (fifo_get(&rx_buff, &d))
        ;

    return d;
}

ISR(USART0_RXC_vect)
{
    uint8_t d;

    d = USART0.RXDATAL;

    fifo_put(&rx_buff, d);
}

ISR(USART0_DRE_vect)
{
    uint8_t d;

    if (fifo_get(&tx_buff, &d)) {
        USART0.CTRLA &= ~USART_DREIE_bm;
    } else {
        USART0.TXDATAL = d;
    }
}
