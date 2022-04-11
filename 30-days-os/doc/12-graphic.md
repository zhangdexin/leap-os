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
来看下画出来的屏幕是啥样的吧：
<img src=https://user-images.githubusercontent.com/22785392/162738625-edb3e7bf-3dba-4808-b116-189c65affd5d.png width="60%" height="60%" />

