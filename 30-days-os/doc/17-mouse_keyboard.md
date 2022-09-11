## 键盘鼠标

### 序
本章我们开始讲一下关于如何处理鼠标键盘，鼠标的很多设定也是包含在键盘的设定中，我们也是通过对外设的设定然后当鼠标按下或者键盘按键触发中断，再进一步执行中断处理函数，相信大家对这一套流程已经很熟悉了，接下来我们还是先大概说一下如何针对鼠标和键盘的端口进行设定吧。

### 知识储备
看到上边我们知道了也是通过中断来触发，同时可以跳到第八章的中断的PIC芯片上就有关于接收键盘和鼠标的信号，不过连接到PIC的还有另外的专门处理鼠标和键盘的外设，这个外设或者芯片和PIC进行沟通。所以我们就是要对这个芯片进行设定或者说初始化。<br/>
我们就需要看下这个芯片的结构：<br/>
这个设备最重要的组成部分是四个寄存器：<br/>
<img width="261" alt="image" src="https://user-images.githubusercontent.com/22785392/188523867-913690b4-4100-4b3f-826b-3d5cd22c8ab6.png">
<br/>

先来看下这个**输入输出的寄存器**，当处理器要往芯片写入数据时，写入到输入缓冲区。当处理器要从芯片读取数据，读取输出缓冲区，那这里的输出和输入的概念自然就是相对于芯片来说的。我们会向输入缓冲区寄存器写入控制指令，我们后边写代码中会说到。<br/>
然后再看下**状态寄存器**，我们看下我们可能会用到的各个位的作用：
* 位0置1表示输出缓冲区寄存器已满，处理器可以通过in指令来读取，读取后自动置0
* 位1置1表示输入缓冲区寄存器已满，芯片将值读取后自动置0

最后看下**命令寄存器**：<br/>
* 位0置1表示启用键盘中断
* 位1置1表示启用鼠标中断
* 位4置1表示禁止键盘
* 位5置1表示禁止鼠标

别的位暂且不会用到，这里不展开讲<br/>

