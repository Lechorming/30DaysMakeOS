#include "bootpack.h"
#include <stdio.h>

void make_window8(unsigned char *buf, int xsize, int ysize, char *title);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40], keybuf[32], mousebuf[128];
	int mx, my, i;
	struct MOUSE_DEC mdec;
	unsigned int memtotal, count=0;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHTCTL *shtctl;
    struct SHEET *sht_back, *sht_mouse, *sht_win;
    unsigned char *buf_back, buf_mouse[256], *buf_win;

	init_gdtidt(); /*初始化段和中断描述符,函数实现在dsctbl.c*/
	init_pic(); //初始化PIC(可编程中断控制器),函数实现在int.c
	io_sti(); /*解除CPU的中断屏蔽,函数实现在naskfunc.nas*/
	fifo8_init(&keyfifo, 32, keybuf); //初始化键盘的中断数据缓冲区,函数实现在fifo.c
	fifo8_init(&mousefifo, 128, mousebuf); //初始化鼠标的中断数据缓冲区
	io_out8(PIC0_IMR, 0xf9); /* 打开PIC1和键盘的中断许可(11111001) */
	io_out8(PIC1_IMR, 0xef); /* 打开鼠标的中断许可(11101111) */
	
	init_keyboard(); //初始化键盘的控制电路,激活键盘,开始接收键盘中断
	enable_mouse(&mdec); //激活鼠标，开始接收鼠标中断
	
	// 初始化内存
	memtotal = memtest(0x00400000, 0xbfffffff); //获取内存总容量
	memman_init(memman); //初始化内存管理区
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000-0x0009efff */
	memman_free(memman, 0x00400000, memtotal -0x00400000);

	//初始化graphic，函数实现在graphic.c
	init_palette(); //初始化调色板
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny); //申请空间并初始化
	sht_back	= sheet_alloc(shtctl); //为背景申请一个图层
	sht_mouse	= sheet_alloc(shtctl); //为鼠标申请一个图层
	sht_win		= sheet_alloc(shtctl); //为窗口申请一个图层
	buf_back	= (unsigned char *) memman_alloc_4k(memman, binfo->scrnx * binfo->scrny); //为back的buf申请内存
	buf_win		= (unsigned char *) memman_alloc_4k(memman, 160 * 52);  //为图层win的buf申请内存
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1); /* 设置back图层buf指针和大小，没有透明色 */
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); /* 设置mouse图层buf指针和大小，透明色号99 */
	sheet_setbuf(sht_win, buf_win, 160, 52, -1); //设置win图层buf指针和大小，没有透明色
	init_screen8(buf_back, binfo->scrnx, binfo->scrny); //初始化要显示的画面
	init_mouse_cursor8(buf_mouse, 99); //初始化鼠标的画面，背景色号99
	
	make_window8(buf_win, 160, 68, "counter"); //画窗口标题
	//putfonts8_asc(buf_win, 160, 24, 28, COL8_000000, "Welcome to");
	//putfonts8_asc(buf_win, 160, 24, 44, COL8_000000, "  Haribote-OS!");
	
	sheet_slide(sht_back, 0, 0); //设置背景的位置(左上角)
	mx = (binfo->scrnx - 16) / 2; /* 计算画面中心坐标 */
	my = (binfo->scrny - 28 - 16) / 2;
	sheet_slide(sht_mouse, mx, my); //设置鼠标的位置(中心)
	sheet_slide(sht_win, 80, 72); //设置win的左上角位置
	sheet_updown(sht_back,	0); //设置背景的高度
	sheet_updown(sht_win,	1); //设置窗口的高度
	sheet_updown(sht_mouse,	2); //设置鼠标的高度
	
	sprintf(s, "(%d, %d)", mx, my); //鼠标位置数据
	putfonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s); //将鼠标位置数据画在back图层上
	
	sprintf(s, "memory %dMB   free : %dKB",
			memtotal / (1024 * 1024), memman_total(memman) / 1024); //内存使用情况信息
	putfonts8_asc(buf_back, binfo->scrnx, 0, 32, COL8_FFFFFF, s); //将内存使用情况画在back图层上
	sheet_refresh(sht_back, 0, 0, binfo->scrnx, 48);

	// 循环进行中断缓冲区处理
	for (; ; ) {
		count++;
		sprintf(s, "%010d", count);
		boxfill8(buf_win,160, COL8_C6C6C6, 40, 28, 119, 43);
		putfonts8_asc(buf_win, 160, 40, 28,  COL8_000000, s);
		sheet_refresh(sht_win, 40, 28, 120, 44);
		io_cli(); // 先暂停接收中断，防止获取keyfifo和mousefifo时数据被修改导致数据错乱，约等于加锁
		if (fifo8_status(&keyfifo) + fifo8_status(&mousefifo) == 0) {
			io_sti(); // 重新打开中断
		} else {
			if (fifo8_status(&keyfifo) != 0) {
				i = fifo8_get(&keyfifo);
				io_sti(); // 数据处理完了，重新打开中断
				sprintf(s, "%02X", i);
				boxfill8(buf_back, binfo->scrnx, COL8_008484,  0, 16, 15, 31); 
				putfonts8_asc(buf_back, binfo->scrnx, 0, 16, COL8_FFFFFF, s); //将数据画在back图层上
				sheet_refresh(sht_back, 0, 16, 16, 32);
			} else if (fifo8_status(&mousefifo) != 0) {
				i = fifo8_get(&mousefifo);
				io_sti(); // 数据处理完了，重新打开中断
				if(mouse_decode(&mdec, i) != 0) {
					/* 鼠标的3个字节都齐了，显示出来 */
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y); //先将数据写进内存
					if((mdec.btn & 0x01) != 0) {
						s[1] = 'L';
					}
					if((mdec.btn & 0x02) != 0) {
						s[3] = 'R';
					}
					if((mdec.btn & 0x04) != 0) {
						s[2] = 'C';
					}
					boxfill8(buf_back, binfo->scrnx, COL8_008484, 32, 16, 32 + 15 * 8-1, 31); //消除旧的LCR
					putfonts8_asc(buf_back, binfo->scrnx, 32, 16, COL8_FFFFFF, s); // 写新的LCR
					sheet_refresh(sht_back, 32, 16, 32 + 15 * 8, 32);
					// 显示鼠标的移动
					mx += mdec.x;
					my += mdec.y;
					if(mx < 0) {
						mx = 0;
					}
					if(my < 0) {
						my = 0;
					}
					if(mx > binfo->scrnx - 1) {
						mx = binfo->scrnx - 1;
					}
					if(my > binfo->scrny - 1) {
						my = binfo->scrny - 1;
					}
					sprintf(s, "(%3d, %3d)", mx, my); //将数据写进内存
					boxfill8(buf_back, binfo->scrnx, COL8_008484, 0, 0, 79, 15); // 消除旧的坐标
					putfonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s); // 显示新的坐标
					sheet_refresh(sht_back, 0, 0, 80, 16);
					sheet_slide(sht_mouse, mx, my);
				}
			}
		}
	}
}


