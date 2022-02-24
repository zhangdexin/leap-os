## 关于中断及设置-2
上一节我们说到中断的配置及跳转到的中断处理函数，我们上一节也留下了一个问题，就是操作系统是要如何才能产生一个中断向量呢？下边我们就来讲一下

### 中断设置知识储备
我们仔细想一下，当我们触发中断时，比如说键盘或者鼠标会把信号传递给CPU，CPU再产生中断号，然后去找中断处理函数。总结下就是外部设备传递信号给CPU，那么CPU肯定不会给每个外部设备分配一个接口。那么就会使用一个把所有的中断统一起来的外部设备，称为PIC(programmable interrupt controller)，可编程中断控制器。那么这个设备做了一些什么事情呢？这个设备就是负责把所有的中断信号接收，通过信号线发送给CPU（提供中断向量号）。如果同时产生很多中断，他也会维护一个队列，同时进行中断的优先级的判定。

我们代码中就是在bootpack.c的main函数处：
```
init_pic();
```
也就是说我们也对PIC进行一下设定，他才会发送给CPU中断号，进而去处理中断。

#### PIC
##### PIC芯片及信号线
我们来大致讲一下PIC吧，不然后边的代码会把大家搞晕。单独一个PIC芯片只有8个中断信号请求（IRQ<Interrupt ReQuest>）线:IRQ0~IRQ7，也就是说可以控制8个中断信号，这肯定是不够的，PIC采用了级联的方式来实现增加中断请求，如图所示：
<img src="https://user-images.githubusercontent.com/22785392/154479500-dee75ab8-8e65-4c62-b0de-af826efb8806.png" width="60%" height="60%" />
  
我们个人电脑只有两个PIC芯片，通过主PIC芯片IRQ2连接从PIC芯片。每一个IRQ会代表一种类型的中断，比如说IRQ0是时钟中断，IRQ1是键盘...

##### PIC处理信号流程
除此之外，我们还需要看下PIC内部大体的构造，如图所示：
<img src="https://user-images.githubusercontent.com/22785392/154481729-fc2f370f-4963-446a-941b-bf644a79b930.png" width="60%" height="60%" />
 
* INT:PIC芯片选择出来最高优先级中断请求，发送给CPU
* INTA: PIC芯片来接收CPU发来的中断响应信号
* IMR: Interrupt Mask Register, 中断屏蔽寄存器（8位），用来屏蔽来自外设的中断
* IRR: Interrupt Request Register, 中断请求寄存器（8位），保存经过IMR后等待处理的中断请求，相当于维护一个等待处理的中断队列
* PR: Priority Resoler, 优先级判别器，用来找出优先级最高的中断
* ISR: Interrupt Service Register, 中断服务寄存器（8位），保存正在被CPU出来的寄存器。
  
简单描述下原理：
  
当外设发出中断信号经过IRQ到达PIC内部时，首先到IMR处理，看IMR是否屏蔽了该IRQ的信号（IMR是8位(分别对应IRQ0~7)的寄存器，该位为1表示屏蔽）,如果屏蔽就丢弃这个信号，如果没有屏蔽信号进入到IRR(IRR也是8为寄存器，相应位为1则表示有该信号需要处理)保存。然后PR会从IRR中选择一个优先级最高的中断通过INT发送给CPU，当CPU处理完手头工作后会通过INTA发送给PIC芯片一个ack，然后PIC就会将ISR中相应的位置1，IRR中相应位置0，之后CPU会再次发送一个INTA信号给PIC（这里是CPU向PIC索要中断信号），PIC芯片会将初始中断向量号（后边讲到）+IRQ号发送给CPU，然后CPU就会执行相应的中断处理程序了。
  
那么当CPU处理完之后，如果PIC设置的中断结束模式EOI(End Of Interrupt)通知模式为手工模式，中断处理程序在结束处需要向PIC发送通知，PIC收到通知表示知道这个中断结束了。如果PIC设置EOI为自动模式，PIC在收到CPU索要中断向量号时就认为结束了，那么PIC认为结束时就会将ISR相应位置0，接着就等下一个中断来了。

