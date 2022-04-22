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
#### 图层绘制
图层的各个管理操作包括，图层的移动，层级上的向上或者向下，如何显示等。
先来看下如何绘制出来吧，所有的操作也都是基于此来实现。
```c
// sheet.c

void sheet_refresh(struct SHEET* sht, int bx0, int by0, int bx1, int by1) {
 if (sht->height >= 0) { // 判断是否在显示
  sheet_refreshsub(sht->ctl, sht->vx0 + bx0, sht->vy0 + by0, sht->vx0 + bx1, sht->vy0 + by1, sht->height, sht->height);
 }
}
```
这个就是绘制指定一个图层中的一部分区域画出来，该图层中的区域为(bx0,by0)左上角到(bx1,by1)右下角组成的矩形区域。另外height表示的该图层所在的层数，小于0表示处于隐藏的状态。
再详细看调用sheet_refreshsub函数来刷新这部分区域：
```c
// sheet.c

void sheet_refreshsub(struct SHTCTL* ctl, int vx0, int vy0, int vx1, int vy1, int h0, int h1) {
	int h, bx, by, vx, vy, bx0, by0, bx1, by1, bx2, sid4, i, i1, *p, *q, *r;
	unsigned char *buf, *vram = ctl->vram, *map = ctl->map, sid;
	struct SHEET *sht;

	if (vx0 < 0) { vx0 = 0; }
	if (vy0 < 0) { vy0 = 0; }
	if (vx1 > ctl->xsize) { vx1 = ctl->xsize; }
	if (vy1 > ctl->ysize) { vy1 = ctl->ysize; }

	for (h = h0; h <= h1; h++) {
		sht = ctl->sheets[h];
		buf = sht->buf;
		sid = sht - ctl->sheets0;
		
		bx0 = vx0 - sht->vx0;
		by0 = vy0 - sht->vy0;
		bx1 = vx1 - sht->vx0;
		by1 = vy1 - sht->vy0;
		if (bx0 < 0) { bx0 = 0; }
		if (by0 < 0) { by0 = 0; }
		if (bx1 > sht->bxsize) { bx1 = sht->bxsize; }
		if (by1 > sht->bysize) { by1 = sht->bysize; }
		if ((sht->vx0 & 3) == 0) {
			/* 4字节 */
			i  = (bx0 + 3) / 4; /* bx0除以4(小数进位) */
			i1 =  bx1      / 4; /* bx1除以4(小数舍去) */
			i1 = i1 - i;
			sid4 = sid | sid << 8 | sid << 16 | sid << 24;
			for (by = by0; by < by1; by++) {
				vy = sht->vy0 + by;
				for (bx = bx0; bx < bx1 && (bx & 3) != 0; bx++) {	/* 前面被4除多余的部分逐字节写入 */
					vx = sht->vx0 + bx;
					if (map[vy * ctl->xsize + vx] == sid) {
						vram[vy * ctl->xsize + vx] = buf[by * sht->bxsize + bx];
					}
				}
				vx = sht->vx0 + bx;
				p = (int *) &map[vy * ctl->xsize + vx];
				q = (int *) &vram[vy * ctl->xsize + vx];
				r = (int *) &buf[by * sht->bxsize + bx];
				for (i = 0; i < i1; i++) {							/* 4倍数部分 */
					if (p[i] == sid4) {
						q[i] = r[i];
					} else {
						bx2 = bx + i * 4;
						vx = sht->vx0 + bx2;
						if (map[vy * ctl->xsize + vx + 0] == sid) {
							vram[vy * ctl->xsize + vx + 0] = buf[by * sht->bxsize + bx2 + 0];
						}
						if (map[vy * ctl->xsize + vx + 1] == sid) {
							vram[vy * ctl->xsize + vx + 1] = buf[by * sht->bxsize + bx2 + 1];
						}
						if (map[vy * ctl->xsize + vx + 2] == sid) {
							vram[vy * ctl->xsize + vx + 2] = buf[by * sht->bxsize + bx2 + 2];
						}
						if (map[vy * ctl->xsize + vx + 3] == sid) {
							vram[vy * ctl->xsize + vx + 3] = buf[by * sht->bxsize + bx2 + 3];
						}
					}
				}
				for (bx += i1 * 4; bx < bx1; bx++) {				/* 后边被4除多余部分逐字节写入 */
					vx = sht->vx0 + bx;
					if (map[vy * ctl->xsize + vx] == sid) {
						vram[vy * ctl->xsize + vx] = buf[by * sht->bxsize + bx];
					}
				}
			}
		} else {
			/* 1字节型 */
			for (by = by0; by < by1; by++) {
				vy = sht->vy0 + by;
				for (bx = bx0; bx < bx1; bx++) {
					vx = sht->vx0 + bx;
					if (map[vy * ctl->xsize + vx] == sid) {
						vram[vy * ctl->xsize + vx] = buf[by * sht->bxsize + bx];
					}
				}
			}
		}
	}
	return;
}
```
这个函数的代码很复杂，从参数来看，相对于显存，是要刷新从（vx0，vy0）左上角到（vx1，vy1）右下角组成的矩形区域，却也并不是从最下图层到最上图层，刷新的图层是h0层到h1层。提升了效率。
> 注意的一点是调用部分都是图层的(vx+bx,vy+by)是相对于该图层点的坐标换算到整个显存的点的坐标

