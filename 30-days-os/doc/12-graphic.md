## 图像显示

### 序
这一节我们来讲讲关于图像显示，我们之前（03讲）有说过，内存中有一部分是和显示图像有关，也即显存映射到内存的部分，我们如果要操作图像显示等，只需要操作这一块内存即可。


### 关于画面设定
我们在05节中穿插着关于画面设定的部分，我们借此再来回忆一下，我们使用VBE可以使用高分辨率的画面，首先回去判断显卡是否支持VBE。不支持或者版本小于2.0的话我们使用默认的320x200x8bit的彩色。如果支持的话我们就通过设定画面模式号码来选择一个画面模式，由于作者使用qemu来调试，所以最大的分辨率也就是1024*768的。
```
; 0x101  -> 640*480*8
; 0x103  -> 800*600*8
; 0x105  -> 1024*768*8
; 0x107  -> 1280*1024*8(QEMU中不能指定到0x107)
```
调用INT 10中断，BIOS就会把相关数据写入到[ES:DI]开始的256字节。然后就会获得了一系列的画面信息：
```
  ; 画面模式信息重要信息：
  ; WORD [ES:DI+0x00] 模式属性
  ; WORD [ES:DI+0x12] x的分辨率
  ; WORD [ES:DI+0x14] y的分辨率
  ; BYTE [ES:DI+0x19] 颜色数，必须是8
  ; BYTE [ES:DI+0x1b] 颜色指定方法，必须为4（调色板模式）
  ; DWORD [ES:DI+0x28] VRAM的地址
```
关键的有获取到的x，y分辨率，位数是8，颜色指定模式调色板模式，同时还获取到了VRAM的地址，也就是我们往这个地址中写入数据就会控制画面的显示了。
我们把这些重要的信息拿出来保存起来，再来列一下保存的位置：
```
; 有关BOOT_INFO存放地址
CYLS	EQU		0x0ff0			; 柱面数
LEDS	EQU		0x0ff1
VMODE	EQU		0x0ff2			; 关于颜色数目信息，颜色位数
SCRNX	EQU		0x0ff4			; 分辨率x
SCRNY	EQU		0x0ff6			; 分辨率x
VRAM	EQU		0x0ff8			; 存放图像缓冲区的开始地址
```
那么我们在06节已经知道，把这些数据通过地址转化为BOOTINFO的结构体对象，这样一来我们使用就方便很多了。

### 初始化调色板
上边我们拿到了图像缓冲区，只要往这个缓冲区写入数据就能显示出来了，其实就是写入颜色数据就可以了。我们上边也看到我们还需要设定颜色值，由于使用调色板模式的颜色指定方式，所以我们需要通过设定调色板从而能够往缓冲区写入颜色数据。
我们先来初始化下调色板：颜色位数是8，也就是说设置颜色色号是8位存储。那么就可以存储256中颜色。
如何设置调色板呢，就是说设置一个颜色号对应一个颜色，设定好了之后我们只需要往vram中写入色号就可以了
```c
// bootpack.c
void HariMain(void)
{
    // ...(略)
    init_palette();
    // ...(略)
}
```
在main函数中调用init_palette函数就是初始化调色板，我们去看下这个函数：
```c
// graphic.c

void set_palette(int start, int end, unsigned char *rgb)
{
	int i, eflags;
	eflags = io_load_eflags();	/* 记录中断许可标志 */
	io_cli(); 					/* 禁止中断 */
	io_out8(0x03c8, start);     // 向显卡0x03c8端口写数据
	for (i = start; i <= end; i++) {
		io_out8(0x03c9, rgb[0] / 4);
		io_out8(0x03c9, rgb[1] / 4);
		io_out8(0x03c9, rgb[2] / 4);
		rgb += 3;
	}
	io_store_eflags(eflags);	/* 复原中断许可标志 */
	return;
}

void init_palette(void)
{
	static unsigned char table_rgb[16 * 3] = {
		0x00, 0x00, 0x00, /* 0:黑 */
        0xff, 0x00, 0x00, /* 1:亮红 */ 
        0x00, 0xff, 0x00, /* 2:亮绿 */ 
        0xff, 0xff, 0x00, /* 3:亮黄 */ 
        0x00, 0x00, 0xff, /* 4:亮蓝 */ 
        0xff, 0x00, 0xff, /* 5:亮紫 */ 
        0x00, 0xff, 0xff, /* 6:浅亮蓝 */ 
        0xff, 0xff, 0xff, /* 7:白 */ 
        0xc6, 0xc6, 0xc6, /* 8:亮灰 */ 
        0x84, 0x00, 0x00, /* 9:暗红 */ 
        0x00, 0x84, 0x00, /* 10:暗绿 */ 
        0x84, 0x84, 0x00, /* 11:暗黄 */ 
        0x00, 0x00, 0x84, /* 12:暗青 */ 
        0x84, 0x00, 0x84, /* 13:暗紫 */ 
        0x00, 0x84, 0x84, /* 14:浅暗蓝 */ 
        0x84, 0x84, 0x84  /* 15:暗灰 */
	};
	unsigned char table2[216 * 3];
	int r, g, b;

	set_palette(0, 15, table_rgb);
	for (b = 0; b < 6; b++) {
		for (g = 0; g < 6; g++) {
			for (r = 0; r < 6; r++) {
				table2[(r + g * 6 + b * 36) * 3 + 0] = r * 51;
				table2[(r + g * 6 + b * 36) * 3 + 1] = g * 51;
				table2[(r + g * 6 + b * 36) * 3 + 2] = b * 51;
			}
		}
	}
	set_palette(16, 231, table2);
	return;
}
```
init_palette代码比较简单，首先声明16个常用的颜色的rgb，然后调用set_palette进行设置，0\~15号按顺序设置table_rgb中的颜色。
然后为了让颜色更为的丰富，我们增加到了6\*6\*6=216种颜色，设置的颜色号码就是从16\~231。
比较重要的其实是set_palette这个函数，因为我们要对显卡进行设置，所以肯定会涉及到设置外部设备的字样，out命令肯定是少不了的。其实相对来说也不是很难，就是设置调色板的固定步骤：
1. 想要设置的颜色号码写入到0x03c8（设备号码），紧接着按照R,G,B的顺序写入到0x03c9
2. 如果还想要按照顺序设置下一个颜色号码，则可以省略掉写入0x03c8这一步，直接再按照R,G,B顺序写到0x03c9。
所以我们程序代码的意思就比较简单了，首先得先禁用中断，不然进来中断访问了调色板很容易引起错乱，所以针对外部设备的设置都是要禁用中断先，然后写入颜色号码对应颜色的RGB，恢复中断即可。

