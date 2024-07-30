#include "bootpack.h"

struct FIFO8 keyfifo;

void inthandler21(int *esp)
{
    unsigned char data;
    io_out8(PIC0_OCW2, 0x61); /* 通知PIC，说IRQ-01的受理已经完成 */
    data = io_in8(PORT_KEYDAT);
    fifo8_put(&keyfifo, data);
    return;
}

#define PORT_KEYSTA				0x0064 //键盘控制电路的设备号
#define KEYSTA_SEND_NOTREADY	0x02 //第2位数据
#define KEYCMD_WRITE_MODE		0x60 //模式设定的指令
#define KBC_MODE				0x47 //固定值，表示利用鼠标模式

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

void init_keyboard(void)
{
    /* 初始化键盘控制电路 */
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, KBC_MODE);
    return;
}

