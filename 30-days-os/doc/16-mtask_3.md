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