这里的四个寄存器实际会涉及到两个端口，读取时要看你读取那个寄存器的数据，我们这里读取只会涉及到读取状态寄存器，那么直接读取0x64端口就可以了。写入时分为命令端口（0x64）和数据端口（0x60），也就是说我们要控制芯片设定需要进行两步，首先写入命令端口，然后写入数据端口。(不过你如果读取命令寄存器也是需要这两步)
具体写入的命令及数据可以参照[这里](http://aodfaq.wikidot.com/kbc-commands)。

### 代码
#### 初始化
那么接下来我们就从代码中来理解了，先来看bootpack.c中的HariMain函数中的初始化init_keyboard和enable_mouse：
```c++
// bootpack.h

// x,y表示移动的矢量
// btn表示鼠标的点击按钮
struct MOUSE_DEC {
	unsigned char buf[3], phase;
	int x, y, btn;
};

// bootpack.c
void HariMain(void)
{
    // ...(略)
    struct MOUSE_DEC mdec;

    // ...(略)
    init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);
    // ...(略)
}
```
使用MOUSE_DEC来存放鼠标触发时的一些信息，鼠标的数据一般都是3个字节，所以就是需要累积到3个字节开始处理，buf就是用来存放着三个字节的数据的，phase更像是现在接受数据到哪个阶段了。x和y就构成了鼠标的位置。btn表示是鼠标的按键状态（左，中，右键是否按下等），具体的数据表示后边还会继续说。<br/>
然后就是调用init_keyboard和enable_mouse初始化芯片或者设定。
```c++
// bootpack.h
#define PORT_KEYDAT		0x0060
#define PORT_KEYCMD		0x0064

// keyboard.c
#define PORT_KEYSTA				0x64
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
```
init_keyboard中可以看到传递过来fifo用来之后存放键盘的按键数据，也就是说我们如果有按键数据就会填充到这个队列中，前边讲到多任务的时候也会说到这个队列，可以返回去复习一下。总结来说就是放置到队列中数据，就会有任务去处理这个数据。然后传递过来了一个data0，这个数据其实是用来区隔不同类型的。因为我们鼠标键盘等都是用到fifo这个队列，那么取出来的数据怎么更好的区分是键盘数据还是鼠标数据呢，基本做法就是实际的数据加上这个data0数据放置到fifo队列中处理。我们看调用那里keyboard是256，mouse是512，也就是说当从fifo中取出数据发现数值是在256和512之间的就是键盘数据，大于512就是鼠标数据等等。<br/>
然后看调用wait_KBC_sendready函数：<br/>
读取0x64的端口的数据，判断第1位是否为0，结合我们上边说的，是读取状态寄存器的位1，从而可以判读输入寄存器是否为空，也就是说输入寄存器是否处于初始化状态或者处理器写入到输入寄存器的数据是否被芯片读取了。<br/>
init_keyboard然后调用io_out8向端口写命令和数据，0x60（KEYCMD_WRITE_MODE）+0x47（KBC_MODE）的组合表示会将0x47数据写入到命令寄存器，我们看0x47二进制是（1000111）开启了键盘中断和鼠标中断。<br/>
看完了键盘的初始化，我们跳到鼠标的初始化函数那里：<br/>
```C++
// bootpack.h
#define PORT_KEYDAT		0x0060
#define PORT_KEYCMD		0x0064

// mouse.c
#define KEYCMD_SENDTO_MOUSE		0xd4
#define MOUSECMD_ENABLE			0xf4

void enable_mouse(struct FIFO32* fifo, int data0, struct MOUSE_DEC *mdec)
{
	// fifio缓冲区信息保存在全局变量
	mousefifo = fifo;
	mousedata0 = data0;

	/* 激活鼠标 */
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	
	/* 如果往键盘控制电路送指令0xd4,下一个数据会自动发送给鼠标，用以激活
	   鼠标会返回CPU一个ACK,即0xfa.
	*/
	mdec->phase = 0; /* 等待0xfa阶段 */
	return;
}
```
同样传递过来data0和要存放鼠标中断信息的队列，通过向命令端口和数据端口写入数据来初始化鼠标，0xD4命令表示下一个命令要发给鼠标，0xf4表示会激活鼠标，0xf5表示关闭鼠标，这里也不用深究。我们看到注释中说鼠标收到后会返回一个ACK给CPU。等下我们会讲到处理这个数据，然后phase初始化为0。<br/>

#### 打开鼠标键盘的中断
如果鼠标和键盘产生了数据，会传递给PIC（之前中断那里可以去看下），PIC再进一步触发中断，所以当产生了数据我们需要保证他可以到达cpu，也就是说PIC这里要设定打开鼠标和键盘的中断。
```c++
// bootpack.c
void HariMain(void)
{
    // ...(略)
    struct MOUSE_DEC mdec;

    // ...(略)
    init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);

    io_out8(PIC0_IMR, 0xf8); /*PIT和开发PIC1和键盘中断(11111000) */
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */
    // ...(略)
}
```
初始化之后，我们设定PIC的主片和从片的IMR寄存器开放中断，机器中就有键盘中断和鼠标中断，关于PIC的知识可以到中断的那两章学习。

#### 中断处理
既然鼠标和键盘的中断可以传送到PIC这里，我们首先要设定中断处理函数，还是需要回到desctbl.c中，结合中断那章我们可以知道，键盘鼠标的中断号分别是0x21和0x2c：<br/>
```c++
// dsctbl.c

void init_gdtidt(void)
{
    // ...(略)

    set_gatedesc(idt + 0x21, (int) asm_inthandler21, 2 * 8, AR_INTGATE32);
    set_gatedesc(idt + 0x2c, (int) asm_inthandler2c, 2 * 8, AR_INTGATE32);
    // ...(略)
}
```
那么我们的中断处理函数就是asm_inthandler21和asm_inthandler2c，进一步asm_inthandlerxxx会调用c语言中的inthandlerxxx，所以最终的中断处理函数就是inthandler21和inthandler2c。<br/>
看下键盘处理函数的实现：
```c++
// bootpack.h
#define PORT_KEYDAT		0x0060

// keyboard.c
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
```
可以看到，我们首先同时PIC处理中断完成，然后读取键盘控制芯片的0x60端口数据，这里可以直接读取到芯片的输出缓冲区，进一步可以去到键盘按键的键码（键码可以标志按了哪个键），放置到fifo队列中等待处理。不过这里有点要注意到，拿到键码后加上keydata0，这样就使得键码都是大雨data的，我们处理的时候也要相应减去keydata0得到键码。<br/>
继续看下鼠标的中断处理函数：
```c++
// bootpack.h
#define PORT_KEYDAT		0x0060

// keyboard.c
void inthandler2c(int* esp)
/* 来自PS/2鼠标的中断 */
{
	int data;
	io_out8(PIC1_OCW2, 0x64);	/* 通知PIC1 IRQ-12受理完成 */
	io_out8(PIC0_OCW2, 0x62);	/* 通知PIC0 IRQ-02受理完成 */
	data = io_in8(PORT_KEYDAT);
	fifo32_put(mousefifo, data + mousedata0);
	return;
}
```
首先要告诉pic处理完了，因为鼠标的中断是在从片上，所以先通知POC1，然后通知PIC0。然后就是通过芯片的输出寄存器中取出data加上mousedata0放置到队列中等待处理。

### 总结
篇幅所限，我们这一章先到这里了，下一章我们继续讲鼠标和键盘的数据处理及解析出键码和鼠标的按键信息。



