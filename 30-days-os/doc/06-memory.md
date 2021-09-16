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
再来看释放内存的，