我们基本上经常使用的还是前16种，另外我们也在bootpack.h种宏定义了：
```c
#define COL8_000000		0
#define COL8_FF0000		1
#define COL8_00FF00		2
#define COL8_FFFF00		3
#define COL8_0000FF		4
#define COL8_FF00FF		5
#define COL8_00FFFF		6
#define COL8_FFFFFF		7
#define COL8_C6C6C6		8
#define COL8_840000		9
#define COL8_008400		10
#define COL8_848400		11
#define COL8_000084		12
#define COL8_840084		13
#define COL8_008484		14
#define COL8_848484		15
```

### 屏幕的画面
有了颜色之后我们就要做点什么，首先肯定是把整个屏幕的颜色画出来。
```c
// graphic.c
void init_screen8(char *vram, int x, int y)
{
	boxfill8(vram, x, COL8_008400,  0,     0,      x -  1, y - 29);
	boxfill8(vram, x, COL8_C6C6C6,  0,     y - 28, x -  1, y - 28);
	boxfill8(vram, x, COL8_FFFFFF,  0,     y - 27, x -  1, y - 27);
	boxfill8(vram, x, COL8_C6C6C6,  0,     y - 26, x -  1, y -  1);

	boxfill8(vram, x, COL8_FFFFFF,  3,     y - 24, 59,     y - 24);
	boxfill8(vram, x, COL8_FFFFFF,  2,     y - 24,  2,     y -  4);
	boxfill8(vram, x, COL8_848484,  3,     y -  4, 59,     y -  4);
	boxfill8(vram, x, COL8_848484, 59,     y - 23, 59,     y -  5);
	boxfill8(vram, x, COL8_000000,  2,     y -  3, 59,     y -  3);
	boxfill8(vram, x, COL8_000000, 60,     y - 24, 60,     y -  3);

	boxfill8(vram, x, COL8_848484, x - 47, y - 24, x -  4, y - 24);
	boxfill8(vram, x, COL8_848484, x - 47, y - 23, x - 47, y -  4);
	boxfill8(vram, x, COL8_FFFFFF, x - 47, y -  3, x -  4, y -  3);
	boxfill8(vram, x, COL8_FFFFFF, x -  3, y - 24, x -  3, y -  3);
	return;
}
```
这里也没有什么，就是调用了几次boxfill8画不同颜色的矩形，这个大家可以自己去算一下都是哪些矩形。我们在看下boxfill8这个函数：

