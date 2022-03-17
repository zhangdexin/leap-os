## 定时器
### 序
这一章节我们来讲定时器，关于定时器，我们从前边学习中断的时候也知道，定时器也是通过硬件来触发中断，然后通过PIC发送给CPU中断信号。定时器也即每隔一段时间就会发送信号给CPU，那么如果让它发来信号呢，又如何设置多久发来一次信号呢，那就是我们也还是对定时器的硬件进行设置，对于CPU来说，只需要知道是设置PIT就好了，PIT（Programmable Interval Timer）可编程的间隔型定时器，电脑中PIT连接着PIC的IRQ0号，PIT发出中断信号给IRQ0，然后PIC就会发送中断向量号给CPU，进一步执行中断处理程序。

### 设置PIT
#### 关于PIT知识储备
我们也简单的来讲下PIT的内部结构吧，PIT内部根据功能的划分有三个计数器，对应端口分别是0x40~0x42，计数器0用于产生实时时钟信号。计数器1专用于DRAM的定时刷新。计数器2专用于扬声器发出声音。所以我们这里使用的就是计数器0。
然后PIT内部有一个控制字寄存器，用来对使用哪一个计时器及如何使用计时器进行控制的一些设定。如图所示：

<image src="https://user-images.githubusercontent.com/22785392/158379662-9377c9a3-bbd3-4b09-878f-122da2bf3876.png" width="40%" height="40%" />

* SC1~SC0就是选择哪一个计数器来使用
* RW1~RW0表示使用的计数器读/写/锁存的方式，00表示锁存数据仅供CPU读，01表示只读写低字节，10表示只读写高字节，11表示先读写低字节，后读写高字节
* M2~M0用来指定工作的方式，有6种方式，不同方式表示计数器的不同技术过程，启动停止方式，我们这里使用的方式2，这种方式我们仅仅关注他会循环计数，即当计数器到达设定的初始值，计数器会重新开始计数，循环触发。
* 最后一位BCD表示使用是否使用十进制，1表示十进制，0表示使用二进制。

给计数器设定以初始值，计数器将初始值每次减1，当减到0时，就会向CPU发出中断信号。计数器的工作频率是1.19318MHz，也即一秒钟可以进行1.19318M次计算，那就是一秒钟可以将1193180减到0。计数器钟的用来存放初始值寄存器是16位（0~65535），默认是全0，表示的是65536。那我们就以初值寄存器设定为0，也即初值为65536为例来算一下。计数器1秒钟可以把1193180减为0。那么初值为65536时，将65536减为0需要的时间是65536/1193180=0.05492s，也就是0.05492s发出一次中断信号，触发一次中断。那你如果不去设定初值，定时器就会0.05492s触发一次中断。

#### 如何设定PIT
有了上边简单的知识了解，那我们就可以对PIT进行设定，然后就可以触发中断了<br/>
首先我们要对PIT的控制寄存器设定，端口是0x43<br/>
然后对计数器进行初始值设定，端口是0x40（这里只使用计数器0）<br/>
这里都是使用out的指令就可以<br/>

### 初始化PIT
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

### 设定定时器
上一小节我们讲了关于PIT的设置，以及定时器的初始化操作。然后我们就开始设置定时器了。
```C
void timer_init(struct TIMER* timer, struct FIFO32* fifo, int data) {
    timer->fifo = fifo;
    timer->data = data;
}

void timer_settime(struct TIMER* timer, unsigned int timeout) {
    int e;
    struct TIMER *t, *s;

    timer->timeout = timeout + timerctl.count;
    timer->flags = TIMER_FLAGS_USING;

    e = io_load_eflags(); // 记录中断许可标志
    io_cli(); // 禁止中断

    t = timerctl.t0; // head(可能是哨兵或者第一个真实元素)

    /* 构建链表过程 */
    // 插入这个比第一个还小，替换第一个
    if (timer->timeout <= t->timeout) {
        timerctl.t0 = timer;
        timer->next = t;
        timerctl.next = timer->timeout;

        io_store_eflags(e);
        return;
    }

    // 其他情况, 遍历链表找到合适位置，链表结尾是哨兵（timeout=oxffffffff）
    for (;;) {
        s = t;
        t = t->next;

        if (timer->timeout <= t->timeout) {
            s->next = timer;
            timer->next = t;

            io_store_eflags(e);
            return;
        }
    }
}
```
首先是timer_init函数，为相应的timer设定他的队列及数据。<br/>
然后就是timer_settime函数，这个就会给这个timer设定时间，并且加入到timerctl控制的定时器列表中。```timer->timeout = timeout + timerctl.count;```这句不是很好理解，我们说timerctl.count是中断触发的次数，我们前边设定每10ms触发一次中断，那么timerctl.count*10ms就是从设定PIT开始到目前的时间。那么传进来参数timeout是时间间隔，timer->timeout就是定时器到达触发时间的时间点。其余就是设置timer的标志等等。再然后就要插入到timerctl控制的定时器列表中，我们前边也说到这个列表是一个排好序的定时器序列，就是按照触发时间点的先后来排序。如果插入的是第一个会更新timerctl.next即最近触发定时器的时间点<br/>
我们也看到会用到禁用中断的语句来保证避免在设定的过程中，有中断进来，然后访问timerctl控制的定时器列表发生的混乱。所以在设定之前需要禁用中断：io_cli()。执行完之后恢复中断，这里使用恢复中断的另一种方法。我们在中断的章节讲到，io_cli执行也是对eflags寄存器的IF位设定，这里```e = io_load_eflags();```是先把eflags寄存器的值保存下来，等到执行完之后恢复```io_store_eflags(e);```，其实这里可能也并不知道目前是不是中断开启，所以就是使用io_store_eflags恢复原来状态就好。<br/>
然后我们继续看下如何调用两个函数来设定定时器，后边我们就会用到，这里举例来说明：
```C
struct TIMER* timer = timer_alloc();
timer_init(timer, &fifo, 1);
timer_settime(timer, 50);
```
其实就是这样使用，首先为timer分配一个空间出来，这里我们只需要在TIMERCTL.timers0找一个还未使用的timer复制给它，然后为这个timer绑定一个队列，设定一个值。然后设定时间加入到定时器队列中去。这里设定的50其实是50*10ms=500ms的时间间隔，也就是500ms后触发定时器。

