#include "bootpack.h"
#include <string.h>

void cons_newline(struct CONSOLE *cons)
{
	int x, y;
	struct SHEET *sheet = cons->sht;
	struct TASK* task = task_now();
	if (cons->cur_y < 28 + 112) {
		cons->cur_y += 16; /* 换行 */
	} else {
		/* 滚动 */
		if (sheet != 0) {
			for (y = 28; y < 28 + 112; y++) {
				for (x = 8; x < 8 + 240; x++) {
					sheet->buf[x + y * sheet->bxsize] = sheet->buf[x + (y + 16) * sheet->bxsize];
				}
			}
			for (y = 28 + 112; y < 28 + 128; y++) {
				for (x = 8; x < 8 + 240; x++) {
					sheet->buf[x + y * sheet->bxsize] = COL8_000000;
				}
			}
			sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
		}
	}

	cons->cur_x = 8;
	if (task->langmode == 1 && task->langbyte1 != 0) {
		cons->cur_x += 8;
	}

	return;
}

void cons_putchar(struct CONSOLE *cons, int chr, char move)
{
	char s[2];
	s[0] = chr;
	s[1] = 0;
	if (s[0] == 0x09) {	/* 制表符 */
		for (;;) {
			if (cons->sht != 0) {
				putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
			}
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240) {
				cons_newline(cons);
			}
			if (((cons->cur_x - 8) & 0x1f) == 0) {
				break;	/* 被32整除 */
			}
		}
	} else if (s[0] == 0x0a) {	/* 换行 */
		cons_newline(cons);
	} else if (s[0] == 0x0d) {	/* 回车 */
		/* 不做任何操作 */
	} else {	/* 一般字符 */
		if (cons->sht != 0) {
			putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
		}

		if (move != 0) {
			/* move为0时光标不后移 */
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240) {
				cons_newline(cons);
			}
		}
	}
	return;
}

void cons_putstr0(struct CONSOLE *cons, char *s)
{
	for (; *s != 0; s++) {
		cons_putchar(cons, *s, 1);
	}
	return;
}

void cons_putstr1(struct CONSOLE *cons, char *s, int l)
{
	int i;
	for (i = 0; i < l; i++) {
		cons_putchar(cons, s[i], 1);
	}
	return;
}

void cmd_mem(struct CONSOLE *cons, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	char s[30];
	sprintf(s, "total   %dMB\nfree  %dKB\n\n", memtotal / (1024 * 1024), memman_total(memman) / 1024);
	cons_putstr0(cons, s);
	return;
}

void cmd_cls(struct CONSOLE *cons)
{
	int x, y;
	struct SHEET *sheet = cons->sht;
	for (y = 28; y < 28 + 128; y++) {
		for (x = 8; x < 8 + 240; x++) {
			sheet->buf[x + y * sheet->bxsize] = COL8_000000;
		}
	}
	sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
	cons->cur_y = 28;
	return;
}

void cmd_ls(struct CONSOLE *cons)
{
	struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	int i, j;
	char s[30];
	for (i = 0; i < 224; i++) {
		if (finfo[i].name[0] == 0x00) {
			break;
		}
		if (finfo[i].name[0] != 0xe5) {
			if ((finfo[i].type & 0x18) == 0) {
				sprintf(s, "filename.ext   %7d\n", finfo[i].size);
				for (j = 0; j < 8; j++) {
					s[j] = finfo[i].name[j];
				}
				s[ 9] = finfo[i].ext[0];
				s[10] = finfo[i].ext[1];
				s[11] = finfo[i].ext[2];
				cons_putstr0(cons, s);
			}
		}
	}
	cons_newline(cons);
	return;
}

