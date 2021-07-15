#include "bootpack.h"


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
		return ;
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

struct SHEET* sheet_alloc(struct SHTCTL* ctl) {
	struct SHEET* sht;
	int i;
	for (i = 0; i < MAX_SHEETS; i++) {
		if (ctl->sheets0[i].flags == 0) {
			sht = &ctl->sheets0[i];
			sht->flags = SHEET_USE; // 标记为正在使用
			sht->height = -1; // 隐藏
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

void sheet_refresh(struct SHEET* sht, int bx0, int by0, int bx1, int by1) {
	if (sht->height >= 0) { // 判断是否在显示
		sheet_refreshsub(sht->ctl, sht->vx0 + bx0, sht->vy0 + by0, sht->vx0 + bx1, sht->vy0 + by1, sht->height, sht->height);
	}
}

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

// 释放sht
void sheet_free(struct SHEET* sht) {
	if (sht->height >= 0) {
		sheet_updown(sht, -1); // 显示状态下，设定为隐藏
	}
	
	sht->flags = 0;
}

void sheet_refreshsub(struct SHTCTL* ctl, int vx0, int vy0, int vx1, int vy1, int h0, int h1) {
	int h, bx, by, vx, vy, bx0, bx1, by0, by1;
	unsigned char* buf, *vram = ctl->vram, *map = ctl->map, sid;
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
	
	for (h = h0; h <= h1; h++) {
		sht = ctl->sheets[h];
		buf = sht->buf;
		sid = sht - ctl->sheets0;
		
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

void sheet_refreshmap(struct SHTCTL* ctl, int vx0, int vy0, int vx1, int vy1, int h0) {
	int h, bx, by, vx, vy, bx0, bx1, by0, by1;
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