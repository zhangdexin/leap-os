## 多任务-2

### 序
上一章我们讲解了关于多任务切换等一些知识概念，这一章我们来通过作者实现的代码来讲解一下。

### 数据结构
首先我们来看下代码结构：
```c
// bootpach.h
#define MAX_TASKS  1000   // 最大任务数量
#define TASK_GDT0  3      // 定义从GDT的几号
#define MAX_TASKS_LV 100  // 一个level的task个数
#define MAX_TASKLEVELS 10 // 最多可以有多少个level

// 任务状态段（task status segment）
struct TSS32 {
	int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3; // 任务设置相关
	int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi; // 32位寄存器
	int es, cs, ss, ds, fs, gs; // 16位寄存器
	int ldtr, iomap; // 任务设置部分
};

struct TASK {
	int sel, flags; /* sel用来存放GDT的编号 */
	int level, priority; // priority 优先级
	struct FIFO32 fifo;
	struct TSS32 tss;
	struct SEGMENT_DESCRIPTOR ldt[2];
	struct CONSOLE* cons; // 所属的命令行
	int ds_base; // 程序的数据段地址的base值，因为目前一个命令行窗口只能运行一个程序
	int cons_stack; // 命令行程序运行时栈空间
	struct FILEHANDLE* fhandle; //文件相关操作
	int* fat;  // fat
	char* cmdline;
	unsigned char langmode, langbyte1;
};

// TASKLEVEL表示任务运行的层级，优先级上更加细粒度的划分，0优先级最高，处于level0的任务最先运行
struct TASKLEVEL {
	int running; /* 正在运行的任务数量 */
	int now;     /* 当前运行的时哪个任务 */
	struct TASK *tasks[MAX_TASKS_LV];
};

struct TASKCTL {
	int now_lv; // 现在活动中的level
	char lv_change; // 下次任务切换时是否需要改变level
	struct TASKLEVEL level[MAX_TASKLEVELS];
	struct TASK tasks0[MAX_TASKS];
};

```
一些信息我们可以从注释中得出来，我们还是要说一些关键的信息。<br>
最开始是我们定义了TSS32（任务状态段的数据结构），你可以拿这个结构和上一节我们讲到的做一下对比 <br>
接着我们的多任务也还是需要管理者，充当管理者角色的是TASKCTL，这个代码风格和我们之前的很像。<br>
我们看下TASKCTL里边的内容，首先有一个level的概念，他其实是在各个task的优先级上又进行了一次划分，相同优先级的task属于同一个level，操作系统优先调用上层（level越小）的任务，然后依次向下。划分了MAX_TASKS_LV=10个level，每个level中是最多100个task。<br>
[图]
然后tasks0就是存放所有task的结构了。<br>
往下我们看TASKLEVEL的结构，因为操作系统设定是一层一层的运行，也即如果level0中有任务就不会运行level1中的任务，除非level0中任务全部休眠或者降级到下一级。结构中的tasks表示属于这个level中的任务。<br>
然后我们看下复杂的TASK中的结构，这里涉及到了很多的字段，我们后边也会提到。我们先知道sel是该task下的TSS存放在GDT中的选择子。然后还有level和优先级，优先级我们下边会讲到如何使用。绑定到一个任务的fifo数据队列，如何使用后边也会讲到。然后是声明了两个元素的ldt，这个其实就是这个任务存放ldt表的位置，LDT的段描述符就是指向这里。我们也把他和task绑定在了一起，后边一些我们用到再说。大家也可以借助注释先理解下。

