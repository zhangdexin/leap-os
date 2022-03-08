## 一个简单的队列
作者自己写了一个队列，很多地方用到了它，感觉有点绕不过，花一个章节讲一下吧，代码比较简单，篇幅也比较小

### 结构
```C
// bootpack.h
struct FIFO32 {
	int *buf;
	int p, q, size, free, flags;
	struct TASK* task;
};

void fifo32_init(struct FIFO32 *fifo, int size, int *buf, struct TASK* task);
int fifo32_put(struct FIFO32 *fifo, int data);
int fifo32_get(struct FIFO32 *fifo);
int fifo32_status(struct FIFO32 *fifo);
```
结构体和相关的函数声明我列出来了，buf是指向存放数据的内存的指针，p表示下一个数据去写的位置索引，q表示下一个数据读取位置索引。free表示还有多少空余空间，flags就是一个标志位这里仅仅表示是否内存用完了。task这个是说这个队列是给哪个任务用的，taks就涉及到了多任务了，大家也可以理解成是个进程，等后边讲到task大家就明白了。

### 初始化
```C
// fifo.c
void fifo32_init(struct FIFO32 *fifo, int size, int *buf, struct TASK* task)
/* FIFO初期化 */
{
	fifo->size = size;
	fifo->buf = buf;
	fifo->free = size; /* 缓冲区大小 */
	fifo->flags = 0;
	fifo->p = 0; /* 下一个数据写去位置 */
	fifo->q = 0; /* 下一个数据读取位置 */
	fifo->task = task; // 有数据写入需要唤醒的任务
	return;
}
```
队列对象的初始化，从外部传入FIFO32结构体对象及size，存放数据的内存，以及相关的任务task。我们看下我们代码中初始化代码：
```C
// bootpack.c

void HariMain(void)
{
    struct FIFO32 fifo, keycmd;
    int fifobuf[128], keycmd_buf[32];
    // ...(略)

    fifo32_init(&fifo, 128, fifobuf, 0);
	fifo32_init(&keycmd, 32, keycmd_buf, 0);
}
```
这里看到是在栈上分配了队列需要的内存及结构体对象，然后将其的数据初始化。task都是0，这里0表示就是操作系统内核。

### 数据存取
```C
int fifo32_put(struct FIFO32 *fifo, int data)
/* 向FIFO发送数据保存 */
{
	if (fifo->free == 0) {
		/* 溢出 */
		fifo->flags |= FLAGS_OVERRUN;
		return -1;
	}
	fifo->buf[fifo->p] = data;
	fifo->p++;
	if (fifo->p == fifo->size) {
		fifo->p = 0;
	}
	fifo->free--;
	if (fifo->task != 0) {
		if (fifo->task->flags != 2) { // 不处于活动状态
			task_run(fifo->task, -1, 0); // 唤醒任务，level和priority不变
		}
	}

	return 0;
}
```
先来看put函数，空余空间没有了，直接返回-1。如果还有空间的花就加入数据，如果p和size相等就到0索引处，这个意思是维护一个循环的队列。最后查看下task是不是处于活动状态，不是活动中就唤醒这个任务。后边讲到多任务时我们相信讨论。

```C
int fifo32_get(struct FIFO32 *fifo)
/* FIFO读取数据 */
{
	int data;
	if (fifo->free == fifo->size) {
		/* 缓冲区是空 */
		return -1;
	}
	data = fifo->buf[fifo->q];
	fifo->q++;
	if (fifo->q == fifo->size) {
		fifo->q = 0;
	}
	fifo->free++;
	return data;
}
```
然后就是取数据，先判断队列是否为空，不为空的话返回q指向的数据，变更free，然后返回这个数据

### 总结
总的来说这个结构非常简单，我们简单梳理一下这个结构，无非就是一个环形队列的put和get操作，这里要关注的就是给每个队列都绑定了task，也即每个队列的对象只能服务于一个任务。
