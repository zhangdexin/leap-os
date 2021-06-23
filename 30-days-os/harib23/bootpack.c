#include "bootpack.h"
#include <stdio.h>

// 控制键盘led灯亮，
#define KEYCMD_LED  0xed

void keywin_off(struct SHEET *key_win)
{
	change_wtitle8(key_win, 0);
	if ((key_win->flags & 0x20) != 0) {
		fifo32_put(&key_win->task->fifo, 3); /* 命令行窗口光标OFF */
	}
}

void keywin_on(struct SHEET *key_win)
{
	change_wtitle8(key_win, 1);
	if ((key_win->flags & 0x20) != 0) {
		fifo32_put(&key_win->task->fifo, 2); /* 命令行窗口光标ON */
	}
}

// ???HarMain
void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct FIFO32 fifo, keycmd;
	int fifobuf[128], keycmd_buf[32], *cons_fifo[2];
	char s[40];
	int mx, my, i, j;
	unsigned int memtotal;
	struct MOUSE_DEC mdec;
	struct MEMMAN* memman = (struct MEMMAN*)MEMMAN_ADDR;
	struct SHTCTL *shtctl;
	struct SHEET* sht_back, *sht_mouse, *sht_cons[2];
	unsigned char* buf_back, buf_mouse[256], *buf_cons[2];
	static char keytable0[0x80] = {
		0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0x08,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0x0a,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0,   0,   ']', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0x5c, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0x5c, 0,  0
	};
	static char keytable1[0x80] = {
		0,   0,   '!', 0x22, '#', '$', '%', '&', 0x27, '(', ')', '~', '=', '~', 0x08,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '`', '{', 0x0a,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', '+', '*', 0,   0,   '}', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   '_', 0,   0,   0,   0,   0,   0,   0,   0,   0,   '|', 0,   0
	};

	struct TASK *task_cons[2], *task_a, *task;
	int key_shift = 0, key_leds = (binfo->leds >> 4) & 7, keycmd_wait = -1;
	struct SHEET* sht = 0, *key_win;
	int x, y, mmx = -1, mmy = -1, mmx2 = 0, 
	new_mx = -1, new_my = 0, new_wx = 0x7fffffff, new_wy = 0;;

	// 初始化中断
	init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PIC初始化完成，开发CPU中断 */
    fifo32_init(&fifo, 128, fifobuf, 0);
	fifo32_init(&keycmd, 32, keycmd_buf, 0);

	init_pit(); // 初始化pit
	init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);
	io_out8(PIC0_IMR, 0xf8); /*PIT和开发PIC1和键盘中断(11111000) */
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */

	//// 初始化内存管理   ////
	memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000);
	
	// 初始化画面
	init_palette();
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);

	task_a = task_init(memman);
	fifo.task = task_a;
	task_run(task_a, 1, 0);
	*((int *) 0x0fe4) = (int) shtctl;

	// background
	sht_back = sheet_alloc(shtctl);
	buf_back = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1); //没有透明色
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);

	/* sht_cons */
	for (i = 0; i < 2; i++) {
		sht_cons[i] = sheet_alloc(shtctl);
		buf_cons[i] = (unsigned char *) memman_alloc_4k(memman, 256 * 165);
		sheet_setbuf(sht_cons[i], buf_cons[i], 256, 165, -1); /* 无透明色 */
		make_window8(buf_cons[i], 256, 165, "console", 0);
		make_textbox8(sht_cons[i], 8, 28, 240, 128, COL8_000000);

		task_cons[i] = task_alloc();
		task_cons[i]->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 12;
		task_cons[i]->tss.eip = (int) &console_task;
		task_cons[i]->tss.es = 1 * 8;
		task_cons[i]->tss.cs = 2 * 8;
		task_cons[i]->tss.ss = 1 * 8;
		task_cons[i]->tss.ds = 1 * 8;
		task_cons[i]->tss.fs = 1 * 8;
		task_cons[i]->tss.gs = 1 * 8;
		*((int *) (task_cons[i]->tss.esp + 4)) = (int) sht_cons[i];
		*((int *) (task_cons[i]->tss.esp + 8)) = (int) memtotal;
		task_run(task_cons[i], 2, 2); /* level=2, priority=2 */
		sht_cons[i]->task = task_cons[i];
		sht_cons[i]->flags |= 0x20; // 有光标
		cons_fifo[i] = (int*)memman_alloc_4k(memman, 128 * 4);
		fifo32_init(&task_cons[i]->fifo, 128, cons_fifo[i], task_cons[i]);
	}

	// sht_mouse
	sht_mouse = sheet_alloc(shtctl);
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); // 透明色号99
	init_mouse_cursor8(buf_mouse, COL8_008484);
	mx = (binfo->scrnx - 16) / 2; 
	my = (binfo->scrny - 28 - 16) / 2;

	// 调整图层
	sheet_slide(sht_back, 0, 0);
	sheet_slide(sht_cons[1], 56, 6);
	sheet_slide(sht_cons[0], 8, 2);
	sheet_slide(sht_mouse, mx, my);
	
	sheet_updown(sht_back, 0);
	sheet_updown(sht_cons[1], 1);
	sheet_updown(sht_cons[0], 2);
	sheet_updown(sht_mouse, 3);

	key_win = sht_cons[0];
	keywin_on(key_win);

	// 为了避免和键盘状态冲突，一开始进行设置
	fifo32_put(&keycmd, KEYCMD_LED);
	fifo32_put(&keycmd, key_leds);

	// sheet_refresh改为局部刷新时，需要多次调用
	for (;;) {
		if (fifo32_status(&keycmd) > 0 && keycmd_wait < 0) {
			// 向键盘控制器发送数据
			keycmd_wait = fifo32_get(&keycmd);
			wait_KBC_sendready();
			io_out8(PORT_KEYDAT, keycmd_wait);
		}

		io_cli();
		if (fifo32_status(&fifo) == 0) {
			// fifo为空时，存在搁置的绘图操作立即实行
			if (new_mx >= 0) {
				io_sti();
				sheet_slide(sht_mouse, new_mx, new_my);
				new_mx = -1;
			}
			else if (new_wx != 0x7fffffff) {
				io_sti();
				sheet_slide(sht, new_wx, new_wy);
			}
			else {
				task_sleep(task_a);
				io_sti();
			}
		} 
		else {
			i = fifo32_get(&fifo);
			io_sti();

			if (key_win->flags == 0) { // 输入窗口被关闭
				key_win = shtctl->sheets[shtctl->top - 1];
				keywin_on(key_win);
			}

			if (256 <= i && i <= 511) { // 键盘数据
				if (i < 0x80 + 256) { // 按键编码转换为字符编码
					if (key_shift == 0) {
						s[0] = keytable0[i - 256];
					}
					else {
						s[0] = keytable1[i - 256];
					}
				}
				else {
					s[0] = 0;
				}
				
				if ('A' <= s[0] && s[0] <= 'Z') { // 英文字母
					if (((key_leds & 4) == 0 && key_shift == 0) ||
						((key_leds & 4) != 0 && key_shift != 0)) {
						s[0] += 0x20; // 大写变成小写
					}
				}

				if (s[0] != 0) { // 一般字符，退格，回车
					fifo32_put(&key_win->task->fifo, s[0] + 256);
				}

				if (i == 256 + 0x0f) { // tab键
					keywin_off(key_win);
					j = key_win->height - 1;
					if (j == 0) {
						j = shtctl->top - 1;
					}
					key_win = shtctl->sheets[j];
					keywin_on(key_win);
				}

				if (i == 256 + 0x2a) {	/* 左shift ON */
					key_shift |= 1;
				}
				if (i == 256 + 0x36) {	/* 右shift ON */
					key_shift |= 2;
				}
				if (i == 256 + 0xaa) {	/* 左shift OFF */
					key_shift &= ~1;
				}
				if (i == 256 + 0xb6) {	/* 右shift OFF */
					key_shift &= ~2;
				}

				if (i == 256 + 0x3a) {	/* CapsLock */
					key_leds ^= 4;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0x45) {	/* NumLock */
					key_leds ^= 2;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0x46) {	/* ScrollLock */
					key_leds ^= 1;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				
				// shift != 0 模拟器当按下F1时key_shift会变为0
				if (i == 256 + 0x3b) {	/* Shift+F1 */
					task = key_win->task;
					if (task != 0 && task->tss.ss0 != 0) {
						cons_putstr0(task->cons, "\nBreak(key) :\n");
						io_cli();	/* 不能在改变寄存器值时切换到其他任务 */
						task->tss.eax = (int) &(task->tss.esp0);
						task->tss.eip = (int) asm_end_app;
						io_sti();
					}
				}

				if (i == 256 + 0xfa) {	/* 键盘成功接收到数据 */
					keycmd_wait = -1;
				}
				if (i == 256 + 0xfe) {	/* 键盘接收数据失败 */
					wait_KBC_sendready();
					io_out8(PORT_KEYDAT, keycmd_wait);
				}

				if (i == 256 + 0x57 && shtctl->top > 2) { // F11,切换窗口
					sheet_updown(shtctl->sheets[1], shtctl->top - 1);
				}
			} 
			else if (512 <= i && i <= 767) { // 鼠标数据
				if (mouse_decode(&mdec, i - 512) != 0) {
					mx += mdec.x;
					my += mdec.y;
					if (mx < 0) {
						mx = 0;
					}
					if (my < 0) {
						my = 0;
					}
					if (mx > binfo->scrnx - 1) {
						mx = binfo->scrnx - 1;
					}
					if (my > binfo->scrny - 1) {
						my = binfo->scrny - 1;
					}
					sheet_slide(sht_mouse, mx, my);
					new_mx = mx;
					new_my = my;
					if ((mdec.btn & 0x01) != 0) { // 按下左键
						//sheet_slide(sht_win, mx - 80, my - 80);
						if (mmx < 0) { // 处于通常模式
							// 按照从上到下的顺序寻找鼠标所指向的图层
							for (j = shtctl->top - 1; j > 0; j--) {
								sht = shtctl->sheets[j];
								x = mx - sht->vx0;
								y = my - sht->vy0;
								if (0 <= x && x < sht->bxsize && 0 <= y && y < sht->bysize) {
									if (sht->buf[y * sht->bxsize + x] != sht->col_inv) {
										sheet_updown(sht, shtctl->top - 1);
										if (sht != key_win) {
											keywin_off(key_win);
											key_win = sht;
											keywin_on(key_win);
										}

										// 鼠标位置在标题栏
										if (3 <= x && x < sht->bxsize - 3 && 3 <= y && y < 21) {
											mmx = mx;	/* 进入窗口移动模式 */
											mmy = my;
											mmx2 = sht->vx0;
											new_wy = sht->vy0;
										}
										
										// 鼠标在x按钮处
										if (sht->bxsize - 21 <= x && x < sht->bxsize - 5 && 5 <= y && y < 19) {
											/* 点击x按钮 */
											if ((sht->flags & 0x10) != 0) {	/* 该窗口是否是应用程序 */
												task = sht->task;
												cons_putstr0(task->cons, "\nBreak(mouse) :\n");
												io_cli();	/* 强制结束应用程序 */
												task->tss.eax = (int) &(task->tss.esp0);
												task->tss.eip = (int) asm_end_app;
												io_sti();
											}
										}
										break;
									}
								}
							}
						}
						else { // 如果处于窗口移动模式
							x = mx - mmx;
							y = my - mmy;
							new_wx = (mmx2 + x + 2) & ~3;
							new_wy = new_wy + y;
							//sheet_slide(sht, (mmx2 + x + 2) & ~3, sht->vy0 + y); // 调整窗口移动后x坐标为4的倍数
							//mmx = mx;
							mmy = my;
						}
					}
					else { // 没有按下左键
						mmx = -1;
						if (new_wx != 0x7fffffff) {
							sheet_slide(sht, new_wx, new_wy);
							new_wx = 0x7fffffff;
						}
					}
				}
			}
		}
	}
}
