#include "bootpack.h"
#include <stdio.h>

extern struct FIFO8 keyfifo;
extern struct FIFO8 mousefifo;
void enable_mouse(void);
void init_keyboard(void);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) 0x0ff0;
	char s[40], mcursor[256], keybuf[32], mousebuf[128];
	int mx, my, i;

	init_gdtidt();
	init_pic();
	io_sti(); /*解除CPU的中断屏蔽*/
	
	fifo8_init(&keyfifo, 32, keybuf);
	io_out8(PIC0_IMR, 0xf9); /* 打开PIC1和键盘的中断许可(11111001) */
	io_out8(PIC1_IMR, 0xef); /* 打开鼠标的中断许可(11101111) */

	init_palette();
	init_screen8(binfo->vram, binfo->scrnx, binfo->scrny);
	mx = (binfo->scrnx - 16) / 2; /* 画面中心坐标 */
	my = (binfo->scrny - 28 - 16) / 2;
	init_mouse_cursor8(mcursor, COL8_008484);
	putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16);
	sprintf(s, "(%d, %d)", mx, my);
	putfonts8_asc(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, s);
	
	init_keyboard();
	enable_mouse();
	fifo8_init(&mousefifo, 128, mousebuf);
	for (; ; ) {
		io_cli();
		if (fifo8_status(&keyfifo) + fifo8_status(&mousefifo) == 0) {
			io_stihlt();
		} else {
			if (fifo8_status(&keyfifo) != 0) {
				i = fifo8_get(&keyfifo);
				io_sti();
				sprintf(s, "%02X", i);
				boxfill8(binfo->vram, binfo->scrnx, COL8_008484,  0, 16, 15, 31);
				putfonts8_asc(binfo->vram, binfo->scrnx, 0, 16, COL8_FFFFFF, s);
			} else if (fifo8_status(&mousefifo) != 0) {
				i = fifo8_get(&mousefifo);
				io_sti();
				sprintf(s, "%02X", i);
				boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 32, 16, 47, 31);
				putfonts8_asc(binfo->vram, binfo->scrnx, 32, 16, COL8_FFFFFF, s);
			}
		}
	}
}

#define PORT_KEYDAT               0x0060 //键盘的端口号
#define PORT_KEYSTA               0x0064 //键盘控制电路的设备号 IN用
#define PORT_KEYCMD               0x0064 //键盘控制电路的设备号 OUT用
#define KEYSTA_SEND_NOTREADY     0x02 //第2位数据
#define KEYCMD_WRITE_MODE        0x60 //模式设定的指令
#define KBC_MODE                  0x47 //固定值，表示利用鼠标模式

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

#define KEYCMD_SENDTO_MOUSE      0xd4
#define MOUSECMD_ENABLE          0xf4
/*如果往键盘控制电路发送指令0xd4，下一个数据就会自动发送给鼠标。我们根据这一特性来发送激活鼠标的指令。*/
void enable_mouse(void)
{
    /* 激活鼠标 */
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
    return; /* 顺利的话，键盘控制其会返送回ACK(0xfa)*/
}
