/* FIFOCu */

#include "bootpack.h"

#define FLAGS_OVERRUN		0x0001

void fifo8_init(struct FIFO8 *fifo, int size, unsigned char *buf)
/* FIFOú» */
{
	fifo->size = size;
	fifo->buf = buf;
	fifo->free = size; /* ?tæå¬ */
	fifo->flags = 0;
	fifo->p = 0; /* ºê¢ÊÊu */
	fifo->q = 0; /* ºê¢?æÊu */
	return;
}

int fifo8_put(struct FIFO8 *fifo, unsigned char data)
/* üFIFO?Û¶ */
{
	if (fifo->free == 0) {
		/* ìo */
		fifo->flags |= FLAGS_OVERRUN;
		return -1;
	}
	fifo->buf[fifo->p] = data;
	fifo->p++;
	if (fifo->p == fifo->size) {
		fifo->p = 0;
	}
	fifo->free--;
	return 0;
}

int fifo8_get(struct FIFO8 *fifo)
/* FIFO?æ? */
{
	int data;
	if (fifo->free == fifo->size) {
		/* ?tæ¥ó */
		return -1;
	}
	data = fifo->buf[fifo->q];
	fifo->q++;
	if (fifo->q == fifo->size) {
		fifo->q = 0;
	}
	fifo->free++;
	return data;
}

int fifo8_status(struct FIFO8 *fifo)
/* ??¹½­ */
{
	return fifo->size - fifo->free;
}