> 这里还是要说一下我们在init_pit时创建的一个超时时间(0xffffffff)极大的timer，主要作用是一个哨兵的工作，不做实际的timer使用，它作用就是方便我们在timer设定时间后很方便的插入到列表中，只需要判断超时时间大小就可以判断，哨兵timer总是在最后一个。算是作者的一个小的tips.

### 定时器触发
定时器触发还得从中断处理函数说起，我们在中断的章节有说到时钟中断对应IRQ0，同时我们也设定了PIC的初始中断号是0x20，那么时钟中断发给CPU的中断向量号就是0x20+IRQ的号即为0x20，我们也在中断的那个章节看到0x20向量号对应asm_inthandler20中断处理程序，asm_inthandler20然后会调用inthandler20函数，然后我们就到inthandler20函数中来：
```C
// 0x60+IRQ号码输出给OCW2表示某个IRQ处理完成
void inthandler20(int *esp)
{
    struct TIMER* timer;
    char ts = 0;

	io_out8(PIC0_OCW2, 0x60);	/* 通知PIC"IRQ-00"已经受理完成 */
    timerctl.count++;
    if (timerctl.next > timerctl.count) { // 判断下一个时刻是否超时
        return ; 
    }

    // 查看有多少个定时器超时
    timer = timerctl.t0;
    for (;;) {
        if (timer->timeout > timerctl.count) {
            break;
        }
        
        timer->flags = TIMER_FLAGS_ALLOC;
        if (timer != task_timer) {
            fifo32_put(timer->fifo, timer->data);
        }
        else {
            ts = 1; // task_timer超时
        }
        timer = timer->next;
    }

    // 有i个定时器超时了, 其余进行移位
    timerctl.t0 = timer;
    timerctl.next = timerctl.t0->timeout;

    if (ts != 0) {
        task_switch();
    }

	return;
}
```
每10ms进入一次中断处理函数，首先写回给PIT表示中断接收完成。累加timerctl.count，判断timerctl.next（最近的一个定时器）是否触发，这个next的作用是可以提高效率，如果第一个都没有触发，那后边的肯定都没有触发。<br/>
然后for循环，如果timer超时且不是task_timer就放到相应的队列中，后边会有程序处理。如果是task_timer超时则是证明是要切换task了，继续遍历下一个timer。这里要说一下task_timer，这个timer专门用来控制task的切换，这里明白task_timer的时间触发就会切换任务。<br/>
最后会把已经触发的timer移除，其他的timer向前移动。如果有task_timer触发，就执行task_switch进行任务切换。

### 定时器释放和取消
```C
void timer_free(struct TIMER* timer) {
    timer->flags = TIMER_FLAGS_UNUSE;
}
```
首先看定时器的free函数，因为我们的timer是已经分配好的，只是在用的时候返回，所以free只需要将标志位置为未使用即可。以便空间后边返回给别的使用timer使用。

```C
int timer_cancel(struct TIMER *timer)
{
	int e;
	struct TIMER *t;
	e = io_load_eflags();
	io_cli();	/* 设置过程中禁止改变定制器状态 */
	if (timer->flags == TIMER_FLAGS_USING) {	/* 是否要取消 */
		if (timer == timerctl.t0) {
			/* 取消第一个 */
			t = timer->next;
			timerctl.t0 = t;
			timerctl.next = t->timeout;
		} else {
			/* 非第一个定时器取消 */
			/* 找到timer前一个定时器 */
			t = timerctl.t0;
			for (;;) {
				if (t->next == timer) {
					break;
				}
				t = t->next;
			}
			t->next = timer->next; /* 删掉链表中的这个定时器 */
		}
		timer->flags = TIMER_FLAGS_ALLOC;
		io_store_eflags(e);
		return 1;	/* 取消成功 */
	}

	io_store_eflags(e);
	return 0; /* 不需要处理 */
}
```
取消一个timer，比较简单，就是从timer列表中删除指定的timer，将状态置为TIMER_FLAGS_ALLOC，该状态是出于未使用和使用中之间的一个状态，这个状态主要方便对该timer重新设置时间加入到列表中，如果真的不使用了，可以使用timer_free释放。

```C
void timer_cancelall(struct FIFO32 *fifo)
{
	int e, i;
	struct TIMER *t;
	e = io_load_eflags();
	io_cli();	/* 设置过程中禁止改变定时器状态 */
	for (i = 0; i < MAX_TIMER; i++) {
		t = &timerctl.timers0[i];
		if (t->flags != 0 && t->flags2 != 0 && t->fifo == fifo) {
			timer_cancel(t);
			timer_free(t);
		}
	}
	io_store_eflags(e);
	return;
}
```
取消绑定在指定队列的所有timer*

### 总结
本章我们整体上熟悉了针对定时器的触发及设定，最开始我们设定PIT，让时钟中断没10ms进行一次中断，有了这个我们就可以设定定时器了，这里我们设置了很多的定时器，不同定时器作用不一样，给他绑定了一个队列，触发时就会加入到相应队列等待后续的处理。同时还有一些针对定时器的设定函数，设置时间，分配定时器，取消等等。