```c
void boxfill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1)
{
	int x, y;
	for (y = y0; y <= y1; y++) {
		for (x = x0; x <= x1; x++)
			vram[y * xsize + x] = c;
	}
	return;
}
```
(x0,y0)是左上角的点，(x1,y1)是右下角的点，vram这里就是显示图像的buffer，我们通过init_screen8来写入屏幕，那么这里vram就是显存，大小自然就是屏幕分辨率，也是我们前边设定好的。
xsize这里传入的buffer最终画出来图像的宽度，这里也就是屏幕的宽度。这里不是很好理解，我简单解释一下：
我们看代码中y代表行数，x代码列数，自然(x,y)就是第y行第x列。由于vram是一个一维数组，那么换一行的话，就需要y加上整个vram的宽才能到下一行。所以xsize代码是buffer(这里参数就是vram)代表图像的宽。
那我们要画一个矩形就是在从左上角(x0,y0)到右下角(x1,y1)的所有点写入设定的颜色色号。
来看下画出来的屏幕是啥样的吧：(0和1的字符构成)
<img src=https://user-images.githubusercontent.com/22785392/162738625-edb3e7bf-3dba-4808-b116-189c65affd5d.png width="60%" height="60%" />

### 鼠标的画面
如此我们再来画一下鼠标的画面，先来看下相关的代码：
```c
// graphic.c
void init_mouse_cursor8(char *mouse, char bc)
/* 准备鼠标指针（16x16） */
{
	static char cursor[16][16] = {
		"**************..",
		"*OOOOOOOOOOO*...",
		"*OOOOOOOOOO*....",
		"*OOOOOOOOO*.....",
		"*OOOOOOOO*......",
		"*OOOOOOO*.......",
		"*OOOOOOO*.......",
		"*OOOOOOOO*......",
		"*OOOO**OOO*.....",
		"*OOO*..*OOO*....",
		"*OO*....*OOO*...",
		"*O*......*OOO*..",
		"**........*OOO*.",
		"*..........*OOO*",
		"............*OO*",
		".............***"
	};
	int x, y;

	for (y = 0; y < 16; y++) {
		for (x = 0; x < 16; x++) {
			if (cursor[y][x] == '*') {
				mouse[y * 16 + x] = COL8_000000;
			}
			if (cursor[y][x] == 'O') {
				mouse[y * 16 + x] = COL8_FFFFFF;
			}
			if (cursor[y][x] == '.') {
				mouse[y * 16 + x] = bc;
			}
		}
	}
	return;
}
```
因为涉及到图层的关系，我们并不是把鼠标直接画到显存，而是先写入到一个缓冲区mouse，鼠标单独自己是一个图层，之后只要把这个缓冲区（图层）画到显存即可，关于画到显存中我们讲到图层管理就知道了，这里我们只考虑画到这个缓冲区中即可。设定这个缓冲区保存的是16\*16的画像，图像内容（cursor）我们写出来了。\*的地方黑色，0的地方白色，.的地方画成bc这个颜色。bc这个颜色其实是这个图层的透明色，当后边讲到图层管理，只需要把这个颜色的地方设置成下一个图层的颜色就可以达到透明的效果。

### 字体的画面
我们学习上边关于鼠标的绘制，那么文字的绘制也是类似，相当于也是提供一个字符图像，然后根据不同字符设置不同颜色。这个字符图像是在hankaku.txt，可以简单看下这里边的内容：

<img src="https://user-images.githubusercontent.com/22785392/163544593-77ca0c9c-6f20-44e0-a19d-1c23894a3206.png" width="10%" height="10%" />


作者使用makefont.exe转化为成hankaku.bin，再用bin2obj.exe转为obj使用，这里的转化是将.会转为0，\*转为1，一行存成一个字符，比如```..***...```转为00111000, 存为char就是111000。
```c
extern char hankaku[4096];
```
使用extern就可以使用到这个变量，且hankaku中存放是按照acsii码存放了256个字符，每个字符有16行，那么访问只需要是hankaku+ascii码\*16开始的16字节。比如说A就是hankaku+0x41\*16开始的16字节（16行）
看下显示字符的代码：
```c
// graphic.c
void putfonts8_asc(char *vram, int xsize, int x, int y, char c, unsigned char *s)
{
	extern char hankaku[4096];
	struct TASK *task = task_now();
	char *nihongo = (char *) *((int *) 0x0fe8), *font;
	int k, t; // k存放区号， t存放点号

	if (task->langmode == 0) { // 英文
		for (; *s != 0x00; s++) {
			putfont8(vram, xsize, x, y, c, hankaku + *s * 16);
			x += 8;
		}
	}
	if (task->langmode == 1) { // 日文半角和shift-jis模式
		// ...(略)
	}
	if (task->langmode == 2) { // 日文EUC模式
		// ...(略)
	}
	return;
}
```
关于显示日文这里不讲解了，我们只看英文字体的。vram也是写入字体的缓冲区，xsize是这个缓冲区表示画面的宽度，(x,y)指这个画面中(x,y)位置作为绘制字体的左上角，c指颜色号，s是要写的字符串。```for (; *s != 0x00; s++)```这个循环表示一个字符一个字符的画出来， putfont8显然就是画一个字符。
```c
// graphic.c
void putfont8(char *vram, int xsize, int x, int y, char c, char *font)
{
	int i;
	char *p, d /* data */;
	for (i = 0; i < 16; i++) {
		p = vram + (y + i) * xsize + x;
		d = font[i];
		if ((d & 0x80) != 0) { p[0] = c; }
		if ((d & 0x40) != 0) { p[1] = c; }
		if ((d & 0x20) != 0) { p[2] = c; }
		if ((d & 0x10) != 0) { p[3] = c; }
		if ((d & 0x08) != 0) { p[4] = c; }
		if ((d & 0x04) != 0) { p[5] = c; }
		if ((d & 0x02) != 0) { p[6] = c; }
		if ((d & 0x01) != 0) { p[7] = c; }
	}
	return;
}
```
上边我们说到了把一个字符的一行转化为一个字符，putfont8就是根据这个字符中二进制位中1就画上颜色c。'd & 0x80'即为d和0x1000000做与运算，能够判断出最高位是不是1，剩下的与运算依次类推。就能设置了vram中颜色了，在把这个缓冲区画到显存就能显示文字。

