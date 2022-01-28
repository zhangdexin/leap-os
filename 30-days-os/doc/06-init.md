## bootpack的初始化

### main函数
上一节最后的语句就是跳转到bootpack.c的main函数地址(HariMain)处：

```c
#include "bootpack.h"
#include <stdio.h>

// bookpack.c
void HariMain(void)
{
    struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct FIFO32 fifo, keycmd;
	int fifobuf[128], keycmd_buf[32];
	char s[40];
	int mx, my, i, j;
	unsigned int memtotal;
	struct MOUSE_DEC mdec;
	struct MEMMAN* memman = (struct MEMMAN*)MEMMAN_ADDR;
	struct SHTCTL *shtctl;
	struct SHEET* sht_back, *sht_mouse;
	unsigned char* buf_back, buf_mouse[256];
    static char keytable0[0x80] = { //... (略)};
    static char keytable1[0x80] = { //... (略)};

    struct TASK *task_cons[2], *task_a, *task;
    //... (略)

    // 初始化中断
    init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PIC初始化完成，开发CPU中断 */

    init_pit(); // 初始化pit
	init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);
	io_out8(PIC0_IMR, 0xf8); /*PIT和开发PIC1和键盘中断(11111000) */
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */

    //// 初始化内存管理   ////
    //... (略)
    memman_init(memman);
    //... (略)

    // 初始化画面
	init_palette();
    //... (略)

    // 初始化任务
	task_a = task_init(memman);
    //... (略)

    for (;;) {
    
        //... (略)
    }
}
```

这大概就是操作系统的全貌了，当然我省略了很多代码，从上往下看，最开始是include "bootpack.h", bootpack.h文件是一些结构体声明，或者是宏定义，整个操作系统的代码中只有这一个.h文件，所以这个文件会比较杂，我们后边讲解都是用到的一些声明会列出来。
然后是BOOTINFO，看下这个结构体的定义：
```c
// bookpack.h
struct BOOTINFO { /* 0x0ff0-0x0fff */
	char cyls;    /* 柱面数 */
	char leds;    /* 启动时键盘led的状态 */
	char vmode;   /* 显卡模式为多少位彩色 */
	char reserve;
	short scrnx, scrny; /* 画面分辨率x,y */
	char *vram; /*存放图像缓冲区的开始地址*/
};

#define ADR_BOOTINFO 0x00000ff0
```
可以看的出来，是在ADR_BOOTINFO(0x00000ff0)地址中的值赋值给binfo这个结构体指针，看注释也能知道表达的含义，那么如何知道0x00000ff0存放的是这些值呢，还得回到asmhead.nas的最开始：
```
; 有关BOOT_INFO存放地址
CYLS	EQU		0x0ff0			; 柱面数
LEDS	EQU		0x0ff1
VMODE	EQU		0x0ff2			; 关于颜色数目信息，颜色位数
SCRNX	EQU		0x0ff4			; 分辨率x
SCRNY	EQU		0x0ff6			; 分辨率x
VRAM	EQU		0x0ff8			; 存放图像缓冲区的开始地址
```
我们定义了各个变量的存放地址，同时在asmhead.nas这里我们也分别向这些地址中写了值。后边在C语言的写法中就可以使用这个结构体指针了，方便了很多。
然后声明了MEMMAN(设定内存的)，SHEET（设定图层），keytable0（键盘表），TASK（多任务）。这些后边我们都会一点一点的讲到。
再然后就是很多的初始化函数：init_gdtidt（中断及gdt相关），memman_init（内存相关），init_palette（画面相关），task_init（任务相关）。

### 初始化GDT
那么我们先从init_gdtidt()这个函数开始，这个函数是设定了gdt和idt的初始化，从而我们也更细致了解下：

因为之前的asmhead中设置的GDT和IDT只是临时使用，所以作者给出来就是还需要重新设置一遍：
这里我们GDT的初始化是用C语言来写的，对于我们整个理解GDT加载及设置有很好的帮助，首先我们看下数据结构：
```c
// bootpack.h
// GDT(global segment descriptor table), 全局段号记录表，存储在内存
// 起始地址存放在GDTR的寄存器中
struct SEGMENT_DESCRIPTOR {
	short limit_low, base_low;
	char base_mid, access_right;
	char limit_high, base_high;
};
```
这里是段描述符的表示，大家可以翻到的4节参考那个图来理解这个数据结构，内存中不同位置表示不同的含义，这里依次（从低到高）表示的段界限（low），段基址（low），段基址（mid），权限，段界限（hight<其中4位表示其他含义>），段基址（base）

