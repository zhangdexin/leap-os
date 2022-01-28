## 内存管理

### 前言
这一节我们来讲内存管理，因为这块内容比较独立，我们理解起来比较简单，作者这这个内存管理的代码也很优秀，单独拿出来我们日常使用也是不错的选择。这部分代码主要是在haribote/memory.c

### 代码讲解
首先我们看下关于内存管理的数据结构
```C++
// bootpack.h

#define MEMMAN_FREES 4090	/* free[]大约32KB(8 * 4090) */
#define MEMMAN_ADDR	 0x003c0000 // 内存信息表存放在0x003c0000

// 可用信息
struct FREEINFO { 
	unsigned int addr, size;
};

// 内存管理器
struct MEMMAN {
	int frees, maxfrees, lostsize, losts;
	struct FREEINFO free[MEMMAN_FREES];
};
```
可以看到MEMMAN(memory manager)是作为内存管理器。
* struct FREEINFO free表示可以使用的空间详细信息的数组，数组大小为MEMMAN_FREES，可以存放不连续MEMMAN_FREES数量内存的信息，数组内部存储的是FREEINFO这个结构体对象，FREEINFO保存这块内存的起始地址和大小。
* frees表示内存中可以使用的条目
* maxfrees用于观察可用状况作为frees的最大值，不过我看代码好像没有什么特殊的用处
* lostsize释放失败的内存大小总和
* losts释放失败的次数

那么接下来我们再看下具体实现是什么样的

```C++
// 
void memman_init(struct MEMMAN* man) {
	man->frees = 0;    // 可用信息数目
	man->maxfrees = 0; // 用于观察可用状况，frees的最大值
	man->lostsize = 0; // 释放失败的内存的大小总和
	man->losts = 0;    // 释放失败的次数
}
```
首先初始化MEMMAN这个结构体对象，全部为0。<br>

```C++
// 分配内存
unsigned int memman_alloc(struct MEMMAN* man, unsigned int size) {
	unsigned int i, a;
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].size >= size) {
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0) {
				man->frees--;
				for (; i < man->frees; i++) {
					man->free[i] = man->free[i + 1];
				}
			}
			
			return a;
		}
	}
	return 0;
}
```
再看内存如何分配，参数是man和要分配的大小。
这里其实就是从frees里边找到一个容量大于等于要分配大小的内存，然后将该free[i]的内存地址向后移动，如果正好等于要分配内存的大小，即该free[i]全部分配出去，那么将frees的总量减1，循环将数组中i后边标记的free向前移动覆盖这个free[i]，进而保证数组的连续性。<br>

```C++
// 释放内存
int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size) {
	int i, j;
	// 为便于归纳内存，将free[]按照addr的顺序排列
	// 先决定放在哪里
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].addr > addr) {
			break;
		}
	}
	
	// 前边有可用内存
	if (i > 0) {
		// 如果前一块内存容量正好和指定内存地址可以拼接，则并入前一块内存
		if (man->free[i - 1].addr + man->free[i - 1].size == addr) {
			man->free[i - 1].size += size;
			if (i < man->frees) { // 如果指定内存位置不是在最后一位
				if (addr + size == man->free[i].addr) { // 检查是否也可以和后一块内存拼接上
					man->free[i - 1].size += man->free[i].size;
					
					// 删除第i块内存，并前移后边内存块
					man->frees--;
					for (; i < man->frees; i++) {
						man->free[i] = man->free[i + 1];
					}
				}
			}
			
			return 0;
		}
	}
	
	// 执行完前边观察i的值，发现不能合并相应的块
	if (i < man->frees) {
		// 查看是否能与后边块合并
		if (addr + size == man->free[i].addr) {
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0;
		}
	}
	
	if (man->frees < MEMMAN_FREES) {
		// free[i]之后向后移动
		for (j = man->frees; j > i; j--) {
			man->free[j] = man->free[j - 1];
		}
		man->frees++;
		if (man->maxfrees < man->frees) {
			man->maxfrees = man->frees; // 更新最大值
		}
		
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0;
	}
	
	
	// 超过存放free的最大空间，释放失败了
	man->losts++;
	man->lostsize += size;
	return -1;
}
```
再来看内存的释放，比较复杂的是需要考虑将释放的内存块和前后free能不能合并成一个大块的内存，条件自然是两块内存是连续就可以合并。
我们看下代码，也比较简单，首先从man中找到释放内存地址的后边一块free信息，记录i。判断前边是否已经有内存，如果前一块内存能够和释放的内存拼接，那么就并入前一块内存。然后看下能不能和后边块合并，如果都不行就只能插入到i这个位置了。
如果以上都不能满足，那么久记录一次释放失败。

