## GDT初始化和中断

### 前言
这一节我们来讲关于中断，同时之前的GDT加载的部分我们还需要重新改动下，因为之前的GDT的加载是作者临时拿一块内存存储，这里是要重新规划下。

### GDT初始化
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

### 中断知识储备
上一小节把之前留下的关于GDT的知识梳理了一下，接下来我们讲述下中断。

#### 什么是中断
首先是中断的概念，中断即为cpu在做一件事情然后被打断去干其他的事情，举个例子来讲，比如说cpu正在压缩一个文件，这个时候你鼠标点击播放音乐，这里鼠标点击等后续一系列处理逻辑可以划分为中断处理内。我们平常也会有中断事情，比如说你在看书，你妈叫你吃饭，那吃饭就可以被称为中断。
中断可以分为外部中断和内部中断，这里外部内部是针对CPU来说嘛！外部中断有被称为硬件中断，又可以细分为可屏蔽中断和不可屏蔽中断。内部中断分为软中断和异常。
外部中断的中断源头肯定是来自于硬件，比如说网卡，打印机等等。可屏蔽中断是指cpu可以屏蔽掉不去处理的，体现在通过设置eflags的寄存器IF位来屏蔽外部中断，一般来说正常的外部中断都可以称为可屏蔽中断，比如说接收网卡的中断，硬盘等等。不可屏蔽中断可以理解成为“即将宕机”的中断，比如说电源掉电，内存读写错误等。这些中断一旦发生，整个计算机就会基本上宕机，这种中断也是屏蔽不了的。
内部中断的中断源来自于CPU内部，分为软中断和异常。软中断即来源于软件中，程序运行期间主动发起的，并不是错误，比如说我们可以使用“INT 8位立即数”指令来发起中断，我们实现系统调用也是使用这种形式。异常是CPU执行期间产生了错误，比如说除0的错误，按照轻重程度来分大致分为，Fault（故障），Trap（陷阱），Abort（终止）。

#### 中断描述符表
中断本质是对于CPU来说，当来了一个中断信号，CPU去执行相应的中断处理程序，那么这个过程怎么实现呢？这里使用**中断描述符表**来统一管理，和我们前边讲的全局描述符表很像，我们进入保护模式下也要设置中断描述符表，之前在实模式下是BIOS为我们提供中断向量表来达到中断的效果.当产生了中断后，为每一个中断分配一个整数，这个整数就是中断号，也称为中断向量，同样这个中断号也是作为中断描述符表的索引进而寻找中断处理程序。
同GDT一样，中断描述符表中存放的是中断描述符，这里中断描述符还有一个别的名字叫做中断门描述符。令人意外的是，中断描述符表中不止有中断门描述符，还可以存放一些其他类型的描述符，也叫其他类型的门描述符，比如说任务门描述符，陷阱门描述符，这些我们后边再说，这里我们就理解到中断描述符表中有中断描述符即可。

#### 中断（门）描述符
然后我们来看下中断描述符长啥样，大家可以和全局描述符相对应着来理解。很多位表示的含义也和全局描述符表一样。大概来讲解下：
* 低32位的31~16位表示的目标代码段的选择子，比较好理解，中断处理程序肯定是指向一段程序，那么选择子也就是指向全局描述符表的代码段的。
* 低32位的15~0位和高32位的31~16位拼起来组合成目标代码段的程序的偏移地址。
* 高32位的4~0位未使用，5，6，7位为0
* 高32位的S和TYPE位能够标识出来这个描述符的类型，我们之前讨论过S表示是系统段还是非系统段，这里说明中断门描述符是系统段，全局描述符是非系统段，其他类型的描述符遇到再说。TYPE对应的各位的数据是D110，这个D也是为了兼容之前的cpu，D为0是16位的保护模式，D为1表示32位的保护模式，所以我们这里其实就是D为1。
* DPL表示特权级，后边再说
* P表示此段是否在内存中，我们设定他为1就好

当通过中断门描述符进入中断处理程序，标志寄存器eflags的IF位会置为0，表示中断关闭，避免中断嵌套。Linux系统也是通过此方式进入中断处理程序

