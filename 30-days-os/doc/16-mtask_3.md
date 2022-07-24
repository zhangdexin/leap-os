## 多任务-3

### 序
上一章我们是讲解了作者针对多任务创建的数据结构，然后针对这个数据结构我们也讲解了关于多任务的一些操作，其中涉及到了任务的添加删除，设定优先级，level等操作。这一章我们主要来讲关于多任务是如何被调度起来的，及本书中是如何使用多任务的。

### 如何调度
讲到如何调度，我们还是要回归到源点，就是怎么触发让任务进行切换呢，答案是定时器。我们在上一章中task_init中有调用到
```c
// mtask.c
struct TASK* task_init(struct MEMMAN* memman) {
    // ...(略)
	task_timer = timer_alloc();
	timer_settime(task_timer, task->priority);
    // ...(略)
}
```
我们就跟着这个往下来梳理下，我们看到申请了一个task_timer，然后设定过期时间，priority也是作为当前task运行的时间，过了这个时间就会触发这个timer，那我们就跳转到timer过期函数这里来：
```c
// timer.c

void inthandler20(int *esp)
{
    // ...(略)

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

    // ...(略)

    if (ts != 0) {
        task_switch();
    }

	return;
}
```
这块代码我们之前讲过，不过没有讲关于task的部分，如果timer是task_timer，那么我们就会执行task_switch函数。也就是说这个任务执行时间用完了，该换下一个任务来执行了。
```c
// mtask.c

void task_switch() {
	struct TASKLEVEL* tl = &taskctl->level[taskctl->now_lv];
	struct TASK* new_task, *now_task = tl->tasks[tl->now];
	tl->now++;
	if (tl->now == tl->running) {
        tl->now = 0;
    }

	if (taskctl->lv_change != 0) {
		task_switchsub();
		tl = &taskctl->level[taskctl->now_lv];
	}

    new_task = tl->tasks[tl->now];
    timer_settime(task_timer, new_task->priority);
    if (new_task != now_task) {
        farjmp(0, new_task->sel);
    }
    return;
}
```
task_switch这个函数首先找到当前运行的level,及当前task。然后将now加1，也就是找下一个同level的task，然后判断是否需要切换level，到task_switchsub函数中看是否需要切换level，如果需要切换，就会得到最新的level对象task队列（tl），然后拿到下一个task赋值给new_task，然后根据这个最新的task的优先级继续设定定时器，然后通过farjmp来进行任务切换。<br>
然后我们再看下切换level的实现：
```c
// mtask.c

// 切换到某个level
void task_switchsub() {
	int i;
	for (i = 0; i < MAX_TASKLEVELS; i++) {
		if (taskctl->level[i].running > 0) {
			break;
		}
	}

	taskctl->now_lv = i;
	taskctl->lv_change = 0;
}
```
我们上一章有提到过，只有高层的level没有任务了，才会去执行下层level的task，我们这里即每次都是从0level向下遍历，如果其中有level中有task，我们就会设定到这个level，用now_lv来标识。<br>
然后我们看下farjmp函数来进行任务切换，farjmp必须要使用汇编来实现，我们去找到它的定义：
```c
// bootpack.h
void farjmp(int eip, int cs);  // 这里声明

// naskfunc.nas
; JMP FAR”指令的功能是执行far跳转。在JMP FAR指令中，可以指定一个内存地址，
; CPU会从指定的内存地址中读取4个字节的数据，
; 并将其存入EIP寄存器，再继续读取2个字节的数据，并将其存入CS寄存器
_farjmp:		; void farjmp(int eip, int cs);
		JMP		FAR	[ESP+4]				; eip, cs
		RET
```
根据我们第14章讲解的关于任务做切换的几种方式，作者这里使用便是JMP的形式，jmp到目标任务的tss的选择子。就实现了任务的切换，然后当前的任务的寄存器等信息同时也会被cpu保存在当前任务的对应的tss结构的指向的内存中。
> ‘JMP FAR’是win汇编的一个写法，主要是用在段间跳转。其他还有短跳转，偏移跳转，直接指定地址跳转，大家可以去学习一下，我们碰到也会讲

<br>
还有一个点我们要说，上边讲到了task的切换，我们也可以让task置于睡眠状态。这里的让task置于睡眠，也就是说把它切换出cpu，直到某一个时刻把他激活。