```C++
// 内存剩余多少
unsigned int memman_total(struct MEMMAN* man) {
	unsigned int i, t = 0;
	for (i = 0; i < man->frees; ++i) {
		t += man->free[i].size;
	}
	return t;
}
```
这个函数能够得到内存还有多少剩余。比较简单就不展开了。<br><br>

除了这些常规的操作，还有一个特殊的函数，我列出来大家看下：
```C++
#define EFLAG_AC_BIT      0x00040000
#define CR0_CACHE_DISABLE 0x60000000

unsigned int memtest(unsigned int start, unsigned int end) {
	char flg486 = 0;
	unsigned int eflg, cr0, i;
	
	eflg = io_load_eflags();
	eflg |= EFLAG_AC_BIT;
	io_store_eflags(eflg);
	
	eflg = io_load_eflags();
	
	// EFLAGS寄存器第18位即所谓的AC标志位，cpu是386，就没有这个标志位
	// 如果是386，即使设定AC=1,AC的值还会自动回到0
	if ((eflg & EFLAG_AC_BIT) != 0) {
		flg486 = 1;
	}
	
	// 恢复
	eflg &= ~EFLAG_AC_BIT;
	io_store_eflags(eflg);
	
	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; // 禁止缓存，CR0寄存器设置还可以切换成保护模式，见asmhead
		store_cr0(cr0);
	}
	
	i = memtest_sub(start, end);
	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; // 允许缓存
		store_cr0(cr0);
	}
	
	return i;
}
```
这个函数的功能是去校验内存有多大，通过一种向内存写一个值，然后去读取的方法来判断。因为cpu访问内存时会经过高速缓存，所以这个时候我们需要将高速缓存禁用掉。通过eflag标志判断cpu是不是486，因为486之前是没有高速缓存的，所以无需禁用。禁用高速缓存还是在cr0寄存器上做手脚，cr0寄存器我们上一节也说到，cr0的第30位是CD(Cache Disable),我们这里设置这位是1即可。然后我们调用memtest_sub函数来校验，完成后恢复使用高速缓存。<br>
接着我们继续看下memtest_sub函数，这个函数是汇编代码写的，毕竟C语言不能做完所有的事情。
> c语言调用的代码写在naskfunc.nas, 编译的时候也会编译成obj，链接时可以和.c文件编译成的obj文件链在一起，即可以互相调用。关于编译可以参考第二节。

