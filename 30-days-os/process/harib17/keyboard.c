#include "bootpack.h"

struct FIFO32* keyfifo;
int keydata0;

// PORT_KEYDAT,0x0060的设备的8位就是按键编码
// 0x60+IRQ号码输出给OCW2表示某个IRQ处理完成
void inthandler21(int *esp)
{
	int data;
	io_out8(PIC0_OCW2, 0x61);	/* 通知PIC"IRQ-01"已经受理完成 */
	data = io_in8(PORT_KEYDAT);
	fifo32_put(keyfifo, data + keydata0);
	return;
}

#define PORT_KEYSTA				0x0064
#define KEYSTA_SEND_NOTREADY	0x02
#define KEYCMD_WRITE_MODE		0x60
#define KBC_MODE				0x47

void wait_KBC_sendready(void)
{
	/* 等待键盘控制电路准备完成 */
	for (;;) {
		if ((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0) {
			break;
		}
	}
	return;
}

// 鼠标控制电路包含在键盘控制电路里，如果键盘控制电路初始化正常完成，鼠标电路控制器激活完成
// 一边确认可否往键控制电路发送信息，一边送模式设定指令
// 模式设定指令是0x60,利用鼠标模式号码是0x47
void init_keyboard(struct FIFO32* fifo, int data0)
{
	// fifio缓冲区信息保存全局变量
	keyfifo = fifo;
	keydata0 = data0;

	/* 初始化键盘控制电路 */
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, KBC_MODE);
	return;
}