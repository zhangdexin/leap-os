/* 割り込み関係 */

#include "bootpack.h"
#include <stdio.h>

// PIC的寄存器是8位
// IMR(interrupt mask register)中断屏蔽寄存器，某一个置?1，?个IRQ信号屏蔽
// ICW(initial control word)初始化控制数据，ICW有四个，ICW1和ICW4与PIC主板配?方式，中断信号?气特性相?，?置?固定?
// ICW3有?主从?接?定，第三个即IRQ2?接从PIC
// ICW2决定IRQ以?一号中断通知CPU
// INT 0x00~0x0f用以保障?用程序?操作系?干坏事，直接触??中断
// IRQ1是??，IRQ12是鼠?
void init_pic(void)
/* PIC的初始化 */
{
	io_out8(PIC0_IMR,  0xff  ); /* 禁止所有中断 */
	io_out8(PIC1_IMR,  0xff  ); /* 禁止所有中断 */

	io_out8(PIC0_ICW1, 0x11  ); /* ?沿触?模式 */
	io_out8(PIC0_ICW2, 0x20  ); /* IRQ0-7由INT20-27接收 */
	io_out8(PIC0_ICW3, 1 << 2); /* PIC0与IRQ2?接?定 */
	io_out8(PIC0_ICW4, 0x01  ); /* 无?冲模式 */

	io_out8(PIC1_ICW1, 0x11  ); /* ?沿触?模式 */
	io_out8(PIC1_ICW2, 0x28  ); /* IRQ8-15由INT28-2f接收 */
	io_out8(PIC1_ICW3, 2     ); /* PIC1与IRQ2?接?定 */
	io_out8(PIC1_ICW4, 0x01  ); /* 无?无?冲模式 */

	io_out8(PIC0_IMR,  0xfb  ); /* 11111011 PIC1以外全部禁止 */
	io_out8(PIC1_IMR,  0xff  ); /* 11111111 禁止所有中断 */

	return;
}

void inthandler27(int *esp)
/* PIC0からの不完全割り込み対策 */
/* Athlon64X2機などではチップセットの都合によりPICの初期化時にこの割り込みが1度だけおこる */
/* この割り込み処理関数は、その割り込みに対して何もしないでやり過ごす */
/* なぜ何もしなくていいの？
	→  この割り込みはPIC初期化時の電気的なノイズによって発生したものなので、
		まじめに何か処理してやる必要がない。									*/
{
	io_out8(PIC0_OCW2, 0x67); /* IRQ-07受付完了をPICに通知 */
	return;
}