int cmd_app(struct CONSOLE *cons, int *fat, char* cmdline)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct FILEINFO *finfo;
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
	char name[18], *p, *q;
	int i;
	struct TASK *task = task_now();
	int segsiz, datsiz, esp, dathrb, appsiz;
	struct SHTCTL* shtctl;
	struct SHEET*  sht;

	/* 根据命令行生成名字 */
	for (i = 0; i < 13; i++) {
		if (cmdline[i] <= ' ') {
			break;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0; /* 文件名后边置0 */

	finfo = file_search(name, (struct FILEINFO*)(ADR_DISKIMG + 0x002600), 224);
	if (finfo == 0 && name[i - 1] != '.') {  //找不到文件时，文件名后边加.hrb后继续寻找
		name[i] = '.';
		name[i + 1] = 'H';
		name[i + 2] = 'R';
		name[i + 3] = 'B';
		name[i + 4] = 0;
		finfo = file_search(name, (struct FILEINFO*)(ADR_DISKIMG + 0x002600), 224);
	}

	if (finfo != 0) {
		/* 找到文件信息 */
		appsiz = finfo->size;
		p = file_loadfile2(finfo->clustno, &appsiz, fat);
		// 0x0004中存放的是“Hari”这4个字节
		if (appsiz >= 36 && strncmp(p + 4, "Hari", 4) == 0 && *p == 0x00) {
			segsiz = *((int *) (p + 0x0000)); // 存放的是数据段的大小
			esp    = *((int *) (p + 0x000c)); // 存放的是应用程序启动时ESP寄存器的初始值
			datsiz = *((int *) (p + 0x0010)); // 存放的是需要向数据段传送的部分的字节数
			dathrb = *((int *) (p + 0x0014)); // 存放的是需要向数据段传送的部分在hrb文件中的起始地址
			q = (char *) memman_alloc_4k(memman, segsiz); // 应用程序专用的内存空间
			task->ds_base = (int) q;   // 修改前：*((int *) 0xfe8) = (int) q;

			// AR_CODE32_ER + 0x60 表示应用程序运行时,如果段寄存器存放操作系统段会发生异常
			// sel为段号*8
			set_segmdesc(task->ldt + 0, finfo->size - 1, (int) p, AR_CODE32_ER + 0x60);
			set_segmdesc(task->ldt + 1, segsiz - 1,      (int) q, AR_DATA32_RW + 0x60);
			
			// 从hrb文件传送数据到数据段，
			// ESP之前地址是栈空间, ESP地址及之后被用于存放字符串等数据
			for (i = 0; i < datsiz; i++) {
				q[esp + i] = p[dathrb + i];
			}
			// + 4表示LDT内的段号
			start_app(0x1b, 0 * 8 + 4, esp, 1 * 8 + 4, &(task->tss.esp0));
			for (i = 0; i < 8; i++) {
				if (task->fhandle[i].buf != 0) {
					memman_free_4k(memman, (int)task->fhandle[i].buf, task->fhandle[i].size);
					task->fhandle[i].buf = 0;
				}
			}

			shtctl = (struct SHTCTL*) *((int *)0x0fe4);
			for (i = 0; i < MAX_SHEETS; i++) {
				sht = &(shtctl->sheets0[i]);
				if ((sht->flags & 0x11) == 0x11 && sht->task == task) {
					sheet_free(sht);
				}
			}
			timer_cancelall(&task->fifo);

			memman_free_4k(memman, (int) q, segsiz);
			task->langbyte1 = 0;
		} else {
			cons_putstr0(cons, ".hrb file format error.\n");
		}
		memman_free_4k(memman, (int) p, finfo->size);
		cons_newline(cons);
		return 1;
	}

	// 没有找到
	return 0;
}

void cmd_exit(struct CONSOLE *cons, int *fat)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = task_now();
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct FIFO32 *fifo = (struct FIFO32 *) *((int *) 0x0fec);
	if (cons->sht != 0) {
		timer_cancel(cons->timer);
	}
	memman_free_4k(memman, (int) fat, 4 * 2880);
	io_cli();

	if (cons->sht != 0) {
		fifo32_put(fifo, cons->sht - shtctl->sheets0 + 768);	/* 768 ~ 1023 */
	}
	else {
		fifo32_put(fifo, task - taskctl->tasks0 + 1024);	/* 768 ~ 1023 */
	}
	
	io_sti();
	for (;;) {
		task_sleep(task);
	}
}