### 多任务的操作
我们先看下bootpack.c中，关于任务的初始化：
```c
// bootpack.c

void HariMain(void)
{
	// ...(略)

	task_a = task_init(memman);
	fifo.task = task_a;
	task_run(task_a, 1, 0);
	*((int *) 0x0fe4) = (int) shtctl;
	*((int *) 0x0fec) = (int) &fifo; 
	task_a->langmode = 0;

	// ...(略)
}
```
其实我们代码运行到这里的时候还没有task这个概念，这些代码都是让BIOS交由操作系统一股脑运行到这里，我们这里的task_a就是当前运行的任务，是通过task_init这个函数来初始化的。
task_init这个函数里边做了一些初始化TASKCTL等等一系列操作，设置GDT段描述符等，然后初始化第一个task作为当前的task，进行了一些赋值后返回回来，具体看一下：
```c
struct TASK* task_init(struct MEMMAN* memman) {
    int i;
	struct TASK *task, *idle;
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
	taskctl = (struct TASKCTL *) memman_alloc_4k(memman, sizeof (struct TASKCTL));
	for (i = 0; i < MAX_TASKS; i++) {
		taskctl->tasks0[i].flags = 0;
		taskctl->tasks0[i].sel = (TASK_GDT0 + i) * 8;
		taskctl->tasks0[i].tss.ldtr = (TASK_GDT0 + MAX_TASKS + i) * 8;
		set_segmdesc(gdt + TASK_GDT0 + i, 103, (int) &taskctl->tasks0[i].tss, AR_TSS32);
		set_segmdesc(gdt + TASK_GDT0 + MAX_TASKS + i, 15, (int) taskctl->tasks0[i].ldt, AR_LDT);
	}

	for (i = 0; i < MAX_TASKLEVELS; i++) {
		taskctl->level[i].running = 0;
		taskctl->level[i].now = 0;
	}

	task = task_alloc();
	task->flags = 2;    /* 2:活动中 */
	task->priority = 2; // 0.02s
	task->level = 0;
	task_add(task);
	task_switchsub();
	load_tr(task->sel);
	task_timer = timer_alloc();
	timer_settime(task_timer, task->priority);

	idle = task_alloc();
	idle->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024;
	idle->tss.eip = (int) &task_idle;
	idle->tss.es = 1 * 8;
	idle->tss.cs = 2 * 8;
	idle->tss.ss = 1 * 8;
	idle->tss.ds = 1 * 8;
	idle->tss.fs = 1 * 8;
	idle->tss.gs = 1 * 8;
	task_run(idle, MAX_TASKLEVELS - 1, 1);
	return task;
}
```
代码也是比较多，这个函数中会创建两个task，一个就是我们所说的主task，另外一个是idle,这个task什么事情也不做就是休眠。<br>
我们继续看代码，首先拿到gdt，分配taskctl的内存中间，然后初始化每一个task，flags为0，sel从TASK_GDT0开始，然后这个任务的ldt的段描述符是放在所有的task的tss的段描述符的后边，然后调用两次set_segmdesc，分别设定tss的段描述符和ldt的段描述符。tss的段描述指向这个任务的tss的结构处，AR_TSS32是0x0089，大家可以去第五章去找下拆解right等等操作，除此之外0x0089能够唯一表示出这个段描述符是TSS，我们上一章也讲到S位为0，type为1001表示是TSS段。ldt段描述符的设定指向我们上边说到的task中的ldt数组，同理AR_LDT也可以唯一表示这个段描述符是LDT的描述符。<br>
然后代码是初始化level这个数组。<br>
然后调用task_alloc来分配了一个task，task_alloc中无非也是做一些task的初始化的状态，分配一个还未用到的task。task->flags默认是0，1表示使用中（包括休眠）但是还未运行，2表示正在运行。那么这里设置为2其实就是我们要返回的task。priority优先级设定为2。我们之前有讲到timer的触发，其中有一个timer时task_timer，这个timer表示每个一段时间就要切换一次任务，避免一个任务长时间的占用cpu。这个优先级就是设定的时间间隔，我们时间间隔的单位时10ms，这里不清楚的可以去看11章，2表示20ms后切换到别的task，也就是说这个task可以在cpu上运行20ms。那么就是设定的priority越大，运行的时间越长，表示优先级越高。这个task的level设定是0档，也就是最先运行的那一档。task_add表示放置在待运行的队列中。task_switchsub表示切换到某个level，我们最开始就是切换到level0。然后load_tr，加载TR寄存器为这个task的TSS的段描述符。创建了task_timer，设定下一次切换的时间是这个task的priority*10ms之后。先停一停往下看，有几个点要看下，其中我们task_alloc仅仅是初始化了tss，但是为什么没有给寄存器赋值？这里有一个load_tr的调用，我们说过通过jmp语句，cpu会自动去load tr寄存器，这里为什么会调用。这两个问题的原因其实答案是一个，那就是我们现在要用一个task表示当前运行的这个任务，我们既然已经运行到这里了，前边的寄存器肯定也已经设定好了，所以也不需要为tss设定寄存器的值了。那么在切换的时候cpu会将当前的task的寄存器的值写入到tss中，这里也就是说切换下一个任务的时候，目前的这个task自然就有值了。不过得有一个前提就是cpu得能够找到旧的这个task的tss，这也就是我们需要手动去load_tr的原因了，其实做load_tr和无需赋tss的值都是因为当前的任务是最特殊的，是从通电运行到这里才需要引入task。