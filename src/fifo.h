#ifndef FIFO_H
#define FIFO_H

#include <stdbool.h>
#include <stdint.h>

#define FIFO_BUFFER_SIZE 64

typedef volatile struct fifo {
    uint8_t buffer[FIFO_BUFFER_SIZE];
    uint8_t put_idx;
    uint8_t get_idx;
    uint8_t len;
} FIFO;

void fifo_init(FIFO *fifo);
bool fifo_get(FIFO *fifo, uint8_t *data);
bool fifo_put(FIFO *fifo, uint8_t data);

#endif