void cmd_start(struct CONSOLE *cons, char *cmdline, int memtotal)
{
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht = open_console(shtctl, memtotal);
	struct FIFO32 *fifo = &sht->task->fifo;
	int i;
	sheet_slide(sht, 32, 4);
	sheet_updown(sht, shtctl->top);
	/* 将命令行输入的字符逐字复制到新的命令行窗口 */
	for (i = 6; cmdline[i] != 0; i++) {
		fifo32_put(fifo, cmdline[i] + 256);
	}
	fifo32_put(fifo, 10 + 256);	/* Enter */
	cons_newline(cons);
	return;
}

void cmd_ncst(struct CONSOLE *cons, char *cmdline, int memtotal)
{
	struct TASK *task = open_constask(0, memtotal);
	struct FIFO32 *fifo = &task->fifo;
	int i;
	/* 将命令行输入的字符逐字复制到新的命令行窗口 */
	for (i = 5; cmdline[i] != 0; i++) {
		fifo32_put(fifo, cmdline[i] + 256);
	}
	fifo32_put(fifo, 10 + 256);	/* Enter */
	cons_newline(cons);
	return;
}

void cmd_langmode(struct CONSOLE *cons, char *cmdline)
{
	struct TASK *task = task_now();
	unsigned char mode = cmdline[9] - '0';
	if (mode <= 2) {
		task->langmode = mode;
	} else {
		cons_putstr0(cons, "mode number error.\n");
	}
	cons_newline(cons);
	return;
}

void cons_runcmd(char *cmdline, struct CONSOLE *cons, int *fat, unsigned int memtotal)
{
	if (strcmp(cmdline, "mem") == 0 && cons->sht != 0) {
		cmd_mem(cons, memtotal);
	} 
	else if (strcmp(cmdline, "cls") == 0 && cons->sht != 0) {
		cmd_cls(cons);
	} 
	else if (strcmp(cmdline, "ls") == 0 && cons->sht != 0) {
		cmd_ls(cons);
	}
	else if (strcmp(cmdline, "exit") == 0) {
		cmd_exit(cons, fat);
	}
	else if (strncmp(cmdline, "start ", 6) == 0) {
		cmd_start(cons, cmdline, memtotal);
	}
	else if(strncmp(cmdline, "ncst ", 5) == 0) {
		cmd_ncst(cons, cmdline, memtotal);
	}
	else if (strncmp(cmdline, "langmode ", 9) == 0) {
		cmd_langmode(cons, cmdline);
	}
	else if (cmdline[0] != 0) {
		if (cmd_app(cons, fat, cmdline) == 0) {
			/* 不是命令也不是空行 */
			cons_putstr0(cons, "Bad command.\n\n");
		}
	}
	return;
}

