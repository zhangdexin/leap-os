## 图层管理

### 序
这一章我们来讲关于图层管理的，因为我们要在显卡中画图像，其中包含了窗口，屏幕，鼠标，文字等等一些图像，这个时候我们移动鼠标，移动窗口，激活窗口，窗口与窗口之间又会覆盖等等很复杂的表现形式。我们这个时候就需要图层来管理各个不同的图像，这样我们在图像的覆盖就会抽象到不同图层的上下层关系，图像的移动就表现位图层的移动。这里我们令鼠标单独一个图层，屏幕或者说桌面单独一个图层，每个窗口都是一个独立的图层。那我们就能知道桌面图层永远是在最下层，鼠标图层永远是在最上层，其他的都是介于这两者之间。

### 初始化图层管理器
我们还是要回到bootpack.c中来看首先初始化图层：
```c
// bootpack.c
void HariMain(void)
{
    struct SHTCTL *shtctl;
    // ...(略)
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
    // ...(略)
}

struct SHTCTL* shtctl_init(struct MEMMAN* memman, unsigned char* vram, int xsize, int ysize) {
    // ...(略)
}
```
我们先来看这个函数形式，传入的参数是内存管理器的对象，看样子是要在里边分配内存，显存的地址及屏幕的分辨率传递进去，返回值是SHTCTL结构体对象指针，那我们先看下这个SHTCTL是个什么样子：
```c
// bootpack.h
/* sheet.c 图层管理*/
#define MAX_SHEETS  256

// 透明图层
struct SHEET {
	unsigned char* buf; // 描述的内容地址
	int bxsize, bysize, // 长宽
	vx0, vy0,           // 位置坐标，v(VRAM)
	col_inv,            // 透明色色号
	height,             // 图层高度(指所在的图层数吧)
	flags;              // 设定信息, 0x20表示图层是否需要光标，0x10表示窗口是否是应用程序, 0x01表示是否自动关闭
	struct SHTCTL* ctl;
	struct TASK* task;  // 所属的task
};

// 图层管理
struct SHTCTL {
	unsigned char* vram, *map; // vram、xsize、ysize代表VRAM的地址和画面的大小, map表示画面上每个点是哪个图层
	int xsize, ysize, top; // top代表最上层图层的高度
	struct SHEET* sheets[MAX_SHEETS]; // 有序存放要显示的图层地址
	struct SHEET sheets0[MAX_SHEETS]; // 图层信息
};
```
SHTCTL就是图层控制器，SHEET就是图层，这里设置最大可以管理MAX_SHEETS（256）个图层。
注释比较清晰了，另外要说明的是SHTCTL中使用sheets0存放每一个SHEET，sheets按照顺序存放指向每个SHEET的指针。SHEET中的buf存放的是要写入到显存的图像数据。

然后就可以继续看下shtctl_init函数中的内容了
```c
struct SHTCTL* shtctl_init(struct MEMMAN* memman, unsigned char* vram, int xsize, int ysize) {
	struct SHTCTL* ctl;
	int i;
	
	// 分配图层管理的内存空间
	ctl = (struct SHTCTL*) memman_alloc_4k(memman, sizeof(struct SHTCTL));
	if (ctl == 0) {
		return ctl;
	}
	
	// 开辟和vram一样大小的内存
	ctl->map = (unsigned char *)memman_alloc_4k(memman, xsize * ysize);
	if (ctl->map == 0) {
		memman_free_4k(memman, (int)ctl, sizeof(struct SHTCTL));
		return 0;
	}
	
	
	ctl->vram = vram;
	ctl->xsize = xsize;
	ctl->ysize = ysize;
	ctl->top = -1; // 一个sheet也没有
	for (i = 0; i < MAX_SHEETS; i++) {
		ctl->sheets0[i].flags = 0; // 标记为未使用
		ctl->sheets0[i].ctl = ctl; // 记录所属的ctl
	}
	
	return ctl;
}
```
首先为ctl分配空间，为map开辟和vram一样大小的内存，因为他们存放的都是每个像素点的信息，map后边用到详细讲。初始化vram，xsize，yszie及top值。标记其中所有的图层都处于未使用阶段，并关联ctl。
内容看起来比较简单，仅仅是初始化了一下图层管理器。

### 图层的设置
以鼠标和桌面画面为例，我们看下首先如何针对的图层进行设置：
```c
// bootpack.c
void HariMain(void)
{
    // ...(略)
    struct SHEET* sht_back, *sht_mouse;
	unsigned char* buf_back, buf_mouse[256];

    // ...(略)
    // background
    sht_back = sheet_alloc(shtctl);
	buf_back = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1); //没有透明色
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);

    // ...(略)

    // sht_mouse
	sht_mouse = sheet_alloc(shtctl);
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); // 透明色号99
	init_mouse_cursor8(buf_mouse, 99);

    // ...(略)
}
```
首先声明桌面和鼠标的图层sht_back和sht_mouse，以及存放他们图像的缓冲区buf_back和buf_mouse。
我们可以看到首先通过shtctl分配一个图层（sheet_alloc）出来，然后分配缓冲区的空间（buf_mouse声明时就有无须分配空间），空间大小就是要存放图像的内容大小，这里sht_back的大小就是整个屏幕分辨率的长宽相乘，同时上一章我们知道鼠标图像的大小是16\*16。sheet_setbuf是刚刚分配出来的图层和buf，长宽等属性绑定起来。再之后init_screen8和init_mouse_cursor8就是向缓冲区里边画图像了，这个我们上一章也讲过了。

继续看下sheet_alloc和sheet_setbuf：

```c
// sheet.c
struct SHEET* sheet_alloc(struct SHTCTL* ctl) {
	struct SHEET* sht;
	int i;
	for (i = 0; i < MAX_SHEETS; i++) {
		if (ctl->sheets0[i].flags == 0) {
			sht = &ctl->sheets0[i];
			sht->flags = 1; // 标记为正在使用
			sht->height = -1; // 隐藏
			sht->task = 0;
			return sht;
		}
	}
	
	return 0; // 所有的SHEET都处于使用状态
}

// 设置sheet的变量
void sheet_setbuf(struct SHEET* sht, unsigned char* buf, int xsize, int ysize, int col_inv) {
	sht->buf = buf;
	sht->bxsize = xsize;
	sht->bysize = ysize;
	sht->col_inv = col_inv;
}
```
代码比较简单，sheet_alloc就是从SHTCTL的sheets0中找一个未被使用的图层拿出来，标记为使用返回给调用者，sheet_setbuf仅仅是将buf，长宽，透明颜色赋值给图层。

### 图层的管理