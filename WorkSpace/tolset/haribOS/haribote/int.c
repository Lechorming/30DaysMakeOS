#include "bootpack.h"

void init_pic(void)
/* PIC的初始化 */
{
    io_out8(PIC0_IMR,  0xff  ); /* 禁止所有中断 */
    io_out8(PIC1_IMR,  0xff  ); /* 禁止所有中断 */

    io_out8(PIC0_ICW1, 0x11  ); /* 边沿触发模式（edge trigger mode）*/
    io_out8(PIC0_ICW2, 0x20  ); /* IRQ0-7由INT20-27接收 */
    io_out8(PIC0_ICW3, 1 << 2); /* PIC1由IRQ2连接 */
    io_out8(PIC0_ICW4, 0x01  ); /* 无缓冲区模式 */

    io_out8(PIC1_ICW1, 0x11  ); /* 边沿触发模式（edge trigger mode）*/
    io_out8(PIC1_ICW2, 0x28  ); /* IRQ8-15由INT28-2f接收 */
    io_out8(PIC1_ICW3, 2      ); /* PIC1由IRQ2连接 */
    io_out8(PIC1_ICW4, 0x01  ); /* 无缓冲区模式 */

    io_out8(PIC0_IMR,  0xfb  ); /* 11111011 PIC1以外全部禁止 */
    io_out8(PIC1_IMR,  0xff  ); /* 11111111 禁止所有中断 */

    return;
}

/* 栈异常 */
int *inthandler0c(int *esp)
{
    struct TASK *task = task_now();
    struct CONSOLE *cons = task->cons;
    char s[30];
    cons_putstr0(cons, "\nINT 0C :\n Stack Exception.\n");
    sprintf(s, "EIP = %08X\n", esp[11]);
    cons_putstr0(cons, s);
    return &(task->tss.esp0); /*强制结束程序*/
}

/* 通用保护异常 */
int *inthandler0d(int *esp)
{
    struct TASK *task = task_now();
    struct CONSOLE *cons = task->cons;
    char s[30];
    cons_putstr0(cons, "\nINT 0D :\n General Protected Exception.\n");
    sprintf(s, "EIP = %08X\n", esp[11]);
    cons_putstr0(cons, s);
    return &(task->tss.esp0); /*强制结束程序*/
}