##### PIC的设置寄存器
除了上边所说的PIC的结构，PIC还有两组寄存器，一组是初始化命令寄存器用来初始化命令字（Initialization Command Words， ICW），ICW共四个ICW1～ICW4。另外一种是操作命令寄存器，用来保存操作命令字的（Operation Command Word，OCW），OCW共三个OCW1～OCW3。所以我们对PIC的设置，分为初始化和操作两部分。

用ICW做初始化的主要功能包括：是否需要级联，设置起始中断向量号，设置中断结束模式。如何设置呢？就是从CPU往PIC系列端口写入数据。并且写入要按照严格的顺序写入，避免关联依赖的一些问题。
  
<img width="50%" height="50%" src="https://user-images.githubusercontent.com/22785392/155432195-5f55abce-342c-478a-b7ee-16b7bd81a06b.png" />

我画了下ICW的图，如图所示，我一个一个来解释下：
1. ICW1主要功能是设置连接方式和中断信号触发方式。连接方式是指用单片工作还是多片级联工作，信号触发方式是电平触发还是边沿触发。
设置ICW1，是写入主片的的0x20端口和从片的0xA0端口。IC4表示是否要写入ICW4，x86系统必须要写入。SNGL表示是否是单片，为1表示单片，0表示级联。ADI在x86中不需要设置。LTIM（level/edge triggered mode）设置中断触发模式，0表示边沿触发，1表示电平触发。5～7位无需设置。
2. ICW2主要功能用来设置起始的中断向量号，那么每个IRQ上产生的中断向量号即位起始的中断向量号加上IRQ的号码。设置ICW2，是写入主片的0x21端口和从片的0xA1端口。且ICW2只需要填写高5位。
3. ICW3仅在级联的方式下才需要（如ICW1中的SNGL位0），由于主片和从片的级联方式不一样，主片和从片有自己的结构。设置ICW3需要写入主片0x21端口和从片0xA1端口，主片中ICW3中置1的那位对应的IRQ用于连接从片。从片并不用IRQ连接主片，那么设置从片连接主片的方式就是将主片连接自己IRQ号写入到ICW3中。
4. ICW4用户设置一些模式等等。设置ICW4需要写入主片0x21端口和从片0xA1端口。5～7位未定义，SFNM表示是否特殊全嵌套模式（special fully nested mode）<我也不懂>，BUF表示是否是缓冲模式，为1为缓冲模式，缓冲模式下M/S位用来规定本片是否是主片。非缓冲模式下M/S无效。AEOI设置是否是自动结束中断，我们前边也提到如果是非自动模式需要向PIC发送结束标识。‘u PM’位表示微处理器类型，用来兼容老处理器，0表示8080和8085处理器，1表示x86处理器。

我们看到ICW2～ICW4都是相同端口，那么PIC怎么区分呢，答案就是顺序，4个ICW要按照一定顺序（即ICW1～ICW4）写入，这样PIC就可以区分了。并且我们也能知道ICW1其实控制了是否会写入ICW3和ICW3。

我们代码就只用了OCW来设置结束标识，我们也就不展开讲了。

### 设置PIC
大致的中断的知识我们也讲到这里，我们接下来看下代码，看下init_pic函数里边对PIC进行了什么设置