#### 中断描述符表寄存器
和全局描述表类似，中断描述符表的起始地址也需要放到一个寄存器中，方便cpu寻找。这个寄存器称为中断描述符表寄存器，该寄存器分为两部分，15~0位表示IDT的界限，47~16位表示IDT基地址。即用32位来表示基地址，16位来表示界限。那么即可以容纳2^16 / 8字节 = 8192个中断描述符。中断向量号0是除法错，即第一个中断描述符。处理器其实只支持256个中断（0~255）。

### 中断的流程
一个完整的中断分为CPU外部和CPU内部。
* CPU外部：外部中断首先由中断代理芯片接收，处理后将该中断的中断向量号发给CPU
* CPU内部：CPU拿到中断向量号后去执行中断处理程序

关于外部中断到中断代理芯片我们后边再将，这里首先梳理下CPU拿到中断向量号去执行中断处理程序的步骤。如图所示。

1. 首先cpu拿到中断向量号，或是从中断代理芯片到来或者是内部中断发起，它用此向量号在中断描述符表中查询对应的中断描述符，然后去执行相应的中断处理程序。由于中断描述符是8字节，所以需要乘以8再和IDTR的中断描述符表地址相加，到此便找到了中断描述符
2. 第二步是要检查中断的特权级，这里先不展开说
3. 然后就是去执行中断处理程序，将中断门描述符中目标代码段选择子加载到代码段寄存器CS中，中断门描述符中偏移地址加载到EIP寄存器，然后开始执行了。这些都是CPU自动去做。

### IDT的初始化

有了上边的知识，我们来实际看下代码。首先我们是初始化IDT

```c
// bootpack.h
// IDT(intertupt descriptor table),中断记录表
// IDT记录了0-255中断号码与调用函数的对应关系
struct GATE_DESCRIPTOR {
	short offset_low, selector;
	char dw_count, access_right;
	short offset_high;
};
```

```c
// dsctbl.c
void set_gatedesc(struct GATE_DESCRIPTOR *gd, int offset, int selector, int ar)
{
	gd->offset_low   = offset & 0xffff;
	gd->selector     = selector;
	gd->dw_count     = (ar >> 8) & 0xff;
	gd->access_right = ar & 0xff;
	gd->offset_high  = (offset >> 16) & 0xffff;
	return;
}
```

```c
// bootpack.h
#define ADR_GDT			0x00270000
#define LIMIT_GDT		0x0000ffff
#define AR_INTGATE32	0x008e

// dsctbl.c
void init_gdtidt(void)
{
    // ...
	struct GATE_DESCRIPTOR    *idt = (struct GATE_DESCRIPTOR    *) ADR_IDT;
    // ...
    
    /* IDT的初始化 */
	for (i = 0; i <= LIMIT_IDT / 8; i++) {
		set_gatedesc(idt + i, 0, 0, 0);
	}
	load_idtr(LIMIT_IDT, ADR_IDT);

	/* IDT的設定 */
	// 0x20~0x2f用于中断信号
	// 0x00~0x1f,操作系统使用，应用程序触发操作系统保护时，触发中断0x00~0x1f
	set_gatedesc(idt + 0x0c, (int) asm_inthandler0c, 2 * 8, AR_INTGATE32);
	set_gatedesc(idt + 0x0d, (int) asm_inthandler0d, 2 * 8, AR_INTGATE32);
	set_gatedesc(idt + 0x20, (int) asm_inthandler20, 2 * 8, AR_INTGATE32);
	set_gatedesc(idt + 0x21, (int) asm_inthandler21, 2 * 8, AR_INTGATE32);
	set_gatedesc(idt + 0x27, (int) asm_inthandler27, 2 * 8, AR_INTGATE32);
	set_gatedesc(idt + 0x2c, (int) asm_inthandler2c, 2 * 8, AR_INTGATE32);
	set_gatedesc(idt + 0x40, (int) asm_hrb_api, 2 * 8, AR_INTGATE32 + 0x60); // +0x60表示应用程序可以使用该中断号
    
	return;
}
```



### 中断处理程序

### 总结