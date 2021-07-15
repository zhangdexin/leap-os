#include "bootpack.h"

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

void memman_init(struct MEMMAN* man) {
	man->frees = 0;    // 可用信息数目
	man->maxfrees = 0; // 用于观察可用状况，frees的最大值
	man->lostsize = 0; // 释放失败的内存的大小总和
	man->losts = 0;    // 释放失败的次数
}

// 内存剩余多少
unsigned int memman_total(struct MEMMAN* man) {
	unsigned int i, t = 0;
	for (i = 0; i < man->frees; ++i) {
		t += man->free[i].size;
	}
	return t;
}

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