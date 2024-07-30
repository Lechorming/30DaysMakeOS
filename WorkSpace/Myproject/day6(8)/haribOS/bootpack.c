#include "bootpack.h"
#include <stdio.h>

struct MOUSE_DEC {
	unsigned char buf[3], phase;
	int x, y, btn;
};

extern struct FIFO8 keyfifo, mousefifo;
void enable_mouse(struct MOUSE_DEC *mdec);
void init_keyboard(void);
int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40], mcursor[256], keybuf[32], mousebuf[128];
	int mx, my, i;
	struct MOUSE_DEC mdec;

	init_gdtidt(); /*初始化段和中断描述符,函数实现在dsctbl.c*/
	init_pic(); //初始化PIC(可编程中断控制器),函数实现在int.c
	io_sti(); /*解除CPU的中断屏蔽,函数实现在naskfunc.nas*/
	fifo8_init(&keyfifo, 32, keybuf); //初始化键盘的中断数据缓冲区，函数实现在fifo.c
	fifo8_init(&mousefifo, 128, mousebuf); //初始化鼠标的中断数据缓冲区
	io_out8(PIC0_IMR, 0xf9); /* 打开PIC1和键盘的中断许可(11111001) */
	io_out8(PIC1_IMR, 0xef); /* 打开鼠标的中断许可(11101111) */
	
	init_keyboard(); //初始化键盘的控制电路,激活键盘,开始接收键盘中断

	//初始化graphic，函数实现在graphic.c
	init_palette(); //初始化调色板
	init_screen8(binfo->vram, binfo->scrnx, binfo->scrny); //初始化要显示的画面
	mx = (binfo->scrnx - 16) / 2; /* 计算画面中心坐标 */
	my = (binfo->scrny - 28 - 16) / 2;
	init_mouse_cursor8(mcursor, COL8_008484); //初始化鼠标
	putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16); //将鼠标画在屏幕上
	sprintf(s, "(%d, %d)", mx, my); //鼠标位置数据
	putfonts8_asc(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, s); //将鼠标位置数据显示在屏幕上
	
	enable_mouse(&mdec); //激活鼠标，开始接收鼠标中断
	
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
				if(mouse_decode(&mdec, i) != 0) {
					/* 鼠标的3个字节都齐了，显示出来 */
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					if((mdec.btn & 0x01) != 0) {
						s[1] = 'L';
					}
					if((mdec.btn & 0x02) != 0) {
						s[3] = 'R';
					}
					if((mdec.btn & 0x04) != 0) {
						s[2] = 'C';
					}
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 32, 16, 32 + 15 * 8-1, 31);
					putfonts8_asc(binfo->vram, binfo->scrnx, 32, 16, COL8_FFFFFF, s);
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, mx, my, mx + 15, my + 15);
					mx += mdec.x;
					my += mdec.y;
					if(mx < 0) {
						mx = 0;
					}
					if(my < 0) {
						my = 0;
					}
					if(mx > binfo->scrnx - 16) {
						mx = binfo->scrnx - 16;
					}
					if(my > binfo->scrny - 16) {
						my = binfo->scrny - 16;
					}
					sprintf(s, "(%3d, %3d)", mx, my);
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 0, 79, 15); // 隐藏坐标
					putfonts8_asc(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, s); // 显示坐标
					putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16); //绘画鼠标
				}
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
void enable_mouse(struct MOUSE_DEC *mdec)
{
	/* 激活鼠标 */
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	/* 顺利的话，键盘控制器会返送回ACK(0xfa) */
	mdec->phase = 0; /* 等待鼠标的0xfa信号 */
	return;
}

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


