// PIT(programmable interval timer)
#include "bootpack.h"

#define PIT_CTRL 0x0043
#define PIT_CNT0 0x0040

// 定时器状态
#define TIMER_FLAGS_UNUSE 0 // 未使用
#define TIMER_FLAGS_ALLOC 1 // 已配置
#define TIMER_FLAGS_USING 2 // 运行中

struct TIMERCTL timerctl;

// AL=0x34, OUT(0x43, AL)
// AL=中断周期的低8位, OUT(0x40, AL)
// AL=中断周期的高8位, OUT(0x40, AL)
void init_pit() {
    int i;
    struct TIMER* t;

    io_out8(PIT_CTRL, 0x34);
    // 设置中断周期为11932, 中断频率就是100hz，即1s钟发生100次中断, 10ms一次
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);

    // 初始化数据
    timerctl.count = 0;
    for (i = 0; i < MAX_TIMER; i++) {
        timerctl.timers0[i].flags = TIMER_FLAGS_UNUSE;
    }
    t = timer_alloc(); // 取得一个
    t->timeout = 0xffffffff;
    t->flags = TIMER_FLAGS_USING;
    t->next = 0;

    timerctl.t0 = t;
    timerctl.next = 0xffffffff;
}

// 0x60+IRQ号码输出给OCW2表示某个IRQ处理完成
void inthandler20(int *esp)
{
    struct TIMER* timer;
    char ts = 0;

	io_out8(PIC0_OCW2, 0x60);	/* 通知PIC"IRQ-00"已经受理完成 */
    timerctl.count++;
    if (timerctl.next > timerctl.count) {
        return ; // 判断下一个时刻是否超时
    }

    timer = timerctl.t0;
    for (;;) {
        if (timer->timeout > timerctl.count) {
            break;
        }
        
        timer->flags = TIMER_FLAGS_ALLOC;
        if (timer != task_timer) {
            fifo32_put(timer->fifo, timer->data);
        }
        else {
            ts = 1; // task_timer超时
        }
        timer = timer->next;
    }

    // 有i个定时器超时了, 其余进行移位
    timerctl.t0 = timer;
    timerctl.next = timerctl.t0->timeout;

    if (ts != 0) {
        task_switch();
    }

	return;
}

struct TIMER* timer_alloc() {
    int i;
    for (i = 0; i < MAX_TIMER; i++) {
        if (timerctl.timers0[i].flags == TIMER_FLAGS_UNUSE) {
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
            timerctl.timers0[i].flags2 = 0;
            return &timerctl.timers0[i];
        }
    }
    return 0;
}

void timer_free(struct TIMER* timer) {
    timer->flags = TIMER_FLAGS_UNUSE;
}

void timer_init(struct TIMER* timer, struct FIFO32* fifo, int data) {
    timer->fifo = fifo;
    timer->data = data;
}

void timer_settime(struct TIMER* timer, unsigned int timeout) {
    int e;
    struct TIMER *t, *s;

    timer->timeout = timeout + timerctl.count;
    timer->flags = TIMER_FLAGS_USING;

    e = io_load_eflags(); // 记录中断许可标志
    io_cli(); // 禁止中断

    t = timerctl.t0; // head(可能是哨兵或者第一个真实元素)

    /* 构建链表过程 */
    // 插入这个比第一个还小，替换第一个
    if (timer->timeout <= t->timeout) {
        timerctl.t0 = timer;
        timer->next = t;
        timerctl.next = timer->timeout;

        io_store_eflags(e);
        return;
    }

    // 其他情况, 遍历链表找到合适位置，链表结尾是哨兵（timeout=oxffffffff）
    for (;;) {
        s = t;
        t = t->next;

        if (timer->timeout <= t->timeout) {
            s->next = timer;
            timer->next = t;

            io_store_eflags(e);
            return;
        }
    }
}


int timer_cancel(struct TIMER *timer)
{
	int e;
	struct TIMER *t;
	e = io_load_eflags();
	io_cli();	/* 设置过程中禁止改变定制器状态 */
	if (timer->flags == TIMER_FLAGS_USING) {	/* 是否要取消 */
		if (timer == timerctl.t0) {
			/* 取消第一个 */
			t = timer->next;
			timerctl.t0 = t;
			timerctl.next = t->timeout;
		} else {
			/* 非第一个定时器取消 */
			/* 找到timer前一个定时器 */
			t = timerctl.t0;
			for (;;) {
				if (t->next == timer) {
					break;
				}
				t = t->next;
			}
			t->next = timer->next; /* 删掉链表中的这个定时器 */
		}
		timer->flags = TIMER_FLAGS_ALLOC;
		io_store_eflags(e);
		return 1;	/* 取消成功 */
	}

	io_store_eflags(e);
	return 0; /* 不需要处理 */
}

void timer_cancelall(struct FIFO32 *fifo)
{
	int e, i;
	struct TIMER *t;
	e = io_load_eflags();
	io_cli();	/* 设置过程中禁止改变定时器状态 */
	for (i = 0; i < MAX_TIMER; i++) {
		t = &timerctl.timers0[i];
		if (t->flags != 0 && t->flags2 != 0 && t->fifo == fifo) {
			timer_cancel(t);
			timer_free(t);
		}
	}
	io_store_eflags(e);
	return;
}