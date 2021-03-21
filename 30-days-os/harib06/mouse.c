/* }EXÖW */

#include "bootpack.h"

struct FIFO8 mousefifo;

void inthandler2c(int *esp)
/* ©PS/2l?If */
{
	unsigned char data;
	io_out8(PIC1_OCW2, 0x64);	/* ÊmPIC1 IRQ-12ó®¬ */
	io_out8(PIC0_OCW2, 0x62);	/* ÊmPIC0 IRQ-02ó®¬ */
	data = io_in8(PORT_KEYDAT);
	fifo8_put(&mousefifo, data);
	return;
}

#define KEYCMD_SENDTO_MOUSE		0xd4
#define MOUSECMD_ENABLE			0xf4

void enable_mouse(struct MOUSE_DEC *mdec)
{
	/* l? */
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	
	/* @Ê??T§?H?wß0xd4,ºê¢ï©???l?CpÈ
	   l?ïÔñCPUê¢ACK,¦0cfa.
	*/
	mdec->phase = 0; /* Ò0xfa?i */
	return;
}

int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat)
{
	if (mdec->phase == 0) {
		/* Òl?I0xfaó? */
		if (dat == 0xfa) {
			mdec->phase = 1;
		}
		return 0;
	}
	if (mdec->phase == 1) {
		/* Òl?Iæê? */
		if ((dat & 0xc8) == 0x08) {
			/* »fæê?¥ÛÝ?_?L½?I¥ÛÝ8~FV? */
			mdec->buf[0] = dat;
			mdec->phase = 2;
		}
		return 0;
	}
	if (mdec->phase == 2) {
		/* Òl?Iæñ?*/
		mdec->buf[1] = dat;
		mdec->phase = 3;
		return 0;
	}
	if (mdec->phase == 3) {
		/* Òl?IæO? */
		mdec->buf[2] = dat;
		mdec->phase = 1;
		// l??Ió?Ýá3Ê
		mdec->btn = mdec->buf[0] & 0x07;
		
		mdec->x = mdec->buf[1];
		mdec->y = mdec->buf[2];
		if ((mdec->buf[0] & 0x10) != 0) {
			mdec->x |= 0xffffff00;
		}
		if ((mdec->buf[0] & 0x20) != 0) {
			mdec->y |= 0xffffff00;
		}
		mdec->y = - mdec->y; /* l?yûüaæÊ½ */
		return 1;
	}
	return -1;
}
