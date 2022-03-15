## 定时器
这一章节我们来讲定时器，关于定时器，我们从前边学习中断的时候也知道，定时器也是通过硬件来触发中断，然后通过PIC发送给CPU中断信号。定时器也即每隔一段时间就会发送信号给CPU，那么如果让它发来信号呢，又如何设置多久发来一次信号呢，那就是我们也还是对定时器的硬件进行设置，对于CPU来说，只需要知道是设置PIT就好了，PIT（Programmable Interval Timer）可编程的间隔型定时器，电脑中PIT连接着PIC的IRQ0号，PIT发出中断信号给IRQ0，然后PIC就会发送中断向量号给CPU，进一步执行中断处理程序。

## 设置PIT
### 关于PIT知识储备
我们也简单的来讲下PIT的内部结构吧，PIT内部根据功能的划分有三个计数器，对应端口分别是0x40~0x42，计数器0用于产生实时时钟信号。计数器1专用于DRAM的定时刷新。计数器2专用于扬声器发出声音。所以我们这里使用的就是计数器0。
然后PIT内部有一个控制字寄存器，用来对使用哪一个计时器及如何使用计时器进行控制的一些设定。如图所示：

<image src="https://user-images.githubusercontent.com/22785392/158379662-9377c9a3-bbd3-4b09-878f-122da2bf3876.png" width="40%" height="40%" />

* SC1~SC0就是选择哪一个计数器来使用
* RW1~RW0表示使用的计数器读/写/锁存的方式，00表示锁存数据仅供CPU读，01表示只读写低字节，10表示只读写高字节，11表示先读写低字节，后读写高字节
* M2~M0用来指定工作的方式，有6种方式，不同方式表示计数器的不同技术过程，启动停止方式，我们这里使用的方式2，这种方式我们仅仅关注他会循环计数，即当计数器到达设定的初始值，计数器会重新开始计数，循环触发。
* 最后一位BCD表示使用是否使用十进制，1表示十进制，0表示使用二进制。

给计数器设定以初始值，计数器将初始值每次减1，当减到0时，就会向CPU发出中断信号。计数器的工作频率是1.19318MHz，也即一秒钟可以进行1.19318M次计算，那就是一秒钟可以将1193180减到0。计数器钟的用来存放初始值寄存器是16位（0~65535），默认是全0，表示的是65536。那我们就以初值寄存器设定为0，也即初值为65536为例来算一下。计数器1秒钟可以把1193180减为0。那么初值为65536时，将65536减为0需要的时间是65536/1193180=0.05492s，也就是0.05492s发出一次中断信号，触发一次中断。那你如果不去设定初值，定时器就会0.05492s触发一次中断。

### 如何设定PIT
有了上边简单的知识了解，那我们就可以对PIT进行设定，然后就可以触发中断了
首先我们要对PIT的控制寄存器设定，端口是0x43
然后对计数器进行初始值设定，端口是0x40（这里只使用计数器0）
这里都是使用out的指令就可以

## 初始化PIT
```C
// bootpack.c
void HariMain(void)
{
    // ...(略)
    init_pit(); // 初始化pit
    // ...(略)
}


// timer.c
struct TIMERCTL timerctl;

void init_pit() {
    //...(略)
}

```
继续我们的代码讲解，bootpack.c中接下来就会调用init_pit来初始化pit的设置，我们然后看下这个函数是做了什么。
我们在timer.c中声明了一个TIMERCTL的数据结构，我们先去看下：
```C
// bootpack.h
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
	struct TIMER* t0;         // 存放排好序的timer地址
	struct TIMER timers0[MAX_TIMER];
};
```
在操作系统中，可能会设定很多的定时器，这里限制是500个。<br/>
用TIMER来作为定时器的数据结构，next指向下一个定时器，作者这里涉及的定时器相当于是排好序的，那么第一个定时器首先触发，next指向的下一个定时器就会接下来触发。flags表示定时器的状态，flags2主要给应用程序使用。timer它还绑定了一个队列，这样中断触发时，中断处理函数里会将中断数据放到队列中，等待之后的业务处理。data就是我们设置给这个定时器的参数。<br/>
用TIMERCTL对所有的TIMER进行整合及控制，count表示PIT向cpu发送信号的次数，也就是时钟中断的次数。next表示下一个超时的时刻，也就是最近的一个定时器超时的时刻。t0就表示最先超时的那个定时器。timers0存放的是所有的定时器。

```C
// timer.c
#define PIT_CTRL 0x0043
#define PIT_CNT0 0x0040

// 定时器状态
#define TIMER_FLAGS_UNUSE 0 // 未使用
#define TIMER_FLAGS_ALLOC 1 // 已配置
#define TIMER_FLAGS_USING 2 // 运行中

// AL=0x34, OUT(0x43, AL)
// AL=中断周期的低8位, OUT(0x40, AL)
// AL=中断周期的高8位, OUT(0x40, AL)
void init_pit() {
    int i;
    struct TIMER* t;

    io_out8(PIT_CTRL, 0x34);
    // 设置中断周期为11932, 中断频率就是100hz，即1s钟发生100次中断, 10ms一次
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);

    // 初始化数据
    timerctl.count = 0;
    for (i = 0; i < MAX_TIMER; i++) {
        timerctl.timers0[i].flags = TIMER_FLAGS_UNUSE;
    }
    t = timer_alloc(); // 取得一个
    t->timeout = 0xffffffff;
    t->flags = TIMER_FLAGS_USING;
    t->next = 0;

    timerctl.t0 = t;
    timerctl.next = 0xffffffff;
}

struct TIMER* timer_alloc() {
    int i;
    for (i = 0; i < MAX_TIMER; i++) {
        if (timerctl.timers0[i].flags == TIMER_FLAGS_UNUSE) {
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
            timerctl.timers0[i].flags2 = 0;
            return &timerctl.timers0[i];
        }
    }
    return 0;
}
```
然后我们就能看初始化PIT了，直接看三个对PIT的设置：
```C
    io_out8(PIT_CTRL, 0x34);
    // 设置中断周期为11932, 中断频率就是100hz，即1s钟发生100次中断, 10ms一次
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
```
对PIT的控制寄存器（PIT_CTRL）设定值为0x34（0011 0100），可以对照我们上边讲的控制寄存器，选择0号计数器，读写方式是先读写低字节，后读写高字节。使用第2中工作方式。使用二进制的数据表示。然后对0号计数器设定初始值，我们知道中断的工作频率是将1.19318MHz，1s可以将1193180减到0，这里设定的初始值是11932，那么大概11932/1193180=0.01s=10ms触发一次中断。11932的十六进制是0x2e9c，先设定低位，然后设定高位，这样就完成初始值的设定。<br/>
再然后我们回到init_pit中，初始化所有的定时器的状态为未使用，然后我们先分配一个定时器，设定他的超时时间为极大的值，也就是都不会超时，这个定时器作为第一个定时器。
定时器的分配（timer_alloc）其实没什么好讲的，就是从所有的定时器中选择一个还未使用的返回。