// task_cons的入口
// 不能return，同样HariMain也不可retun
void console_task(struct SHEET *sheet, unsigned int memtotal)
{
	struct TASK *task = task_now();
	int i;
	char cmdline[30];
	struct CONSOLE cons;
	struct FILEHANDLE fhandle[8];
	struct MEMMAN* memman = (struct MEMMAN*) MEMMAN_ADDR;
	unsigned char *nihongo = (char *) *((int *) 0x0fe8);

	for (i = 0; i < 8; i++) {
		fhandle[i].buf = 0;
	}
	task->fhandle = fhandle;

	// 分配内存并读取fat到内存中
	// fat(file alloction table<内存分配表>)存放各个文件所在的扇区，
	// fat 位于磁盘的0x000200~0x0013ff及0x001400~0x0025ff,第二个为第一个备份
	int* fat = (int *)memman_alloc_4k(memman, 4 * 2880);
	file_readfat(fat, (unsigned char*)(ADR_DISKIMG + 0x000200)); 
	task->fat = fat;
	if (nihongo[4096] != 0xff) { /* 是否载入日文字库 */
		task->langmode = 1;
	} else {
		task->langmode = 0;
	}
	task->langbyte1 = 0;

	cons.cur_x = 8;
	cons.cur_y = 28;
	cons.cur_c = -1;
	cons.sht = sheet;
	task->cons = &cons; // 修改前：*((int*)0xfec) = (int)&cons;
	task->cmdline = cmdline;

	if (cons.sht != 0) {
		cons.timer = timer_alloc();
		timer_init(cons.timer, &task->fifo, 1);
		timer_settime(cons.timer, 50);
	}

	/* 显示提示符 */
	putfonts8_asc_sht(cons.sht, 8, 28, COL8_FFFFFF, COL8_000000, ">", 1);
	cons_putchar(&cons, '>', 1);

	for (;;) {
		io_cli();
		if (fifo32_status(&task->fifo) == 0) {
			task_sleep(task);
			io_sti();
		} else {
			i = fifo32_get(&task->fifo);
			io_sti();
			if (i <= 1 && cons.sht != 0) { /* 光标定时器 */
				if (i != 0) {
					timer_init(cons.timer, &task->fifo, 0); /* 下次置0 */
					if (cons.cur_c >= 0) {
						cons.cur_c = COL8_FFFFFF;
					}
				} else {
					timer_init(cons.timer, &task->fifo, 1); /* 下次置1 */
					if (cons.cur_c >= 0) {
						cons.cur_c = COL8_000000;
					}
				}
				timer_settime(cons.timer, 50);
			}
			if (i == 2) { // 光标ON
				cons.cur_c = COL8_FFFFFF;
			}
			if (i == 3) { // 光标OFF
				if (cons.sht != 0) {
					boxfill8(cons.sht->buf, cons.sht->bxsize, COL8_000000, cons.cur_x, cons.cur_y, cons.cur_x + 7, 43);
				}
				cons.cur_c = -1;
			}
			if (i == 4) { // exit 命令行窗口
				cmd_exit(&cons, fat);
			}

			if (256 <= i && i <= 511) { // 键盘数据（通过任务A）
				if (i == 8 + 256) { /* 退格键 */
					if (cons.cur_x > 16) {
						/* 用空白擦除，光标前移 */
						cons_putchar(&cons, ' ', 0);
						cons.cur_x -= 8;
					}
				} 
				else if (i == 10 + 256) { // 回车键
					// 用空格擦除原来的光标
					cons_putchar(&cons, ' ', 0);
					cmdline[cons.cur_x / 8 - 2] = 0;
					cons_newline(&cons);
					cons_runcmd(cmdline, &cons, fat, memtotal); // 执行命令

					if (cons.sht == 0) {
						cmd_exit(&cons, fat);
					}

					cons_putchar(&cons, '>', 1);
				}
				else { /* 一般字符 */
					if (cons.cur_x < 240) {
						/* 显示字符，光标后移 */
						cmdline[cons.cur_x / 8 - 2] = i - 256;
						cons_putchar(&cons, i - 256, 1);
					}
				}
			}

			/* 刷新光标 */
			if (cons.sht != 0) {
				if (cons.cur_c >= 0) {
					boxfill8(cons.sht->buf, cons.sht->bxsize, cons.cur_c, cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
				}
				sheet_refresh(cons.sht, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y + 16);
			}
		}
	}
}

void hrb_api_linewin(struct SHEET *sht, int x0, int y0, int x1, int y1, int col)
{
	int i, x, y, len, dx, dy;

	dx = x1 - x0;
	dy = y1 - y0;
	x = x0 << 10;
	y = y0 << 10;
	if (dx < 0) {
		dx = - dx;
	}
	if (dy < 0) {
		dy = - dy;
	}
	if (dx >= dy) {
		len = dx + 1;
		if (x0 > x1) {
			dx = -1024;
		} else {
			dx =  1024;
		}
		if (y0 <= y1) {
			dy = ((y1 - y0 + 1) << 10) / len;
		} else {
			dy = ((y1 - y0 - 1) << 10) / len;
		}
	}
	else {
		len = dy + 1;
		if (y0 > y1) {
			dy = -1024;
		} else {
			dy =  1024;
		}
		if (x0 <= x1) {
			dx = ((x1 - x0 + 1) << 10) / len;
		} else {
			dx = ((x1 - x0 - 1) << 10) / len;
		}
	}

	for (i = 0; i < len; i++) {
		sht->buf[(y >> 10) * sht->bxsize + (x >> 10)] = col;
		x += dx;
		y += dy;
	}

	return;
}

// EDX存放功能号
int* hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
	struct TASK* task = task_now();
	int ds_base = task->ds_base;
	struct CONSOLE *cons = task->cons;

	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct FIFO32 *sys_fifo = (struct FIFO32 *)* ((int *) 0x0fec);
	struct SHEET *sht;
	int *reg = &eax + 1;	/* eax后边的地址 */
		/* 强行改写通过PUSHAD保存的值, 作为返回值 */
		/* reg[0] : EDI,   reg[1] : ESI,   reg[2] : EBP,   reg[3] : ESP */
		/* reg[4] : EBX,   reg[5] : EDX,   reg[6] : ECX,   reg[7] : EAX */
	int i;
	struct FILEINFO *finfo;
	struct FILEHANDLE *fh;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;

	if (edx == 1) {
		cons_putchar(cons, eax & 0xff, 1); // AL = 字符编码
	}
	 else if (edx == 2) {
		cons_putstr0(cons, (char *) ebx + ds_base);  // EBX = 字符串地址
	} 
	else if (edx == 3) {
		cons_putstr1(cons, (char *) ebx + ds_base, ecx); // EBX = 字符串地址，ECX = 字符串长度
	}
	else if (edx == 5) { // 显示窗口
		// EBX = 窗口缓冲区 ESI = 窗口在x轴方向上的大小（即窗口宽度）
		// EDI = 窗口在y轴方向上的大小（即窗口高度）EAX = 透明色 
		// ECX = 窗口名称
		// 返回值: EAX =用于操作窗口的句柄（用于刷新窗口等操作）
		sht = sheet_alloc(shtctl);
		sht->task = task;
		sht->flags |= 0x10;
		sheet_setbuf(sht, (char *) ebx + ds_base, esi, edi, eax);
		make_window8((char *) ebx + ds_base, esi, edi, (char *) ecx + ds_base, 0);
		sheet_slide(sht, ((shtctl->xsize - esi) / 2) & ~3, (shtctl->ysize - edi) / 2); // x坐标调整为4的倍数
		sheet_updown(sht, shtctl->top);	/* 窗口高度指定为当前鼠标层，鼠标上移一层 */
		reg[7] = (int) sht;
	}
	else if (edx == 6) { // 窗口上显示字符
		// EBX = 窗口句柄 ESI = 显示位置的x坐标 EDI = 显示位置的y坐标 
		// EAX = 色号 ECX = 字符串长度EBP = 字符串
		// 窗口句柄的值为偶数，使之加1表明该窗口不自动刷新
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		putfonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *) ebp + ds_base);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
		}
	}
	else if (edx == 7) { // 窗口上描绘方块
		// EBX = 窗口句柄 EAX = x0 ECX = y0 ESI = x1 EDI = y1 EBP = 色号
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		boxfill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	}
	else if (edx == 4) {
		return &(task->tss.esp0);
	}
	else if (edx == 8) { // memman初始化
		// EBX=memman的地址 EAX=memman所管理的内存空间的起始地址
		// ECX=memman所管理的内存空间的字节数
		memman_init((struct MEMMAN *) (ebx + ds_base));
		ecx &= 0xfffffff0;	/* 以16字节为单位 */
		memman_free((struct MEMMAN *) (ebx + ds_base), eax, ecx);
	} 
	else if (edx == 9) { // malloc
	    // EBX=memman的地址  ECX=需要请求的字节数  EAX=分配到的内存空间地址
		ecx = (ecx + 0x0f) & 0xfffffff0; /* 16字节为单位 */
		reg[7] = memman_alloc((struct MEMMAN *) (ebx + ds_base), ecx);
	} 
	else if (edx == 10) { // free
		// EBX=memman的地址 EAX=需要释放的内存空间地址 ECX=需要释放的字节数
		ecx = (ecx + 0x0f) & 0xfffffff0; /* 16字节为单位 */
		memman_free((struct MEMMAN *) (ebx + ds_base), eax, ecx);
	}
	else if (edx == 11) { // 在窗口中画点
		//EBX =窗口句柄 ESI =显示位置的x坐标 EDI =显示位置的y坐标 EAX =色号
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		sht->buf[sht->bxsize * edi + esi] = eax;
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, esi, edi, esi + 1, edi + 1);
		}
	}
	else if (edx == 12) { // 刷新窗口
		// EBX = 窗口句柄 EAX = x0 ECX = y0 ESI = x1 EDI = y1
		sht = (struct SHEET *) ebx;
		sheet_refresh(sht, eax, ecx, esi, edi);
	}
	else if (edx == 13) { // 在窗口上画直线
		// EBX = 窗口句柄 EAX = x0 ECX = y0 ESI = x1 EDI = y1 EBP = 色号
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		hrb_api_linewin(sht, eax, ecx, esi, edi, ebp);
		if ((ebx & 1) == 0) {
			if (eax > esi) {
				i = eax;
				eax = esi;
				esi = i;
			}
			if (ecx > edi) {
				i = ecx;
				ecx = edi;
				edi = i;
			}

			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	}
	else if (edx == 14) { // 关闭窗口
		//EBX=窗口句柄
		sheet_free((struct SHEET*) ebx);
	}
	else if (edx == 15) { // 键盘输入
		// EAX = 0……没有键盘输入时返回-1，不休眠
		//     = 1……休眠直到发生键盘输入
		// EAX = 输入的字符编码

		for (;;) {
			io_cli();
			if (fifo32_status(&task->fifo) == 0) {
				if (eax != 0) {
					task_sleep(task);	/* FIFO为空，休眠并等待 */
				} else {
					io_sti();
					reg[7] = -1;
					return 0;
				}
			}

			i = fifo32_get(&task->fifo);
			io_sti();
			if (i <= 1) { /* 光标用的定时器 */
				/* 当应用程序运行时不需要显示光标，因此总是将下次显示用的值置1 */
				timer_init(cons->timer, &task->fifo, 1); /* 下次置为1 */
				timer_settime(cons->timer, 50);
			}
			if (i == 2) {	/* 光标ON */
				cons->cur_c = COL8_FFFFFF;
			}
			if (i == 3) {	/* 光标OFF */
				cons->cur_c = -1;
			}
			if (i == 4) { // 只关闭命令行窗口
				timer_cancel(cons->timer);
				io_cli();
				fifo32_put(sys_fifo, cons->sht - shtctl->sheets0 + 2024); // 2024 ~ 2279
				cons->sht = 0;
				io_sti();
			}
			if (256 <= i) { /* 键盘数据（通过任务A） */
				reg[7] = i - 256;
				return 0;
			}
		}
	}
	else if (edx == 16) {  // 获取定时器（alloc）
		// EAX=定时器句柄（由操作系统返回）
		reg[7] = (int) timer_alloc();
		((struct TIMER*) reg[7])->flags2 = 1; // 程序结束自动取消定时器
	}
	else if (edx == 17) { // 设置定时器的发送数据（init）
		// EBX=定时器句柄 EAX=数据
		timer_init((struct TIMER *)ebx, &task->fifo, eax + 256);
	} 
	else if (edx == 18) { // 定时器时间设定（set）
		// EBX=定时器句柄 EAX=时间
		timer_settime((struct TIMER *) ebx, eax);
	}
	else if (edx == 19) { // 释放定时器（free）
		// EBX=定时器句柄
		timer_free((struct TIMER *) ebx);
	}
	else if (edx == 20) { // 蜂鸣器发声
		// EAX=声音频率（单位是mHz，即毫赫兹）例如当EAX=4400000时，则发出440Hz的声音频率
		// 设为0则表示停止发声
		if (eax == 0) { // // 蜂鸣器ON
			i = io_in8(0x61);
			io_out8(0x61, i & 0x0d);
		} else {
			i = 1193180000 / eax; // 声音频率为时钟处于设定的值
			
			// 设置声音频率
			io_out8(0x43, 0xb6);
			io_out8(0x42, i & 0xff);
			io_out8(0x42, i >> 8);

			// 蜂鸣器ON
			i = io_in8(0x61);
			io_out8(0x61, (i | 0x03) & 0x0f);
		}
	}
	else if (edx == 21) { // 打开文件 
		// EBX=文件名 EAX=文件句柄（为0时表示打开失败）（由操作系统返回）
		for (i = 0; i < 8; i++) {
			if (task->fhandle[i].buf == 0) {
				break;
			}
		}
		fh = &task->fhandle[i];
		reg[7] = 0;
		if (i < 8) {
			finfo = file_search((char *) ebx + ds_base,
					(struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
			if (finfo != 0) {
				reg[7] = (int) fh;
				fh->size = finfo->size;
				fh->pos = 0;
				fh->buf = file_loadfile2(finfo->clustno, &fh->size, task->fat);
			}
		}
	}
	else if (edx == 22) { // 关闭文件
		// EAX=文件句柄
		fh = (struct FILEHANDLE *) eax;
		memman_free_4k(memman, (int) fh->buf, fh->size);
		fh->buf = 0;
	}
	else if (edx == 23) { // 文件定位
		// EAX=文件句柄 
		// ECX=定位模式
		//    =0：定位的起点为文件开头
		//    =1：定位的起点为当前的访问位置
		//    =2：定位的起点为文件末尾
		// EBX=定位偏移量
		fh = (struct FILEHANDLE *) eax;
		if (ecx == 0) {
			fh->pos = ebx;
		} else if (ecx == 1) {
			fh->pos += ebx;
		} else if (ecx == 2) {
			fh->pos = fh->size + ebx;
		}
		if (fh->pos < 0) {
			fh->pos = 0;
		}
		if (fh->pos > fh->size) {
			fh->pos = fh->size;
		}
	}
	else if (edx == 24) { // 获取文件大小
		// EAX=文件句柄
		// ECX=文件大小获取模式
		// 	  =0：普通文件大小
		//    =1：当前读取位置从文件开头起算的偏移量
		//    =2：当前读取位置从文件末尾起算的偏移量
		// EAX=文件大小（由操作系统返回）
		fh = (struct FILEHANDLE *) eax;
		if (ecx == 0) {
			reg[7] = fh->size;
		} else if (ecx == 1) {
			reg[7] = fh->pos;
		} else if (ecx == 2) {
			reg[7] = fh->pos - fh->size;
		}
	} else if (edx == 25) { // 文件读取
		// EAX=文件句柄 EBX=缓冲区地址 ECX=最大读取字节数
		// EAX=本次读取到的字节数（由操作系统返回）
		fh = (struct FILEHANDLE *) eax;
		for (i = 0; i < ecx; i++) {
			if (fh->pos == fh->size) {
				break;
			}
			*((char *) ebx + ds_base + i) = fh->buf[fh->pos];
			fh->pos++;
		}
		reg[7] = i;
	}
	else if (edx == 26) { // 获取命令行内容
		// EBX=存放命令行内容的地址 ECX=最多可存放多少字节 
		// EAX=实际存放了多少字节（由操作系统返回）

		i = 0;
		for (;;) {
			*((char *) ebx + ds_base + i) =  task->cmdline[i];
			if (task->cmdline[i] == 0) {
				break;
			}
			if (i >= ecx) {
				break;
			}
			i++;
		}
		reg[7] = i;

	}
	else if (edx == 27) { // 获取langmode
		reg[7] = task->langmode;
	}

	return 0;
}

int* inthandler0c(int *esp)
{
	struct TASK* task = task_now();
	struct CONSOLE *cons = task->cons;
	
	char s[30];
	cons_putstr0(cons, "\nINT 0C :\n Stack Exception.\n");

	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);

	return &(task->tss.esp0);; /* 强制结束程序 */
}

int* inthandler0d(int *esp)
{
	struct TASK* task = task_now();
	struct CONSOLE *cons = task->cons;
	
	char s[30];
	cons_putstr0(cons, "\nINT 0D :\n General Protected Exception.\n");

	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);

	return &(task->tss.esp0);; /* 强制结束程序 */
}