/* FIFOライブラリ */

#include "bootpack.h"

#define FLAGS_OVERRUN		0x0001

void fifo32_init(struct FIFO32 *fifo, int size, int *buf, struct TASK* task)
/* FIFO初期化 */
{
	fifo->size = size;
	fifo->buf = buf;
	fifo->free = size; /* 缓冲区大小 */
	fifo->flags = 0;
	fifo->p = 0; /* 下一个数据写去位置 */
	fifo->q = 0; /* 下一个数据读取位置 */
	fifo->task = task; // 有数据写入需要唤醒的任务
	return;
}

int fifo32_put(struct FIFO32 *fifo, int data)
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
	if (fifo->task != 0) {
		if (fifo->task->flags != 2) { // 不处于活动状态
			task_run(fifo->task, -1, 0); // 唤醒任务，level和priority不变
		}
	}

	return 0;
}

int fifo32_get(struct FIFO32 *fifo)
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

int fifo32_status(struct FIFO32 *fifo)
/* 积攒了多少数据 */
{
	return fifo->size - fifo->free;
}