整体的思想就是从h0图层到h1图层每个图层遍历找到和这块区域相交的部分，将该图层相交区域的颜色值写入到显存中。
* 开始就是循环遍历h0到h1图层， 然后把（vx0，vx1）和（vx1，vy1）换算到相对于的该图层坐标，如果小于该图层的左上角或者超过了该图层最大（右下角），给他限定在这个图层内。sid计算得出来该图层对应在sheets0中的index值，map中存放也是这个来表示图层信息。
* 接下来判断(sht->vx0 & 3) == 0，这个其实就是为了提升效率，如果满足条件表示该图层起始位置为4的倍数，这里也是一个性能优化，具体是想要一次性4字节写入到显存。
* ```for (by = by0; by < by1; by++) {```再来进入这个循环，是按行来遍历，首先绘制前边不足4个的部分，然后4的倍数，最后不满足4个部分，大体的代码都是判断该像素点是否在这个图层上（map存放的数据），如果是就将这个图层颜色值赋给相应位置的显存。
* 可以着重看下中间4倍数的部分，判断4个颜色值是否相同且位于该图层，就直接写4个字节颜色到显存中，不满足就还是一个颜色一个颜色的写入。
* 当左上角不满足4的倍数，也还是一个颜色一个颜色的写入到显存中
> 
>```c 
>i  = (bx0 + 3) / 4; /* bx0除以4(小数进位) */
>i1 =  bx1      / 4; /* bx1除以4(小数舍去) */
>i1 = i1 - i;
>```
>这几行代码使用了一个小的tips，(bx0+3)/4相当于bx0这个值除以4并向上取整，bx1/4就是除以4向下取整，正好可以取到是4的倍数的部分。

另外我们还需要关注的是map这个空间，我们用它来存放每个像素点所在的图层，这种方法很类似与图形学上的z-buffer方式，顾名思义就是存放像素的深度信息。map主要作用就是处理图层之间相互覆盖的情况，如果发生覆盖，有了像素的深度信息就能在绘制的时候去相应图层的像素来填充显存。

既然说到这里了，那我们在操作图层的时候可能会涉及到map这个空间的更新，我们也来看下这个函数：
```c
// sheet.c

void sheet_refreshmap(struct SHTCTL* ctl, int vx0, int vy0, int vx1, int vy1, int h0) {
	int h, bx, by, vx, vy, bx0, bx1, by0, by1, sid4, *p;
	unsigned char* buf, c, sid, *map = ctl->map; // sid == sheet id
	struct SHEET* sht;
	
	if (vx0 < 0) {
		vx0 = 0;
	}
	if (vy0 < 0) {
		vy0 = 0;
	}
	if (vx1 > ctl->xsize) {
		vx1 = ctl->xsize;
	}
	if (vy1 > ctl->ysize) {
		vy1 = ctl->ysize;
	}
	
	for (h = h0; h <= ctl->top; h++) {
		sht = ctl->sheets[h];
		sid = sht - ctl->sheets0; // 进行减法计算地址作为图层号码使用
		buf = sht->buf;
		
		// 使用vx0~vy1, 对bx0~by1进行倒退
		bx0 = vx0 - sht->vx0;
		by0 = vy0 - sht->vy0;
		bx1 = vx1 - sht->vx0;
		by1 = vy1 - sht->vy0;
		
		if (bx0 < 0) {bx0 = 0;}
		if (by0 < 0) {by0 = 0;}
		if (bx1 > sht->bxsize) {
			bx1 = sht->bxsize;
		}
		if (by1 > sht->bysize) {
			by1 = sht->bysize;
		}
		
		if (sht->col_inv == -1) { // 无透明
			if ((sht->vx0 & 3) == 0 && (bx0 & 3) == 0 && (bx1 & 3) == 0) { // 判断是否为4的倍数
				bx1 = (bx1 - bx0) / 4; // mov次数
				sid4 = sid | sid << 8 | sid << 16 | sid << 24;

				for (by = by0; by < by1; by++) {
					vy = sht->vy0 + by;
					vx = sht->vx0 + bx0;
					p = (int*)&map[vy * ctl->xsize + vx];
					for (bx = 0; bx < bx1; bx++) {
						p[bx] = sid4;
					}
				}
			}
			else {
				for (by = by0; by < by1; by++) {
					vy = sht->vy0 + by;
					for (bx = bx0; bx < bx1; bx++) {
						vx = sht->vx0 + bx;
						map[vy * ctl->xsize + vx] = sid;
					}
				}
			}
		}
		else { // 有透明
			for (by = by0; by < by1; by++) {
				vy = sht->vy0 + by;
				for (bx = bx0; bx < bx1; bx++) {
					vx = sht->vx0 + bx;
					c = buf[by * sht->bxsize + bx];
					if (c != sht->col_inv) {
						map[vy * ctl->xsize + vx] = sid;
					}
				}
			}
		}
	}
}
```
这个函数的代码也很长，表示今天都比较难，哈哈。没关系很多代码和sheet_refreshsub类似，最开始还是看下参数：
也还是更新（vx0,vy0）到（vx1,vy1）这块区域对应的map值，这里传进来一个图层的高度值h0，起始就是更新从h0层到最顶层top之间所有的像素点。
* 同样也是从图层的h0层到最顶层top来遍历，计算图层相应的index（可以当作图层的id），map可以存放不同图层的id，那么也就可以实现透明的效果，这里会先判断该图层是否会有透明的部分。
* 无透明的情况下先还是先来找是否满足4的倍数，这里要求图层左上角，更新区域的左上角的x坐标和右下角x坐标都是4的倍数，按4字节一次来写入到map。不是4的倍数就一份字节一次来写入到map
* 有透明的情况下，不好找4的倍数，因为4字节中可能夹杂着透明的点，所以也还是一个字节一次来更新map，如果遇到透明点不更新map，不是透明点才更新map