```
_memtest_sub:	; unsigned int memtest_sub(unsigned int start, unsigned int end)
		PUSH	EDI						; （由于本程序会使用EBX,ESI,EDI会改变这里值，所以要先保存起来，程序执行完再恢复）
		PUSH	ESI
		PUSH	EBX
		MOV		ESI,0xaa55aa55			; pat0 = 0xaa55aa55;
		MOV		EDI,0x55aa55aa			; pat1 = 0x55aa55aa;
		MOV		EAX,[ESP+12+4]			; i = start;
		
; 每0x1000检查一次，检查0x1000最后四个字节
mts_loop:
		MOV		EBX,EAX
		ADD		EBX,0xffc				; p = i + 0xffc;
		MOV		EDX,[EBX]				; old = *p 先记住修改前的值
		MOV		[EBX],ESI				; *p = pat0; 试写
		XOR		DWORD [EBX],0xffffffff	; *p ^= 0xffffffff; 反转，异或达到取反的效果，pat0的二进制是10交差
		CMP		EDI,[EBX]				; if (*p != pat1) goto fin; 检查反转结果
		JNE		mts_fin
		XOR		DWORD [EBX],0xffffffff	; *p ^= 0xffffffff;  再次反转
		CMP		ESI,[EBX]				; if (*p != pat0) goto fin; 检查是否恢复
		JNE		mts_fin
		MOV		[EBX],EDX				; *p = old;  恢复修改之前的值
		ADD		EAX,0x1000				; i += 0x1000;
		CMP		EAX,[ESP+12+8]			; if (i <= end) goto mts_loop;
		JBE		mts_loop
		POP		EBX
		POP		ESI
		POP		EDI
		RET
mts_fin:
		MOV		[EBX],EDX				; *p = old;
		POP		EBX
		POP		ESI
		POP		EDI
		RET
```
本书中作者有c语言的版本：
```
unsigned int memtest_sub(unsigned int start, unsigned int end)
{
	unsigned int i, *p, old, pat0 = 0xaa55aa55, pat1 = 0x55aa55aa;
	for (i = start; i <= end; i += 0x1000) {
		p = (unsigned int *) (i + 0xffc);
		old = *p;			/* 记住修改前的值 */
		*p = pat0;			/* *p = pat0; 试写 */
		*p ^= 0xffffffff;	/* 反转，异或达到取反的效果 */
		if (*p != pat1) {	/* 检查反转结果 */
not_memory:
			*p = old;
			break;
		}
		*p ^= 0xffffffff;	/* 再次反转 */
		if (*p != pat0) {	/* 检查是否恢复 */
			goto not_memory;
		}
		*p = old;			/* 恢复修改之前的值 */
	}
	return i;
}
```
大概讲解下意思是，会检验start到end这块内存是否可以操作，每0x1000通过写入四字节pat0，并对其反转等操作判断是否符合预期，如何符合预期则表明这块内存可用，进而得到内存的最大值。<br>
> 大家也许会疑惑这个不是c语言可以实现吗，为啥要汇编呢，关于这个作者也是花费了一节来说明，大家看到代码中p最终恢复原来的值，编译器就给把异或等一些操作给优化了。导致结果不合预期。

最后我们来说下，为了方便使用及内存对齐，作者封装了两个函数，分配内存和释放内存以4k为单位的功能：
```C++
// 0x1000B = 4KB
unsigned int memman_alloc_4k(struct MEMMAN* man, unsigned int size) {
	unsigned int a;
	size = (size + 0xfff) & 0xfffff000; // 向上(4K)进位
	a = memman_alloc(man, size);
	return a;
}

int memman_free_4k(struct MEMMAN* man, unsigned int addr, unsigned int size) {
	int i;
	size = (size + 0xfff) & 0xfffff000; // 向上(4K)进位
	i = memman_free(man, addr, size);
	return i;
}
```

### 如何使用
那我们如何使用呢，我们看下程序中如何调用的：
```C++
// bootpack.c

// ...(略)
struct MEMMAN* memman = (struct MEMMAN*)MEMMAN_ADDR;

// ...(略)

memtotal = memtest(0x00400000, 0xbfffffff);
memman_init(memman);
memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
memman_free(memman, 0x00400000, memtotal - 0x00400000);

// ...(略)

// eg1. alloc
buf_back = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);

// eg2. free
memman_free_4k(memman, (int) fat, 4 * 2880);

```
相关的代码我都罗列出来了，首先我们声明memman，我们把MEMMAN_ADDR(0x003c0000)这块内存地址存放内存信息表。<br>
我们获取这块内存的大小0x00400000~0xbfffffff，初始化memman的内容，然后将0x00001000起始0x0009e000大小内存和0x00400000起始(memtotal - 0x00400000)大小的内存都放置在memman这里管理，这也是我们可以使用的所有内存了。<br>

我们参考上一章的内存分布图及这一章我们需要存放memman到0x003c0000处，所以我们空余的内存地址就是0x00001000~0x0009e000和0x00400000~0xbfffffff。<br>

然后我列出了alloc和free内存的两个使用例子，供大家参考

### 总结
内存管理我们就讲完了，主要是讲述程序是如何来管理内存的资源的，作者使用的数据结构存放的时没有使用的空间信息，而不是平常一些内存管理存放使用和没使用管理的信息，不过这样来说作者实现的内存管理器在释放的时候必须要指定内存大小，这也是不方便的一点。总的来说实现很小巧，比较精致。
