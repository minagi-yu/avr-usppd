#include "fifo.h"
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/atomic.h>

void fifo_init(FIFO *fifo)
{
    *fifo = (FIFO){ 0 };
}

bool fifo_get(FIFO *fifo, uint8_t *data)
{
    uint8_t idx;

    if (fifo->len <= 0)
        return 1;

    idx = fifo->get_idx;
    *data = fifo->buffer[idx];
    fifo->get_idx = (idx + 1) & (FIFO_BUFFER_SIZE - 1);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        fifo->len--;
    }

    return 0;
}

bool fifo_put(FIFO *fifo, uint8_t data)
{
    uint8_t idx;

    if (fifo->len >= FIFO_BUFFER_SIZE)
        return 1;

    idx = fifo->put_idx;
    fifo->buffer[idx] = data;
    fifo->put_idx = (idx + 1) & (FIFO_BUFFER_SIZE - 1);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        fifo->len++;
    }

    return 0;
}