void make_window8(unsigned char *buf, int xsize, int ysize, char *title)
{
    static char closebtn[14][16] = {
        "OOOOOOOOOOOOOOO@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQ@@QQQQ@@QQ$@",
        "OQQQQ@@QQ@@QQQ$@",
        "OQQQQQ@@@@QQQQ$@",
        "OQQQQQQ@@QQQQQ$@",
        "OQQQQQ@@@@QQQQ$@",
        "OQQQQ@@QQ@@QQQ$@",
        "OQQQ@@QQQQ@@QQ$@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQQQQQQQQQQQ$@",
        "O$$$$$$$$$$$$$$@",
        "@@@@@@@@@@@@@@@@"
    };
    int x, y;
    char c;
    boxfill8(buf, xsize, COL8_C6C6C6, 0,		0,			xsize -1,	0		);
    boxfill8(buf, xsize, COL8_FFFFFF, 1,		1,			xsize -2,	1		);
    boxfill8(buf, xsize, COL8_C6C6C6, 0,		0,			0,			ysize -1);
    boxfill8(buf, xsize, COL8_FFFFFF, 1,		1,			1,			ysize -2);
    boxfill8(buf, xsize, COL8_848484, xsize -2, 1,			xsize -2,	ysize -2);
    boxfill8(buf, xsize, COL8_000000, xsize -1, 0,			xsize -1,	ysize -1);
    boxfill8(buf, xsize, COL8_C6C6C6, 2,		2,			xsize -3,	ysize -3);
    boxfill8(buf, xsize, COL8_000084, 3,		3,			xsize -4,	20		);
    boxfill8(buf, xsize, COL8_848484, 1,		ysize -2,	xsize -2,	ysize -2);
    boxfill8(buf, xsize, COL8_000000, 0,		ysize -1,	xsize -1,	ysize -1);
    putfonts8_asc(buf, xsize, 24, 4, COL8_FFFFFF, title);
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 16; x++) {
            c = closebtn[y][x];
            if (c == '@') {
                c = COL8_000000;
            } else if (c == '$') {
                c = COL8_848484;
            } else if (c == 'Q') {
                c = COL8_C6C6C6;
            } else {
                c = COL8_FFFFFF;
            }
            buf[(5 + y) * xsize + (xsize -21 + x)] = c;
        }
    }
    return;
}

