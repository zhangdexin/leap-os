/* マウス関係 */

#include "bootpack.h"

struct FIFO32* mousefifo;
int mousedata0;

void inthandler2c(int* esp)
/* 来自PS/2鼠标的中断 */
{
	int data;
	io_out8(PIC1_OCW2, 0x64);	/* 通知PIC1 IRQ-12受理完成 */
	io_out8(PIC0_OCW2, 0x62);	/* 通知PIC0 IRQ-02受理完成 */
	data = io_in8(PORT_KEYDAT);
	fifo32_put(mousefifo, data + mousedata0);
	return;
}

#define KEYCMD_SENDTO_MOUSE		0xd4
#define MOUSECMD_ENABLE			0xf4

void enable_mouse(struct FIFO32* fifo, int data0, struct MOUSE_DEC *mdec)
{
	// fifio缓冲区信息保存在全局变量
	mousefifo = fifo;
	mousedata0 = data0;

	/* 激活鼠标 */
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	
	/* 如果往键盘控制电路送指令0xd4,下一个数据会自动发送给鼠标，用以激活
	   鼠标会返回CPU一个ACK,即0xfa.
	*/
	mdec->phase = 0; /* 等待0xfa阶段 */
	return;
}

int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat)
{
	if (mdec->phase == 0) {
		/* 等待鼠标的0xfa状态 */
		if (dat == 0xfa) {
			mdec->phase = 1;
		}
		return 0;
	}
	if (mdec->phase == 1) {
		/* 等待鼠标的第一字节 */
		if ((dat & 0xc8) == 0x08) {
			/* 判断第一字节点节是否在8~F之间 */
			mdec->buf[0] = dat;
			mdec->phase = 2;
		}
		return 0;
	}
	if (mdec->phase == 2) {
		/* 等待鼠标的第二字节*/
		mdec->buf[1] = dat;
		mdec->phase = 3;
		return 0;
	}
	if (mdec->phase == 3) {
		/* 等待鼠标的第三字节 */
		mdec->buf[2] = dat;
		mdec->phase = 1;
		// 鼠标键的状态在低3位，取出来
		mdec->btn = mdec->buf[0] & 0x07;
		
		mdec->x = mdec->buf[1];
		mdec->y = mdec->buf[2];
		if ((mdec->buf[0] & 0x10) != 0) {
			mdec->x |= 0xffffff00;
		}
		if ((mdec->buf[0] & 0x20) != 0) {
			mdec->y |= 0xffffff00;
		}
		mdec->y = - mdec->y; /* 鼠标y方向和画面符号相反 */
		return 1;
	}
	return -1;
}
