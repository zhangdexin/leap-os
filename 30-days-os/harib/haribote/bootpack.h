// asmhead.nas
struct BOOTINFO { /* 0x0ff0-0x0fff */
	char cyls;    /* 柱面数 */
	char leds;    /* 启动时键盘led的状态 */
	char vmode;   /* 显卡模式为多少位彩色 */
	char reserve;
	short scrnx, scrny; /* 画面分辨率 */
	char *vram;
};
#define ADR_BOOTINFO 0x00000ff0
#define ADR_DISKIMG  0x00100000

/* naskfunc.nas */
void io_hlt(void);
void io_cli(void);
void io_sti(void);
void io_stihlt(void);
int io_in8(int port);
void io_out8(int port, int data);
int io_load_eflags(void);
void io_store_eflags(int eflags);
void load_gdtr(int limit, int addr);
void load_idtr(int limit, int addr);
int load_cr0(void);
void store_cr0(int cr0);
void load_tr(int tr);// tr: 段号*8
void asm_inthandler20(void);
void asm_inthandler21(void);
void asm_inthandler27(void);
void asm_inthandler2c(void);
void asm_inthandler0d(void);
void asm_inthandler0c(void);
unsigned int memtest_sub(unsigned int start, unsigned int end);
//void taskswitch4(void);
void farjmp(int eip, int cs);
void farcall(int eip, int cs);
void asm_hrb_api();
void asm_end_app();
void start_app(int eip, int cs, int esp, int ds, int *tss_esp0);

/* memory.c 内存管理 */
#define MEMMAN_FREES 4090	/* free[]大约32KB(8 * 4090) */
#define MEMMAN_ADDR	 0x003c0000 // 内存信息表存放在0x003c0000

// 可用信息
struct FREEINFO { 
	unsigned int addr, size;
};

// 内存管理器
struct MEMMAN {
	int frees, maxfrees, lostsize, losts;
	struct FREEINFO free[MEMMAN_FREES];
};

unsigned int memtest(unsigned int start, unsigned int end);
void memman_init(struct MEMMAN *man);
unsigned int memman_total(struct MEMMAN *man);
unsigned int memman_alloc(struct MEMMAN *man, unsigned int size);
int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size);
unsigned int memman_alloc_4k(struct MEMMAN *man, unsigned int size);
int memman_free_4k(struct MEMMAN *man, unsigned int addr, unsigned int size);

/* fifo.c */
struct FIFO32 {
	int *buf;
	int p, q, size, free, flags;
	struct TASK* task;
};
void fifo32_init(struct FIFO32 *fifo, int size, int *buf, struct TASK* task);
int fifo32_put(struct FIFO32 *fifo, int data);
int fifo32_get(struct FIFO32 *fifo);
int fifo32_status(struct FIFO32 *fifo);

/* graphic.c */
void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);
void boxfill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1);
void init_screen8(char *vram, int x, int y);
void putfont8(char *vram, int xsize, int x, int y, char c, char *font);
void putfonts8_asc(char *vram, int xsize, int x, int y, char c, unsigned char *s);
void init_mouse_cursor8(char *mouse, char bc);
void putblock8_8(char *vram, int vxsize, int pxsize,
	int pysize, int px0, int py0, char *buf, int bxsize);
	
#define COL8_000000		0
#define COL8_FF0000		1
#define COL8_00FF00		2
#define COL8_FFFF00		3
#define COL8_0000FF		4
#define COL8_FF00FF		5
#define COL8_00FFFF		6
#define COL8_FFFFFF		7
#define COL8_C6C6C6		8
#define COL8_840000		9
#define COL8_008400		10
#define COL8_848400		11
#define COL8_000084		12
#define COL8_840084		13
#define COL8_008484		14
#define COL8_848484		15


/* dsctbl.c */
// GDT(global segment descriptor table), 全局段号记录表，存储在内存
// 起始地址存放在GDTR的寄存器中
struct SEGMENT_DESCRIPTOR {
	short limit_low, base_low;
	char base_mid, access_right;
	char limit_high, base_high;
};

