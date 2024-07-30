#include "bootpack.h"

struct FIFO32 *keyfifo;
int keydata0;

void inthandler21(int *esp)
{
    int data;
    io_out8(PIC0_OCW2, 0x61);   /* 把IRQ-01接收信号结束的信息通知给PIC */
    data = io_in8(PORT_KEYDAT);
    fifo32_put(keyfifo, data + keydata0);
    return;
}

#define PORT_KEYSTA				0x0064  /* 键盘控制电路的设备号 */
#define KEYSTA_SEND_NOTREADY	0x02    /* 第2位数据 */
#define KEYCMD_WRITE_MODE		0x60    /* 模式设定的指令 */
#define KBC_MODE				0x47    /* 固定值，表示利用鼠标模式 */

void wait_KBC_sendready(void)
{
    /* 等待键盘控制电路准备完毕 */
    for (; ; ) {
        if ((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0) {
			/*设备号码0x0064处所读取的数据的倒数第二位（从低位开始数的第二位）是0时，表示键盘控制电路准备好了*/
            break;
        }
    }
    return;
}

void init_keyboard(struct FIFO32 *fifo, int data0)
{
    /* 将FIFO缓冲区的信息保存到全局变量里 */
    keyfifo = fifo;
    keydata0 = data0;
    /* 键盘控制器的初始化 */
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, KBC_MODE);
    return;
}

