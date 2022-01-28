## 关于中断及设置

### 中断知识储备
这一节我们来讲中断，上一节把之前留下的关于GDT的知识梳理了一下，接下来我们讲述下中断。

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
<img src="https://user-images.githubusercontent.com/22785392/150312587-1df62adf-a22b-4d9f-9b6a-aa8e7863b61d.png" width="80%" height="80%" />

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
<img src="https://user-images.githubusercontent.com/22785392/150312436-8fa0da84-9895-4846-bddd-4b984b979e11.png" width="60%" height="60%" />

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
这里是中断描述符的结构，从低位到高位依次是低16位的偏移值（offset_low）,选择子（selecttor），默认或者未使用的8位（dw_count）,权限及类型设定（access_right）,高16位的偏移（offset_high）。

```c
// bootpack.h
#define ADR_IDT			0x0026f800
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

同样也是在init_gdtidt函数中来初始化IDT，首先把idt中每个描述符设为0，LIMIT_IDT是8192，每个描述符8字节，8192/8是描述符的数量。然后调用load_idtr来设置中断描述符表寄存器。load_idtr也是必须用汇编来写。
```
; naskfunc.c
_load_idtr:		; void load_idtr(int limit, int addr);
		MOV		AX,[ESP+4]		; limit
		MOV		[ESP+6],AX
		LIDT	[ESP+6]
		RET
```
将limit和addr组合在一起（6字节），使用LIDT加载到中断描述符表寄存器中。
我们再回到init_gdtidt中，下面就是为每个中断向量号设置中断处理程序，中断向量号对应0x00到0x1f（0~31）已经cpu设定好的中断类型，准确来说是0~19是设定好的，20~31是预留的。但是我们也还是要自己来写中断处理程序。这里比如说0x0c是栈段发生错误，0x0d是一般的保护错误，这里我们后边会讲到，那么留给操作系统自己设定的就是0x20~0xff(32~255)号的中断。
```c
set_gatedesc(idt + 0x20, (int) asm_inthandler20, 2 * 8, AR_INTGATE32);
```
以这个为例，设定0x20的中断处理程序，asm_inthandler20是中断处理程序（也即在目标代码段的偏移），设定目标代码段是2（2*8表示）号，AR_INTGATE32的具体内容我们结合set_gatedesc函数一同看下：
```c
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
把offset的低32位和高32位分别取给offset_low和offset_high，如果AR_INTGATE32值为0x008e，dw_count的8位全部置为0，剩余的8位为0x8e（10001110），可以看到type设置为了1110，S位为0表示系统段，P位设置为1，DPL为00表示操作系统的特权。如果AR_INTGATE32为0x008e+0x60=0x00ee,和0x008e不同是DPL是11表示用户的特权。

### 中断处理程序
我们再次回到init_gdtidt函数，看下设置的中断处理程序，以asm_开头，能看出来也是汇编写的，为什么用汇编来写呢，作者给出的回复是从中断处理程序中返回需要用的'iretd'指令，这个只能是汇编来写。我们以asm_inthandler20这个为例，看下汇编的代码：
```
; naskfunc.nas

; 处理中断时，可能发生在正在执行的函数中，所以需要将寄存器的值保存下来
_asm_inthandler20:
		PUSH	ES
		PUSH	DS
		PUSHAD
		MOV		EAX,ESP
		PUSH	EAX
		MOV		AX,SS
		MOV		DS,AX
		MOV		ES,AX
		CALL	_inthandler20
		POP		EAX
		POPAD
		POP		DS
		POP		ES
		IRETD
```
这里不用很细致的了解，只是首先要将之前的寄存器入栈，退出中断处理程序时在出栈，PUSHAD是把一坨寄存器入栈，POPAD是把这坨寄存器相反的方向出栈，省掉PUSH和POP指令。
这里看到还是会调用_inthandler20这个函数，这个函数是用C语言写的，这样方便我们来写流程。我们就简单的看下函数声明，先不细讲，后边用到时再详细：
```
void inthandler20(int *esp)
{
	// ...(略)
}
```
inthandler20正好时定时器的中断处理程序, 看到汇编处使用IRETD（d表示32位操作数）从中断处理程序返回。

### 总结
这里的话我们把中断的下半部分讲完了，即cpu接到中断向量号，来调用中断处理程序的这部分，那么怎么设置触发中断呢，这个我们下一节再来说。