```C++
// bootpack.h
#define PIC0_ICW1  0x0020
#define PIC0_OCW2  0x0020
#define PIC0_IMR  0x0021
#define PIC0_ICW2  0x0021
#define PIC0_ICW3  0x0021
#define PIC0_ICW4  0x0021
#define PIC1_ICW1  0x00a0
#define PIC1_OCW2  0x00a0
#define PIC1_IMR  0x00a1
#define PIC1_ICW2  0x00a1
#define PIC1_ICW3  0x00a1
#define PIC1_ICW4  0x00a1

// int.c
void init_pic(void)
/* PIC的初始化 */
{
 io_out8(PIC0_IMR,  0xff  ); /* 禁止所有中断 */
 io_out8(PIC1_IMR,  0xff  ); /* 禁止所有中断 */

 io_out8(PIC0_ICW1, 0x11  ); /* 边沿触发模式, 级联, 要写入ICW4*/
 io_out8(PIC0_ICW2, 0x20  ); /* IRQ0-7由INT20-27接收 */
 io_out8(PIC0_ICW3, 1 << 2); /* PIC0与IRQ2连接设定 */
 io_out8(PIC0_ICW4, 0x01  ); /* 无缓冲模式 */

 io_out8(PIC1_ICW1, 0x11  ); /* 边沿触发模式, 级联, 要写入ICW4 */
 io_out8(PIC1_ICW2, 0x28  ); /* IRQ8-15由INT28-2f接收 */
 io_out8(PIC1_ICW3, 2     ); /* PIC1与IRQ2连接设定 */
 io_out8(PIC1_ICW4, 0x01  ); /* 无缓冲模式 */

 io_out8(PIC0_IMR,  0xfb  ); /* 11111011 PIC1以外全部禁止 */
 io_out8(PIC1_IMR,  0xff  ); /* 11111111 禁止所有中断 */

 return;
}
```
* 首先通过设置IMR禁用所有的中断，我们上边也提到过IMR中会存放是否禁用了该类型的中断，0xff二进制全是1表示所有的外部到来的中断都屏蔽掉。
* 然后通过设置主片的ICW1, 设置为0x11： IC4位是1，表示要使用ICW4，SNGL位是0，会级联，ICW3也会被使用。LTIM位是0，边沿触发模式。
* 然后设置主片的ICW2，起始的中断向量号0x20，那么后边每次中断号都是IRQ号加上起始中断向量号发送给CPU，比如说键盘的中断向量号是0x21，这样我们结合上节讲到的asm_inthandler21中断处理程序就说键盘的中断处理程序。
* 设置主片ICW3，主片的IRQ2和从片(PCI1)连接
* 设置主片ICW4，0x01:非缓冲模式，AEOI通知时手动通知，即中断结束时需要手动发送结束通知给PIC
* 从片的设置和主片类似，唯一不同的时从片的ICW3需要设置自己与主片的哪个IRQ相连，这里是2号IRQ。
* 最后，我们第一步因为在设置PIC的时候首先需要把外部中断禁用，不然可能会有错乱。等设置完成后，我们最开始初始时基本也只是打开了PIC0(主片)和PIC1(从片)连接的中断，其他也还是屏蔽掉，后边我们如果需要接受哪个中断就打开哪个。

然后上边也说到了我们结束通知模式是手动的，那我们看下如何通知给PIC呢，我们还是以asm_inthandler21->inthandler21 这个中断处理程序为例：
```C++
// keyboard.c

void inthandler21(int *esp)
{
	int data;
	io_out8(PIC0_OCW2, 0x61);	/* 通知PIC"IRQ-01"已经受理完成 */
	data = io_in8(PORT_KEYDAT);
	fifo32_put(keyfifo, data + keydata0);
	return;
}
```
上边我们也看到io_out8(PIC0_OCW2, 0x61)这句代码即为向PIC告知中断处理完成，我们也不展开说了。

### 总结
到这里我们已经把中断的基本知识讲完，同时也讲了下代码中关于初始化中断的部分进行讲解，我们回忆总结下，初始化中断基本就是设置IDT，表示中断发生时，CPU通过中断向量号能够找到中断处理程序。那么中断怎么来，一部分是程序中写的INT指令等触发的软中断，还有一部分是外部中断的触发，那么如何设置能够接受到外部中断呢，那就是通过设置PIC即可。