### 一个窗口的画面
接下来我们讲一下如何画出来一个窗口，其实我们有了上边接触，应该就能够画出来一个窗口了吧，很多也都是重复性的工作。
```c
// window.c
void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act)
{
	boxfill8(buf, xsize, COL8_C6C6C6, 0,         0,         xsize - 1, 0        );
	boxfill8(buf, xsize, COL8_FFFFFF, 1,         1,         xsize - 2, 1        );
	boxfill8(buf, xsize, COL8_C6C6C6, 0,         0,         0,         ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1,         1,         1,         ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1,         xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0,         xsize - 1, ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2,         2,         xsize - 3, ysize - 3);
	boxfill8(buf, xsize, COL8_848484, 1,         ysize - 2, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, 0,         ysize - 1, xsize - 1, ysize - 1);

	make_wtitle8(buf, xsize, title, act);
	return;
}
```
我们看到是画了很多的矩形组成一个窗口，另外也都是画在了buf这个缓冲区中，xsize和ysize分别表示窗口的长宽。
然后我们看make_wtitle8是画了一个窗口的标题栏，把act和titile的字符串传进来
```c
void make_wtitle8(unsigned char *buf, int xsize, char *title, char act)
{
	static char closebtn[14][16] = {
		"OOOOOOOOOOOOOOO@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQQQ@@QQQQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"O$$$$$$$$$$$$$$@",
		"@@@@@@@@@@@@@@@@"
	};
	int x, y;
	char c, tc, tbc;
	if (act != 0) {
		tc = COL8_FFFFFF;
		tbc = COL8_000084;
	} else {
		tc = COL8_C6C6C6;
		tbc = COL8_848484;
	}
	boxfill8(buf, xsize, tbc, 3, 3, xsize - 4, 20);
	putfonts8_asc(buf, xsize, 24, 4, tc, title);
	for (y = 0; y < 14; y++) {
		for (x = 0; x < 16; x++) {
			c = closebtn[y][x];
			if (c == '@') {
				c = COL8_000000;
			} else if (c == '$') {
				c = COL8_848484;
			} else if (c == 'Q') {
				c = COL8_C6C6C6;
			} else {
				c = COL8_FFFFFF;
			}
			buf[(5 + y) * xsize + (xsize - 21 + x)] = c;
		}
	}
	return;
}
```
哈，作者还是用字符写了一个关闭按钮（所有的@构成了x样子），根据不同的字符设置不同的颜色。act的意思表示是这个窗口是否被激活，根据激活状态设置不同的颜色给标题栏和标题文字。然后再来画关闭按钮，```(5 + y) * xsize + (xsize - 21 + x)```这里5是离窗口最上边的距离，```xsize-21```表示是关闭按钮距离左边的位置。
也可以来看一个窗口的样子：(未激活和激活状态)

<img src="https://user-images.githubusercontent.com/22785392/163545421-e8ae3ac5-798a-4a98-9a69-e8a7e2516714.png" />
<img src="https://user-images.githubusercontent.com/22785392/163545512-fb6b9204-9a71-420c-af0f-2374e8e182d2.png" />

### 总结
这一章我们讲述了怎么画一个画面，虽然多数是画在指定的buf，但是我们知道最终画在前边申请的显存就可以显示了，开始我们需要设定颜色号，需要针对显卡进行设置。然后就可以肆无忌惮的往显存写颜色号就能显示了。这一章我们分别把屏幕背景，鼠标，文字，窗口都画出来了，那么最基本的其实都可以出来了，下面一章我们讲图层管理，把这些画面管理起来，相互作用，激活状态等等。
