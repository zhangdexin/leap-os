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
    io_out8(PIT_CTRL, 0x34);
    // 设置中断周期为11932, 中断频率就是100hz，即1s钟发生100次中断, 10ms一次
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);

    // 初始化数据
    timerctl.count = 0;
    timerctl.next = 0xffffffff;
    timerctl.using = 0;
    for (i = 0; i < MAX_TIMER; i++) {
        timerctl.timers0[i].flags = TIMER_FLAGS_UNUSE;
    }
}

// 0x60+IRQ号码输出给OCW2表示某个IRQ处理完成
void inthandler20(int *esp)
{
    int i, j;
	io_out8(PIC0_OCW2, 0x60);	/* 通知PIC"IRQ-00"已经受理完成 */
    timerctl.count++;
    if (timerctl.next > timerctl.count) {
        return ; // 判断下一个时刻是否超时
    }

    for (i = 0; i < timerctl.using; i++) {
        if (timerctl.timers[i]->timeout > timerctl.count) {
            break;
        }
        
        timerctl.timers[i]->flags = TIMER_FLAGS_ALLOC;
        fifo8_put(timerctl.timers[i]->fifo, timerctl.timers[i]->data);
    }

    // 有i个定时器超时了, 其余进行移位
    timerctl.using -= i;
    for (j = 0; j < timerctl.using; j++) {
        timerctl.timers[j] = timerctl.timers[i + j];
    }

    if (timerctl.using > 0) {
        timerctl.next = timerctl.timers[0]->timeout;
    }
    else {
        timerctl.next = 0xffffffff;
    }

	return;
}

struct TIMER* timer_alloc() {
    int i;
    for (i = 0; i < MAX_TIMER; i++) {
        if (timerctl.timers0[i].flags == TIMER_FLAGS_UNUSE) {
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
            return &timerctl.timers0[i];
        }
    }
    return 0;
}

void timer_free(struct TIMER* timer) {
    timer->flags = TIMER_FLAGS_UNUSE;
}

void timer_init(struct TIMER* timer, struct FIFO8* fifo, unsigned char data) {
    timer->fifo = fifo;
    timer->data = data;
}

void timer_settime(struct TIMER* timer, unsigned int timeout) {
    int e, i, j;
    
    timer->timeout = timeout + timerctl.count;
    timer->flags = TIMER_FLAGS_USING;

    e = io_load_eflags(); // 记录中断许可标志
    io_cli(); // 禁止中断

    // 搜索注册位置
    for (i = 0; i < timerctl.using; i++) {
        if (timerctl.timers[i]->timeout >= timer->timeout) {
            break;
        }
    }

    for (j = timerctl.using; j > i; j--) {
        timerctl.timers[j] = timerctl.timers[j - 1];
    }
    timerctl.using++;

    // 插入新的timer
    timerctl.timers[i] = timer;
    timerctl.next = timerctl.timers[0]->timeout;

    io_store_eflags(e);
}

// void settimer(unsigned int timeout, struct FIFO8* fifo, unsigned char data) {
//     int eflags;
//     eflags = io_load_eflags(); // 记录中断许可标志
//     io_cli(); // 禁止中断

//     timerctl.timeout = timeout;
//     timerctl.fifo = fifo;
//     timerctl.data = data;

//     io_store_eflags(eflags);
// }