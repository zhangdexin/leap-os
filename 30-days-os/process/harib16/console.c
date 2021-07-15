#include "bootpack.h"
#include <string.h>

int cons_newline(int cursor_y, struct SHEET *sheet)
{
	int x, y;
	if (cursor_y < 28 + 112) {
		cursor_y += 16; /* 换行 */
	} else {
		/* 滚动 */
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

	return cursor_y;
}

// task_cons的入口
// 不能return，同样HariMain也不可retun
void console_task(struct SHEET *sheet, unsigned int memtotal)
{
	struct TIMER *timer;
	struct TASK *task = task_now();
	char s[30], cmdline[30], *p;
	int i, fifobuf[128], cursor_x = 16, cursor_y = 28, cursor_c = -1;
	int x, y;

	struct MEMMAN* memman = (struct MEMMAN*) MEMMAN_ADDR;
	struct FILEINFO* finfo = (struct FILEINFO*)(ADR_DISKIMG + 0x002600);
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR*)ADR_GDT;

	// 分配内存并读取fat到内存中
	// fat(file alloction table<内存分配表>)存放各个文件所在的扇区，
	// fat 位于磁盘的0x000200~0x0013ff及0x001400~0x0025ff,第二个为第一个备份
	int* fat = (int *)memman_alloc_4k(memman, 4 * 2880);
	file_readfat(fat, (unsigned char*)(ADR_DISKIMG + 0x000200)); 

	fifo32_init(&task->fifo, 128, fifobuf, task);

	timer = timer_alloc();
	timer_init(timer, &task->fifo, 1);
	timer_settime(timer, 50);

	/* 显示提示符 */
	putfonts8_asc_sht(sheet, 8, 28, COL8_FFFFFF, COL8_000000, ">", 1);

	for (;;) {
		io_cli();
		if (fifo32_status(&task->fifo) == 0) {
			task_sleep(task);
			io_sti();
		} else {
			i = fifo32_get(&task->fifo);
			io_sti();
			if (i <= 1) { /* 光标定时器 */
				if (i != 0) {
					timer_init(timer, &task->fifo, 0); /* 下次置0 */
					if (cursor_c >= 0) {
						cursor_c = COL8_FFFFFF;
					}
				} else {
					timer_init(timer, &task->fifo, 1); /* 下次置1 */
					if (cursor_c >= 0) {
						cursor_c = COL8_000000;
					}
				}
				timer_settime(timer, 50);
			}
			if (i == 2) { // 光标ON
				cursor_c = COL8_FFFFFF;
			}
			if (i == 3) { // 光标OFF
				boxfill8(sheet->buf, sheet->bxsize, COL8_000000, cursor_x, 28, cursor_x + 7, 43);
				cursor_c = -1;
			}

			if (256 <= i && i <= 511) { // 键盘数据（通过任务A）
				if (i == 8 + 256) { /* 退格键 */
					if (cursor_x > 16) {
						/* 用空白擦除，光标前移 */
						putfonts8_asc_sht(sheet, cursor_x, 28, COL8_FFFFFF, COL8_000000, " ", 1);
						cursor_x -= 8;
					}
				} 
				else if (i == 10 + 256) { // 回车键
					// 用空格擦除原来的光标
					putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
					cmdline[cursor_x / 8 - 2] = 0;
					cursor_y = cons_newline(cursor_y, sheet);

					/* 执行命令 */
					if (strcmp(cmdline, "mem") == 0) {
						/* mem命令 */
						sprintf(s, "total   %dMB", memtotal / (1024 * 1024));
						putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
						cursor_y = cons_newline(cursor_y, sheet);
						sprintf(s, "free %dKB", memman_total(memman) / 1024);
						putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
						cursor_y = cons_newline(cursor_y, sheet);
						cursor_y = cons_newline(cursor_y, sheet);
					}
					else if (strcmp(cmdline, "cls") == 0) {
						// cls
						for (y = 28; y < 28 + 128; y++) {
							for (x = 8; x < 8 + 240; x++) {
								sheet->buf[x + y * sheet->bxsize] = COL8_000000;
							}
						}
						sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
						cursor_y = 28;
					}
					else if (strcmp(cmdline, "ls") == 0) { // ls命令
						for (x = 0; x < 224; x++) {
							if (finfo[x].name[0] == 0x00) {
								break;
							}
							if (finfo[x].name[0] != 0xe5) {
								if ((finfo[x].type & 0x18) == 0) {
									sprintf(s, "filename.ext   %7d", finfo[x].size);
									for (y = 0; y < 8; y++) {
										s[y] = finfo[x].name[y];
									}
									s[ 9] = finfo[x].ext[0];
									s[10] = finfo[x].ext[1];
									s[11] = finfo[x].ext[2];
									putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
									cursor_y = cons_newline(cursor_y, sheet);
								}
							}
						}
						cursor_y = cons_newline(cursor_y, sheet);
					}
					else if (strncmp(cmdline, "type ", 5) == 0) { // type命令
						// 获得文件名
						for (y = 0; y < 11; y++) {
							s[y] = ' ';
						}
						y = 0;
						for (x = 5; y < 11 && cmdline[x] != 0; x++) {
							if (cmdline[x] == '.' && y <= 8) {
								y = 8;
							} 
							else {
								s[y] = cmdline[x];
								if ('a' <= s[y] && s[y] <= 'z') {
									/* 小写字母转大写 */
									s[y] -= 0x20;
								} 
								y++;
							}
						}

						/* 寻找文件 */
						for (x = 0; x < 224; ) {
							if (finfo[x].name[0] == 0x00) {
								break;
							}
							if ((finfo[x].type & 0x18) == 0) {
								for (y = 0; y < 11; y++) {
									if (finfo[x].name[y] != s[y]) {
										goto type_next_file;
									}
								}
								break; /* 找到 */
							}
		type_next_file:
							x++;
						}

						if (x < 224 && finfo[x].name[0] != 0x00) {
							p = (char*)memman_alloc_4k(memman, finfo[x].size);
							file_loadfile(finfo[x].clustno, finfo[x].size, p, fat, (char*)(ADR_DISKIMG + 0x003e00));
							cursor_x = 8;
							for (y = 0; y < finfo[x].size; y++) {
								/* 逐字输出 */
								s[0] = p[y];
								s[1] = 0;
								if (s[0] == 0x09) { // 制表符
									for (;;) {
										putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
										cursor_x += 8;
										if (cursor_x == 8 + 240) {
											cursor_x = 8;
											cursor_y = cons_newline(cursor_y, sheet);
										}
										if (((cursor_x - 8) & 0x1f) == 0) {
											break;	/* 被32整除则break */
										}
									}
								}
								else if (s[0] == 0x0a) { // 换行
									cursor_x = 8;
									cursor_y = cons_newline(cursor_y, sheet);
								}
								else if (s[0] == 0x0d) { // 回车
									// 暂不处理
								}
								else { // 一般字符
									putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
									cursor_x += 8;
									if (cursor_x == 8 + 240) {
										cursor_x = 8;
										cursor_y = cons_newline(cursor_y, sheet);
									}
								}
							}
							memman_free_4k(memman, (int)p, finfo[x].size);
						} 
						else {
							/* 没有找到文件信息 */
							putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
							cursor_y = cons_newline(cursor_y, sheet);
						}
						cursor_y = cons_newline(cursor_y, sheet);
					}
					else if (strcmp(cmdline, "hlt") == 0) {
						// 启动hlt.hrb
						for (y = 0; y < 11; y++) {
							s[y] = ' ';
						}

						s[0] = 'H';
						s[1] = 'L';
						s[2] = 'T';
						s[8] = 'H';
						s[9] = 'R';
						s[10] = 'B';
						for (x = 0; x < 224; ) {
							if (finfo[x].name[0] == 0x00) {
								break;
							}
							if ((finfo[x].type & 0x18) == 0) {
								for (y = 0; y < 11; y++) {
									if (finfo[x].name[y] != s[y]) {
										goto hlt_next_file;
									}
								}
								break; /* 找到文件 */
							}
		hlt_next_file:
							x++;
						}

						if (x < 224 && finfo[x].name[0] != 0x00) {
							/* 找打文件信息 */
							// 将文件内容存到p地址，设置1003号段的地址为p, 跳转到1003号段开始执行
							p = (char *) memman_alloc_4k(memman, finfo[x].size);
							file_loadfile(finfo[x].clustno, finfo[x].size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
							set_segmdesc(gdt + 1003, finfo[x].size - 1, (int) p, AR_CODE32_ER);
							farjmp(0, 1003 * 8);
							memman_free_4k(memman, (int) p, finfo[x].size);
						} else {
							/* 没有找到文件信息 */
							putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
							cursor_y = cons_newline(cursor_y, sheet);
						}
						cursor_y = cons_newline(cursor_y, sheet);
					}
					else if (cmdline[0] != 0) {
						/* 不是命令也不是空行 */
						putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "Bad command.", 12);
						cursor_y = cons_newline(cursor_y, sheet);
						cursor_y = cons_newline(cursor_y, sheet);
					}
					/* 显示提示符 */
					putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, ">", 1);
					cursor_x = 16;
				}
				else { /* 一般字符 */
					if (cursor_x < 240) {
						/* 显示字符，光标后移 */
						s[0] = i - 256;
						s[1] = 0;
						cmdline[cursor_x / 8 - 2] = i - 256;
						putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
						cursor_x += 8;
					}
				}
			}

			/* 刷新光标 */
			if (cursor_c >= 0) {
				boxfill8(sheet->buf, sheet->bxsize, cursor_c, cursor_x, cursor_y, cursor_x + 7, cursor_y + 15);
			}
			sheet_refresh(sheet, cursor_x, cursor_y, cursor_x + 8, cursor_y + 16);
		}
	}
}