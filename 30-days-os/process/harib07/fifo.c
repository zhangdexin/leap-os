/* FIFOライブラリ */

#include "bootpack.h"

#define FLAGS_OVERRUN		0x0001

void fifo8_init(struct FIFO8 *fifo, int size, unsigned char *buf)
/* FIFO初期化 */
{
	fifo->size = size;
	fifo->buf = buf;
	fifo->free = size; /* 缓冲区大小 */
	fifo->flags = 0;
	fifo->p = 0; /* 下一个数据写去位置 */
	fifo->q = 0; /* 下一个数据?读取位置 */
	return;
}

int fifo8_put(struct FIFO8 *fifo, unsigned char data)
/* 向FIFO发送数据保存 */
{
	if (fifo->free == 0) {
		/* 溢出 */
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
/* FIFO读取数据 */
{
	int data;
	if (fifo->free == fifo->size) {
		/* 缓冲区是空 */
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
/* 积攒了多少数据 */
{
	return fifo->size - fifo->free;
}