```c
// mtask.c
void task_sleep(struct TASK* task) {
	struct TASK *now_task;
	if (task->flags == 2) { /* 如果处于活动状态 */
		now_task = task_now();
		task_remove(task); /* 移除task */
		if (task == now_task) {
			/* 让自己休眠时，进行任务切换 */
			task_switchsub();
			now_task = task_now(); /* 获取当前任务 */
			farjmp(0, now_task->sel);
		}
	}
	return;
}
```
代码也不是很复杂，flag=2表示这个tasl在待运行的队列中（等待运行或者正在运行），flag=1表示task不在待运行队列，自然时间片到了切换任务也轮不到它，也就是我们所说的睡眠。flag=0表示task这块资源释放并回归到了taskctl中，处于未使用的状态了。<br>
然后我们看下代码，首先判断是否处于活动状态，因为如果不是处于活动状态，自然就是睡眠状态或者未使用状态，没必要处理了。移除掉指定的task，当前运行的如果是指定的task，那么就需要切换成别的task。因为我们有一个idle_task，所以待切换的task队列肯定有task存在。<br><br>
那么我们让一个睡眠之后，如何激活它呢，这个其实在前面我们有遇到了, 我们再来看一遍，task还是由中断触发的，
```c
// fifo.c

int fifo32_put(struct FIFO32 *fifo, int data)
/* 向FIFO发送数据保存 */
{
	// ...(略)
	if (fifo->task != 0) {
		if (fifo->task->flags != 2) { // 不处于活动状态
			task_run(fifo->task, -1, 0); // 唤醒任务，level和priority不变
		}
	}

	return 0;
}
```
我们上一章有说到task会和一个fifo队列绑定起来，fifo队列就是用来接收发向这个task的中断信息，put到fifo中，进而激活task。后边我们讲到输入输出的的中断，再来完整梳理一遍。现在我们只需要知道，如果键盘或者鼠标中断到来就会到task的fifo中。如果task处于睡眠状态，使用task_run让task重新运行。

### 系统中如何应用
之前所有的文章中我们也了解了如何切换task及相应的task的一些操作，同时我们也知道task_a就是我们的主task，并且我们也讲解了关于task_a的初始化。除此之外我们还讲述了idle_task，不过这个是放置在level层级的最下层，也就是没有其他任务才会运行它。那我们再次进行梳理下，然后带着大家进入console，也就是说控制台的程序。
```c
// bootpack.c
void HariMain(void)
{
    // ...(略)

	/* sht_cons */
	key_win = open_console(shtctl, memtotal);
    
    // ...(略)
}
```
这里我们创建了一个console，返回他的图层。我们再进入到open_console中看下
```c
struct SHEET *open_console(struct SHTCTL *shtctl, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHEET *sht = sheet_alloc(shtctl);
	unsigned char *buf = (unsigned char *) memman_alloc_4k(memman, 256 * 165);
	sheet_setbuf(sht, buf, 256, 165, -1); /* 无透明 */
	make_window8(buf, 256, 165, "console", 0);
	make_textbox8(sht, 8, 28, 240, 128, COL8_000000);
	sht->task = open_constask(sht, memtotal);
	sht->flags |= 0x20;	/* 设置窗口是否有光标 */
	return sht;
}
```
这里很多代码我们也能看懂，分配内存，分配图层，设定窗口字体及大小颜色等。和task相关的比较关键的是open_constask函数，sht->flags的设定0x20表示这个窗口需要闪烁的光标，之后会画上去，后边我们讲到console时还会继续讲。我们这里就先来看open_constask：
```c
struct TASK *open_constask(struct SHEET *sht, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = task_alloc();
	int *cons_fifo = (int *) memman_alloc_4k(memman, 128 * 4);
	task->cons_stack = memman_alloc_4k(memman, 64 * 1024);
	task->tss.esp = task->cons_stack + 64 * 1024 - 12;
	task->tss.eip = (int) &console_task;
	task->tss.es = 1 * 8;
	task->tss.cs = 2 * 8;
	task->tss.ss = 1 * 8;
	task->tss.ds = 1 * 8;
	task->tss.fs = 1 * 8;
	task->tss.gs = 1 * 8;
	*((int *) (task->tss.esp + 4)) = (int) sht;
	*((int *) (task->tss.esp + 8)) = memtotal;
	task_run(task, 2, 2); /* level=2, priority=2 */
	fifo32_init(&task->fifo, 128, cons_fifo, task);
	return task;
}
```
可以看到新创建了一个task，分配内存，设定寄存器，代码段设定为2，其他设定为1。我们系统也只有这两个段（比较简单）。要关注的一点是，每个task使用栈地址是不一样的，包括我们前边的idle_task和这个新建的constask，使用esp来指向栈顶。我们这里知到task分配了64 * 1024的大小的内存作为栈，esp指向栈顶，然后-12表示栈顶指针移动12字节，这里是向栈中push了sht和memtotal变量：
```	
    *((int *) (task->tss.esp + 4)) = (int) sht;
	*((int *) (task->tss.esp + 8)) = memtotal;
```
然后设定了优先级和level，level是在第二层。最后把task放置到队列，等待运行。<br><br>