这里作者写了一个函数来设置GDT的段描述：
```c
// dsctbl.c
void set_segmdesc(struct SEGMENT_DESCRIPTOR *sd, unsigned int limit, int base, int ar)
{
	if (limit > 0xfffff) {
		ar |= 0x8000; /* G_bit = 1 */
		limit /= 0x1000;
	}
	sd->limit_low    = limit & 0xffff;
	sd->base_low     = base & 0xffff;
	sd->base_mid     = (base >> 16) & 0xff;
	sd->access_right = ar & 0xff;
	sd->limit_high   = ((limit >> 16) & 0x0f) | ((ar >> 8) & 0xf0);
	sd->base_high    = (base >> 24) & 0xff;
	return;
}
```
这里limit和base也没什么好说，分别将其内容拆解到内存不同位置，我们看ar（access_right）是4字节保存的，把低8位赋值给access_right为访问权限，ar的第12~15位及limit的16~19组合赋值给limit_high，因为我们说过limit_hight中有4位表示其他含义。我们也可以看到如果limit的值大于0xfffff(1M),GDT段界限保存为20位宽，即最大是1M，如果超过1M我们需要把保存段界限的单位变成4K（原来是1字节），这里同样可以参考第4节我们对段描述符讲解的G_bit,G_bit设置为1，段界限的单位是4K，那么limit表示也要除以4K。

```c
// bootpack.h
#define ADR_GDT			0x00270000
#define ADR_BOTPAK		0x00280000
#define LIMIT_BOTPAK	0x0007ffff
#define AR_DATA32_RW	0x4092
#define AR_CODE32_ER	0x409a

// dsctbl.c
void init_gdtidt(void)
{
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
	
    //...

    /* GDT初始化 */
    for (int i = 0; i <= LIMIT_GDT / 8; i++) {
        set_segmdesc(gdt + i, 0, 0, 0);
    }
    set_segmdesc(gdt + 1, 0xffffffff,   0x00000000, AR_DATA32_RW);
    set_segmdesc(gdt + 2, LIMIT_BOTPAK, ADR_BOTPAK, AR_CODE32_ER);
    load_gdtr(LIMIT_GDT, ADR_GDT);
    
    //...
}
```
然后我们初始化整个GDT，首先将gdt存放在ADR_GDT(0x00270000)位置处。和第5章我们讲到，第0个段描述符是全0，同样也设置第一个段描述符指向全部内存，第二个段描述符从0x00280000，段界限是0x0007ffff。段属性AR_DATA32_RW(0x4092)大家可以简单理解成表示的是可读写的数据段，AR_CODE32_ER表示可读可写可执行的代码段。也就是说我们ADR_BOTPAK起始到LIMIT_BOTPAK都是操作系统（bootpack）的代码段，与第四章对比发现，DPL位是0，表明是系统模式，如果全为1表明是应用模式。当
cpu执行程序的代码段为系统模式，则cpu在系统模式下执行，程序的代码段是应用模式，cpu在应用模式下执行。当cpu在应用模式下执行时，不可以使用系统专用的段，同时不可以执行加载gdtr指令，在整个操作系统起到保护作用。

```
// naskfunc.nas
_load_gdtr:		; void load_gdtr(int limit, int addr);
		MOV		AX,[ESP+4]		; limit
		MOV		[ESP+6],AX
		LGDT	[ESP+6]
		RET
```
最后一步就是loadgdt，即将gdt的地址和上限加载到gdtr的寄存器中，这个寄存器有48位宽（6字节），这里ESP+4存放的是第一个参数limit，ESP+8存放的是addr，这里将limit移后2位，可以一次性将limit和addr都加载到gdtr中。

### 结束
后边就是设置IDT了，不过设置IDT还需要完整的讲解下中断的一些知识。就放到下一节了。