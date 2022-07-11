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

### 系统中如何应用