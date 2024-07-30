#include "bootpack.h"

struct FIFO32 *mousefifo;
int mousedata0;

/* 来自PS/2鼠标的中断 */
void inthandler2c(int *esp)
{
    int data;
    io_out8(PIC1_OCW2, 0x64);   /* 把IRQ-12接收信号结束的信息通知给PIC1 */
    io_out8(PIC0_OCW2, 0x62);   /* 把IRQ-02接收信号结束的信息通知给PIC0 */
    data = io_in8(PORT_KEYDAT);
    fifo32_put(mousefifo, data + mousedata0);
    return;
}

#define KEYCMD_SENDTO_MOUSE		0xd4 //初始化鼠标用，向键盘控制电路PORT_KEYCMD发送该数据，下一数据会自动发送给鼠标
#define MOUSECMD_ENABLE			0xf4 //向PORT_KEYDAT发送该数据，鼠标就会被激活，并返回ACK(0xfa)

/*如果往键盘控制电路发送指令0xd4，下一个数据就会自动发送给鼠标。我们根据这一特性来发送激活鼠标的指令。*/
void enable_mouse(struct FIFO32 *fifo, int data0, struct MOUSE_DEC *mdec)
{
    /* 将FIFO缓冲区的信息保存到全局变量里 */
    mousefifo = fifo;
    mousedata0 = data0;
    /* 鼠标有效 */
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
    /* 顺利的话，ACK(0xfa)会被发送*/
    mdec->phase = 0; /* 等待鼠标的0xfa的阶段*/
    return;
}

/* 鼠标解码程序，结果输出在mdec */
int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat)
{
	if (mdec->phase == 0) {
		/* 等待鼠标的0xfa的状态 */
		if (dat == 0xfa) {
			mdec->phase = 1;
		}
		return 0;
	}
	if (mdec->phase == 1) {
		/* 等待鼠标的第一字节 */
		if((dat & 0xc8) == 0x08) {
			mdec->buf[0] = dat;
			mdec->phase = 2;
		}
		return 0;
	}
	if (mdec->phase == 2) {
		/* 等待鼠标的第二字节 */
		mdec->buf[1] = dat;
		mdec->phase = 3;
		return 0;
	}
	if (mdec->phase == 3) {
		/* 等待鼠标的第三字节 */
		mdec->buf[2] = dat;
		mdec->phase = 1;
		mdec->btn = mdec->buf[0] & 0x07;
		mdec->x = mdec->buf[1];
		mdec->y = mdec->buf[2];
		if((mdec->buf[0] & 0x10) != 0) {
			mdec->x |= 0xffffff00;
		}
		if((mdec->buf[0] & 0x20) != 0) {
			mdec->y |= 0xffffff00;
		}
		mdec->y = -mdec->y;
		return 1;
	}
	return -1; /* 正常情况下不会跑到这里 */
}