这个时候我们其实有三个任务在待运行的队列中，主程序的，console和一个idle的任务，我们回到bootpack.c的文件中，基本上初始化的事情我们讲完了，接下来会进入一个大的循环，这个循环就是我们操作系统主程序最后的部分了，也就是说主程序一直处于这个循环中，等待或者处理中断发来的事件。我们来看一下：
```c
// bootpack.c
void HariMain(void)
{
    // 略(...)
    for (;;) {
        // 略(...)
        io_cli();
        if (fifo32_status(&fifo) == 0) {
            // fifo为空时，存在搁置的绘图操作立即实行
			if (new_mx >= 0) {
				io_sti();
				sheet_slide(sht_mouse, new_mx, new_my);
				new_mx = -1;
			}
			else if (new_wx != 0x7fffffff) {
				io_sti();
				sheet_slide(sht, new_wx, new_wy);
			}
			else {
				task_sleep(task_a);
				io_sti();
			}
        }
        else {
            i = fifo32_get(&fifo);
			io_sti();

            // 略(...)
            if (256 <= i && i <= 511) {} // 键盘数据
            else if (512 <= i && i <= 767) {} // 鼠标数据
            else if (768 < i && i <= 1023) {} // 命令行窗口关闭
            else if (1024 < i && i <= 2023) {} // 只关闭task
            else if (2024 < i && i <= 2279) {} // 只关闭命令行窗口
        }
    }
}
```
我们先来看下这个循环主要做的事情是什么，其实就是从主任务的队列（fifo）中取出来数据去处理，这个队列主要就是存放中断数据等。我们看他可能会处理键盘数据，鼠标数据，命令行窗口等等一些数据。这里我们留到后边再讲，我们主要看从没有数据到有数据这块和任务相关的内容。进入到主循环，获取消息之前需要先关闭中断，也是避免在操作fifo时发生冲突，然后判断fifo中是否有数据，如果有数据取出数据，恢复中断，然后去处理。<br>
没有数据我们看如果不做其他事情要讲主任务睡眠```task_sleep(task_a);```，然后睡醒之后在恢复中断。这里我搞了很久才明白，首先把恢复中断放在睡眠之后其实可以理解，如果放在之前万一在调用睡眠的时候中断进来就不能及时处理中断。放在后边等到中断进来可以激活任务来处理。第二个点不好理解，就是既然睡眠了，且是在禁用中断的情况下睡眠，那不就是接收不到中断了？答案是这样的，睡眠其实是只切换到别的task，暂时让这个task不在运行队列中，那么切换其他的task会做什么呢，就是会把其相应的寄存器重新加载，这里就包括eflags寄存器，其他的task并没有禁用中断，也还是可以接收到中断的。

### 总结
多任务就大概讲到这里了，本章讲解了如何调度多任务，同时使用比较简单的代码中的例子来阐述，后边还有很多和多任务相关的操作，不过我们有了这个基础可以很容易看懂其他的代码。多任务这块的东西还是比较多，从最开始的tss，idt结构等。到后边我们定义的task的数据结构及管理方式方法，到这一章的如何调度。这里多任务也比较简单，仅仅也只是使用优先级和层级的属性来作为调度的依据。现代操作系统可能要考虑更多的场景。