这里要说明的是由于是从底层到上层来更新map的，那么就会一层一层写入到map，如果覆盖的情况出现上层数据就会覆盖下层的数据写入到map中。透明效果就是在透明点不写入数据那么就是该层图层下边图层数据，也就达到了透明的效果。

作者也是为了能够达到图形运行的效率更高的效果，使得窗口的大小及目标位置点都是4的倍数。后边我们也能看到

#### 图层的移动
有了上边的基础，我们接下来看图层的一些操作就比较简单：
```c
// sheet.c

// 滑动
void sheet_slide(struct SHEET* sht, int vx0, int vy0) {
	struct SHTCTL* ctl = sht->ctl;
	int old_vx0 = sht->vx0, old_vy0 = sht->vy0;
	sht->vx0 = vx0;
	sht->vy0 = vy0;
	
	// 判断是否是显示的图层
	if (sht->height >= 0) {
		sheet_refreshmap(ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0);
		sheet_refreshmap(ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height);
		sheet_refreshsub(ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0, sht->height - 1);
		sheet_refreshsub(ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height, sht->height);
	}
}
```
代码也很简单，更新图层移动前的位置，更新图层移动后的位置。图层移动会导致map中的值发生变化，所有要先更新map的值，再去更新实际的图像的值

#### 图层的层级设定
有时候我们也需要把底层的图层提到最上层，或者把上层的图层移动到底层等等，所以我们看下层级变化如何设定：
```c
// sheet.c

void sheet_updown(struct SHEET* sht, int height) {
	struct SHTCTL* ctl = sht->ctl;
	int h, old = sht->height; // 存储射之前的高度信息
	
	// 如果指定的高度过高或过低，进行修正
	if (height > ctl->top + 1) {
		height = ctl->top + 1;
	}
	
	if (height < -1) {
		height = -1;
	}
	
	sht->height = height;
	
	// 设置完高度，然后对sheets[]重新排列
	if (old > height) { // 比之前高度低
		if (height >= 0) {
			for (h = old; h > height; h--) {
				ctl->sheets[h] = ctl->sheets[h - 1];
				ctl->sheets[h]->height = h;
			}
			ctl->sheets[height] = sht;
			sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height + 1);
			sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height + 1, old); // 刷新画面
		}
		else { // 设置该图层隐藏
			// 将该图层以上的图层下降一个层
			for (h = old; h < ctl->top; h++) {
				ctl->sheets[h] = ctl->sheets[h + 1];
				ctl->sheets[h]->height = h;
			}
			ctl->top--; // 有一个图层隐藏，显示图层减少一个，最大高度减一
			sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0);
			sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0, old - 1); // 刷新画面
		}
	}
	else if (old < height) { // 比之前高度高
		if (old >= 0) { // old和height之间的图层降1
			for (h = old; h < height; h++) {
				ctl->sheets[h] = ctl->sheets[h + 1];
				ctl->sheets[h]->height = h;
			}
			ctl->sheets[height] = sht;
		}
		else { // 由隐藏变为显示，height高度及以上图层升高1
			for (h = ctl->top; h >= height; h--) {
				ctl->sheets[h + 1] = ctl->sheets[h];
				ctl->sheets[h + 1]->height = h + 1;
			}
			ctl->sheets[height] = sht;
			ctl->top++; // 图层加1
		}
		sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height);
		sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height, height); // 刷新画面
	}
}
```
参数中height就是要将该图层设定为哪一层。