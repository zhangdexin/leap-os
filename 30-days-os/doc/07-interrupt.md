## GDT初始化和中断

### 前言
这一节我们来讲关于中断，同时之前的GDT加载的部分我们还需要重新改动下，因为之前的GDT的加载是作者临时拿一块内存存储，这里是要重新规划下。

### GDT初始化
这里我们GDT的初始化是用C语言来写的，对于我们整个理解GDT加载及设置有很好的帮助，首先我们看下数据结构：
```
// bootpack.h
// GDT(global segment descriptor table), 全局段号记录表，存储在内存
// 起始地址存放在GDTR的寄存器中
struct SEGMENT_DESCRIPTOR {
	short limit_low, base_low;
	char base_mid, access_right;
	char limit_high, base_high;
};
```
这里是段描述符的表示，大家可以翻到的4节参考那个图来理解这个数据结构，内存中不同位置表示不同的含义，这里依次（从低到高）表示的段界限（low），段基址（low），段基址（mid），权限，段界限（hight<其中4位表示其他含义>），段基址（base）

这里作者写了一个函数来设置GDT的段描述：
```
// dsctbl.c
void set_segmdesc(struct SEGMENT_DESCRIPTOR *sd, unsigned int limit, int base, int ar)
{
	if (limit > 0xfffff) {
		ar |= 0x8000; /* G_bit = 1 */
		limit /= 0x1000;
	}
	sd->limit_low    = limit & 0xffff;
	sd->base_low     = base & 0xffff;
	sd->base_mid     = (base >> 16) & 0xff;
	sd->access_right = ar & 0xff;
	sd->limit_high   = ((limit >> 16) & 0x0f) | ((ar >> 8) & 0xf0);
	sd->base_high    = (base >> 24) & 0xff;
	return;
}
```
这里limit和base也没什么好说，分别将其内容拆解到内存不同位置，我们看ar（access_right）是4字节保存的，把低8位赋值给access_right为访问权限，ar的第12~15位及limit的16~19组合赋值给limit_high，因为我们说过limit_hight中有4位表示其他含义。我们也可以看到如果limit的值大于0xfffff(1M),GDT段界限保存为20位宽，即最大是1M，如果超过1M我们需要把保存段界限的单位变成4K（原来是1字节），这里同样可以参考第4节我们对段描述符讲解的G_bit,G_bit设置为1，段界限的单位是4K，那么limit表示也要除以4K。

```
// bootpack.h
#define ADR_GDT			0x00270000
#define ADR_BOTPAK		0x00280000
#define LIMIT_BOTPAK	0x0007ffff
#define AR_DATA32_RW	0x4092
#define AR_CODE32_ER	0x409a

// dsctbl.c
struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
	

/* GDT初始化 */
for (int i = 0; i <= LIMIT_GDT / 8; i++) {
    set_segmdesc(gdt + i, 0, 0, 0);
}
set_segmdesc(gdt + 1, 0xffffffff,   0x00000000, AR_DATA32_RW);
set_segmdesc(gdt + 2, LIMIT_BOTPAK, ADR_BOTPAK, AR_CODE32_ER);
load_gdtr(LIMIT_GDT, ADR_GDT);

```
然后我们初始化整个GDT，首先将gdt存放在ADR_GDT(0x00270000)位置处。和第5章我们讲到，第0个段描述符是全0，同样也设置第一个段描述符指向全部内存，第二个段描述符从0x00280000，段界限是0x0007ffff。段属性AR_DATA32_RW(0x4092)大家可以简单理解成表示的是可读写的数据段，AR_CODE32_ER表示可读可写可执行的代码段。也就是说我们ADR_BOTPAK起始到LIMIT_BOTPAK都是操作系统（bootpack）的代码段。

### 中断知识储备

### IDT的初始化

### 中断处理程序

### 总结