// IDT(intertupt descriptor table),中断记录表
// IDT记录了0-255中断号码与调用函数的对应关系
struct GATE_DESCRIPTOR {
	short offset_low, selector;
	char dw_count, access_right;
	short offset_high;
};
void init_gdtidt(void);
void set_segmdesc(struct SEGMENT_DESCRIPTOR *sd, unsigned int limit, int base, int ar);
void set_gatedesc(struct GATE_DESCRIPTOR *gd, int offset, int selector, int ar);
#define ADR_IDT			0x0026f800
#define LIMIT_IDT		0x000007ff
#define ADR_GDT			0x00270000
#define LIMIT_GDT		0x0000ffff
#define ADR_BOTPAK		0x00280000
#define LIMIT_BOTPAK	0x0007ffff
#define AR_DATA32_RW	0x4092
#define AR_CODE32_ER	0x409a
#define AR_INTGATE32	0x008e
#define AR_TSS32		0x0089
#define AR_LDT          0x0082 // LDT 段属性编号


/* int.c */
// PIC分为主PIC和从PIC,主PIC通过IRQ2与主PIC链接
// 主PIC负责处理0-7号中断，从pic负责处理8-15号中断信号

void init_pic(void);
void inthandler27(int *esp);
#define PIC0_ICW1		0x0020
#define PIC0_OCW2		0x0020
#define PIC0_IMR		0x0021
#define PIC0_ICW2		0x0021
#define PIC0_ICW3		0x0021
#define PIC0_ICW4		0x0021
#define PIC1_ICW1		0x00a0
#define PIC1_OCW2		0x00a0
#define PIC1_IMR		0x00a1
#define PIC1_ICW2		0x00a1
#define PIC1_ICW3		0x00a1
#define PIC1_ICW4		0x00a1


/* mt_task.c 多任务 */
#define MAX_TASKS  1000   // 最大任务数量
#define TASK_GDT0  3      // 定义从GDT的几号
#define MAX_TASKS_LV 100  // 一个level的task个数
#define MAX_TASKLEVELS 10 // 最多可以有多少个level

// 任务状态段（task status segment）
struct TSS32 {
	int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3; // 任务设置相关
	int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi; // 32位寄存器
	int es, cs, ss, ds, fs, gs; // 16位寄存器
	int ldtr, iomap; // 任务设置部分
};

struct TASK {
	int sel, flags; /* sel用来存放GDT的编号 */
	int level, priority; // priority 优先级
	struct FIFO32 fifo;
	struct TSS32 tss;
	struct SEGMENT_DESCRIPTOR ldt[2];
	struct CONSOLE* cons; // 所属的命令行
	int ds_base; // 程序的数据段地址的base值，因为目前一个命令行窗口只能运行一个程序
	int cons_stack; // 命令行程序运行时栈空间
	struct FILEHANDLE* fhandle; //文件相关操作
	int* fat;  // fat
	char* cmdline;
	unsigned char langmode, langbyte1;
};

// TASKLEVEL表示任务运行的层级，优先级上更加细粒度的划分，0优先级最高，处于level0的任务最先运行
struct TASKLEVEL {
	int running; /* 正在运行的任务数量 */
	int now;     /* 当前运行的时哪个任务 */
	struct TASK *tasks[MAX_TASKS_LV];
};

struct TASKCTL {
	int now_lv; // 现在活动中的level
	char lv_change; // 下次任务切换时是否需要改变level
	struct TASKLEVEL level[MAX_TASKLEVELS];
	struct TASK tasks0[MAX_TASKS];
};

extern struct TASKCTL *taskctl;
extern struct TIMER *task_timer;
struct TASK *task_init(struct MEMMAN *memman);
struct TASK *task_alloc(void);
void task_run(struct TASK *task, int level, int priority);
void task_switch(void);
void task_sleep(struct TASK* task);
struct TASK* task_now();

/* keyboard.c */
void inthandler21(int *esp);
void wait_KBC_sendready(void);
void init_keyboard(struct FIFO32* fifo, int data0);
#define PORT_KEYDAT		0x0060
#define PORT_KEYCMD		0x0064


/* mouse.c */
// x,y表示移动的矢量
// btn表示鼠标的点击按钮

struct MOUSE_DEC {
	unsigned char buf[3], phase;
	int x, y, btn;
};
void inthandler2c(int *esp);
void enable_mouse(struct FIFO32* fifo, int data0, struct MOUSE_DEC *mdec);
int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat);

