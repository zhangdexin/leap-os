#include "bootpack.h"
#include <stdio.h>


#define MEMMAN_FREES 4090	/* free[]大约32KB(8 * 4090) */
#define MEMMAN_ADDR	 0x003c0000 // 内存信息表存放在0x003c0000

// 可用信息
struct FREEINFO { 
	unsigned int addr, size;
};

// 内存管理
struct MEMMAN {
	int frees, maxfrees, lostsize, losts;
	struct FREEINFO free[MEMMAN_FREES];
};

void memman_init(struct MEMMAN* man) {
	man->frees = 0;    // 可用信息数目
	man->maxfrees = 0; // 用于观察可用状况，frees的最大值
	man->lostsize = 0; // 释放失败的内存的大小总和
	man->losts = 0;    // 释放失败的次数
}

// 内存剩余多少
unsigned int memman_total(struct MEMMAN* man) {
	unsigned int i, t = 0;
	for (i = 0; i < man->frees; ++i) {
		t += man->free[i].size;
	}
	return t;
}

// 分配内存
unsigned int memman_alloc(struct MEMMAN* man, unsigned int size) {
	unsigned int i, a;
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].size >= size) {
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0) {
				man->frees--;
				for (; i < man->frees; i++) {
					man->free[i] = man->free[i + 1];
				}
			}
			
			return a;
		}
	}
	return 0;
}

// 释放内存
int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size) {
	int i, j;
	// 为便于归纳内存，将free[]按照addr的顺序排列
	// 先决定放在哪里
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].addr > addr) {
			break;
		}
	}
	
	// 前边有可用内存
	if (i > 0) {
		// 如果前一块内存容量正好和指定内存地址可以拼接，则并入前一块内存
		if (man->free[i - 1].addr + man->free[i - 1].size == addr) {
			man->free[i - 1].size += size;
			if (i < man->frees) { // 如果指定内存位置不是在最后一位
				if (addr + size == man->free[i].addr) { // 检查是否也可以和后一块内存拼接上
					man->free[i - 1].size += man->free[i].size;
					
					// 删除第i块内存，并前移后边内存块
					man->frees--;
					for (; i < man->frees; i++) {
						man->free[i] = man->free[i + 1];
					}
				}
			}
			
			return 0;
		}
	}
	
	// 执行完前边观察i的值，发现不能合并相应的块
	if (i < man->frees) {
		// 查看是否能与后边块合并
		if (addr + size == man->free[i].addr) {
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0;
		}
	}
	
	if (man->frees < MEMMAN_FREES) {
		// free[i]之后向后移动
		for (j = man->frees; j > i; j--) {
			man->free[j] = man->free[j - 1];
		}
		man->frees++;
		if (man->maxfrees < man->frees) {
			man->maxfrees = man->frees; // 更新最大值
		}
		
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0;
	}
	
	
	// 释放失败了
	man->losts++;
	man->lostsize += size;
	return -1;
}


#define EFLAG_AC_BIT      0x00040000
#define CR0_CACHE_DISABLE 0x60000000

unsigned int memtest(unsigned int start, unsigned int end) {
	char flg486 = 0;
	unsigned int eflg, cr0, i;
	
	eflg = io_load_eflags();
	eflg |= EFLAG_AC_BIT;
	io_store_eflags(eflg);
	
	eflg = io_load_eflags();
	
	// EFLAGS寄存器第18位即所谓的AC标志位，cpu是386，就没有这个标志位
	// 如果是386，即使设定AC=1,AC的值还会自动回到0
	if ((eflg & EFLAG_AC_BIT) != 0) {
		flg486 = 1;
	}
	
	// 恢复
	eflg &= ~EFLAG_AC_BIT;
	io_store_eflags(eflg);
	
	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; // 禁止缓存，CR0寄存器设置还可以切换成保护模式，见asmhead
		store_cr0(cr0);
	}
	
	i = memtest_sub(start, end);
	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; // 允许缓存
		store_cr0(cr0);
	}
	
	return i;
}


// ???HarMain
void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40], mcursor[256], keybuf[32], mousebuf[128];
	int mx, my, i;
	struct MOUSE_DEC mdec;
	
	unsigned int memtotal;
	struct MEMMAN* memman = (struct MEMMAN*)MEMMAN_ADDR;

	// 初始化中断
	init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PIC初始化完成，开发CPU中断 */
	fifo8_init(&keyfifo, 32, keybuf);
	fifo8_init(&mousefifo, 128, mousebuf);
	io_out8(PIC0_IMR, 0xf9); /*开发PIC1和键盘中断(11111001) */
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */

	init_keyboard();
	enable_mouse(&mdec);
	
	// 初始化内存管理
	memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000);
	
	// 初始化画面
	init_palette();
	init_screen8(binfo->vram, binfo->scrnx, binfo->scrny);
	
	/* 初始时鼠标绘制在画面中央*/
	mx = (binfo->scrnx - 16) / 2; 
	my = (binfo->scrny - 28 - 16) / 2;
	
	init_mouse_cursor8(mcursor, COL8_008484);
	putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16);
	
	// 鼠标的坐标值
	sprintf(s, "(%3d, %3d)", mx, my);
	putfonts8_asc(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, s);

	// 内存的容量
	sprintf(s, "memory %dMB   free : %dKB",
			memtotal / (1024 * 1024), memman_total(memman) / 1024);
	putfonts8_asc(binfo->vram, binfo->scrnx, 0, 32, COL8_FFFFFF, s);
	
	for (;;) {
		io_cli();
		if (fifo8_status(&keyfifo) + fifo8_status(&mousefifo) == 0) {
			io_stihlt();
		} else {
			if (fifo8_status(&keyfifo) != 0) {
				i = fifo8_get(&keyfifo);
				io_sti();
				sprintf(s, "%02X", i);
				boxfill8(binfo->vram, binfo->scrnx, COL8_008484,  0, 16, 15, 31);
				putfonts8_asc(binfo->vram, binfo->scrnx, 0, 16, COL8_FFFFFF, s);
			} else if (fifo8_status(&mousefifo) != 0) {
				i = fifo8_get(&mousefifo);
				io_sti();
				if (mouse_decode(&mdec, i) != 0) {
					/* 数据3个字节集齐 */
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					if ((mdec.btn & 0x01) != 0) {
						s[1] = 'L';
					}
					if ((mdec.btn & 0x02) != 0) {
						s[3] = 'R';
					}
					if ((mdec.btn & 0x04) != 0) {
						s[2] = 'C';
					}
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
					putfonts8_asc(binfo->vram, binfo->scrnx, 32, 16, COL8_FFFFFF, s);
					
					// 鼠标图案清除
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, mx, my, mx + 15, my + 15);
					mx += mdec.x;
					my += mdec.y;
					if (mx < 0) {
						mx = 0;
					}
					if (my < 0) {
						my = 0;
					}
					if (mx > binfo->scrnx - 16) {
						mx = binfo->scrnx - 16;
					}
					if (my > binfo->scrny - 16) {
						my = binfo->scrny - 16;
					}
					sprintf(s, "(%3d, %3d)", mx, my);
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 0, 79, 15); /* 鼠标坐标清楚 */
					putfonts8_asc(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, s); /* 鼠标坐标绘制 */
					putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16); /* 鼠标图案绘制 */
				}
			}
		}
	}
}