/* sheet.c 图层管理*/
#define MAX_SHEETS  256

// 透明图层
struct SHEET {
	unsigned char* buf; // 描述的内容地址
	int bxsize, bysize, // 长宽
	vx0, vy0,           // 位置坐标，v(VRAM)
	col_inv,            // 透明色色号
	height,             // 图层高度(指所在的图层数吧)
	flags;              // 设定信息, 0x20表示图层是否需要光标，0x10表示窗口是否是应用程序, 0x01表示是否自动关闭
	struct SHTCTL* ctl;
	struct TASK* task;  // 所属的task
};

// 图层管理
struct SHTCTL {
	unsigned char* vram, *map; // vram、xsize、ysize代表VRAM的地址和画面的大小, map表示画面上每个点是哪个图层
	int xsize, ysize, top; // top代表最上层图层的高度
	struct SHEET* sheets[MAX_SHEETS]; // 有序存放要显示的图层地址
	struct SHEET sheets0[MAX_SHEETS]; // 图层信息
};

void sheet_free(struct SHEET* sht);
void sheet_slide(struct SHEET* sht, int vx0, int vy0);
void sheet_refresh(struct SHEET* sht, int bx0, int by0, int bx1, int by1);
void sheet_updown(struct SHEET* sht, int height);
void sheet_setbuf(struct SHEET* sht, unsigned char* buf, int xsize, int ysize, int col_inv);
struct SHEET* sheet_alloc(struct SHTCTL* ctl);
struct SHTCTL* shtctl_init(struct MEMMAN* memman, unsigned char* vram, int xsize, int ysize);

/* timer.c */
#define MAX_TIMER 500 // 最大可以设定500个定时器

struct TIMER {
	struct TIMER* next;
	unsigned int timeout;
	char flags, flags2; // flags2区分定时器是否应用程序结束自动取消
	struct FIFO32* fifo;
	int data;
};

struct TIMERCTL {
	unsigned int count, next; // next指下一个时刻
	struct TIMER* t0; // 存放排好序的timer地址
	struct TIMER timers0[MAX_TIMER];
};

void init_pit();
void inthandler20(int *esp);
struct TIMER* timer_alloc();
void timer_free(struct TIMER* timer);
void timer_init(struct TIMER* timer, struct FIFO32* fifo, int data);
void timer_settime(struct TIMER* timer, unsigned int timeout);
void timer_cancelall(struct FIFO32 *fifo);
int timer_cancel(struct TIMER *timer);
extern struct TIMERCTL timerctl;


/* window.c */
void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act);
void putfonts8_asc_sht(struct SHEET *sht, int x, int y, int c, int b, char *s, int l);
void make_textbox8(struct SHEET *sht, int x0, int y0, int sx, int sy, int c);
void make_wtitle8(unsigned char *buf, int xsize, char *title, char act);
void change_wtitle8(struct SHEET *sht, char act);

/* console.c */
struct CONSOLE {
	struct SHEET *sht;
	int cur_x, cur_y, cur_c;
	struct TIMER* timer;
};

struct FILEHANDLE {
	char *buf;
	int size;
	int pos;
};

void console_task(struct SHEET *sheet, unsigned int memtotal);
int* inthandler0d(int *esp);
int* inthandler0c(int *esp);
int* hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax);
void cons_putstr0(struct CONSOLE *cons, char *s);

/* file.c */
// 加载img文件中的文件名信息，0x002600字节开始
// name:文件名，如果第一个字节0xe5,表示文件已经删除，如果是0x00, 表示不包含文件名信息
// ext: 文件扩展名
// type: 文件属性信息
// reserve: 保留信息
// time, date: 时间信息
// clustno: 簇号，代表这个文件的内容从磁盘的哪个扇区开始存放，相差一个clustno即相差一个扇区(0x003e00为内容所在位置)
// size: 文件大小
struct FILEINFO {
	unsigned char name[8], ext[3], type;
	char reserve[10];
	unsigned short time, date, clustno;
	unsigned int size;
};
void file_readfat(int *fat, unsigned char *img);
void file_loadfile(int clustno, int size, char *buf, int *fat, char *img);
struct FILEINFO *file_search(char *name, struct FILEINFO *finfo, int max);
char *file_loadfile2(int clustno, int *psize, int *fat);
