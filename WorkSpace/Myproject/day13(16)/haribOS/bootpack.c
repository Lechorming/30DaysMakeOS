#include "bootpack.h"
#include <stdio.h>

void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act);
void putfonts8_asc_sht(struct SHEET *sht, int x, int y, int c, int b, char *s, int l);
void make_textbox8(struct SHEET *sht, int x0, int y0, int sx, int sy, int c);
void task_b_main(struct SHEET *sht_back);

static char keytable[0x54] = {
	0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 0, 0, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', 0, '\\', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.'};

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct FIFO32 fifo;
	char s[40];
	int fifobuf[128];
	int mx, my, i, cursor_x, cursor_c;
	unsigned int memtotal;
	struct MOUSE_DEC mdec;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHTCTL *shtctl;
	unsigned char *buf_back, buf_mouse[256], *buf_win, *buf_win_b;
	struct SHEET *sht_back, *sht_mouse, *sht_win, *sht_win_b[3];
	struct TASK *task_a, *task_b[3];
	struct TIMER *timer;

	init_gdtidt();					  		/* 初始化段和中断描述符,函数实现在dsctbl.c*/
	init_pic();						  		/* 初始化PIC(可编程中断控制器),函数实现在int.c */
	io_sti();						  		/* 解除CPU的中断屏蔽,函数实现在naskfunc.nas*/
	fifo32_init(&fifo, 128, fifobuf, 0);	/* 初始化中断数据缓冲区,函数实现在fifo.c */
	init_pit();						  		/* 初始化PIT */
	init_keyboard(&fifo, 256);		  		/* 初始化键盘的控制电路,激活键盘,开始接收键盘中断 */
	enable_mouse(&fifo, 512, &mdec);  		/* 激活鼠标，开始接收鼠标中断 */
	io_out8(PIC0_IMR, 0xf8);		  		/* 打开PIC1(2)和键盘的中断(1)和PIT(0)许可(11111000) */
	io_out8(PIC1_IMR, 0xef);		  		/* 打开鼠标的中断许可(11101111) */

	/* 初始化内存 */
	memtotal = memtest(0x00400000, 0xbfffffff);	 /* 获取内存总容量 */
	memman_init(memman);						 /* 初始化内存管理区 */
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000-0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000);

	/* 初始化graphic，函数实现在graphic.c */
	init_palette();																	  // 初始化调色板
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);			  // 申请空间并初始化
	task_a = task_init(memman);
	fifo.task = task_a;
	task_run(task_a, 1, 2);
	/* 初始化back图层 */
	sht_back = sheet_alloc(shtctl);													  /* 为背景申请一个图层 */
	buf_back = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny); /* 为back的buf申请内存 */
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);				  /* 设置back图层buf指针和大小，没有透明色 */
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);								  /* 初始化背景的画面 */
	/* 初始化所有图层b */
	for (i = 0; i < 3; i++) 
	{
	    sht_win_b[i] = sheet_alloc(shtctl);
	    buf_win_b = (unsigned char *) memman_alloc_4k(memman, 144 * 52);
	    sheet_setbuf(sht_win_b[i], buf_win_b, 144, 52, -1); /*无透明色*/
	    sprintf(s, "task_b%d", i);
	    make_window8(buf_win_b, 144, 52, s, 0);
	    task_b[i] = task_alloc();
	    task_b[i]->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024-8;
	    task_b[i]->tss.eip = (int) &task_b_main;
	    task_b[i]->tss.es = 1 * 8;
	    task_b[i]->tss.cs = 2 * 8;
	    task_b[i]->tss.ss = 1 * 8;
	    task_b[i]->tss.ds = 1 * 8;
	    task_b[i]->tss.fs = 1 * 8;
	    task_b[i]->tss.gs = 1 * 8;
	    *((int *) (task_b[i]->tss.esp + 4)) = (int) sht_win_b[i];
	    task_run(task_b[i], 2, i + 1);
	}
	/* 初始化windows图层 */
	sht_win = sheet_alloc(shtctl);									/* 为窗口申请一个图层 */
	buf_win = (unsigned char *)memman_alloc_4k(memman, 144 * 52);	/* 为图层win的buf申请内存 */
	sheet_setbuf(sht_win, buf_win, 144, 52, -1);					/* 设置win图层buf指针和大小，没有透明色 */
	make_window8(buf_win, 144, 52, "task_a", 1);					/* 画窗口标题 */
	make_textbox8(sht_win, 8, 28, 128, 16, COL8_FFFFFF);
	cursor_x = 8;													/* 闪烁光标的初始位置 */
	cursor_c = COL8_FFFFFF;											/* 闪烁光标的颜色 */
	timer = timer_alloc();		/* 初始化光标定时器 */
	timer_init(timer, &fifo, 1);
	timer_settime(timer, 50);
	/* 初始化鼠标图层 */
	sht_mouse = sheet_alloc(shtctl);												  // 为鼠标申请一个图层
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);									  /* 设置mouse图层buf指针和大小，透明色号99 */
	init_mouse_cursor8(buf_mouse, 99);												  /* 初始化鼠标的画面，背景色号99 */
	mx = (binfo->scrnx - 16) / 2;													  /* 计算画面中心坐标 */
	my = (binfo->scrny - 28 - 16) / 2;
	/* 设置各个图层的xy位置和高度 */
	sheet_slide(sht_back, 0, 0);
	sheet_slide(sht_win_b[0], 168,  56);
	sheet_slide(sht_win_b[1],   8, 116);
    sheet_slide(sht_win_b[2], 168, 116);
    sheet_slide(sht_win, 8, 56);
    sheet_slide(sht_mouse, mx, my);
    sheet_updown(sht_back, 0);
    sheet_updown(sht_win_b[0], 1);
    sheet_updown(sht_win_b[1], 2);
    sheet_updown(sht_win_b[2], 3);
    sheet_updown(sht_win, 4);
    sheet_updown(sht_mouse, 5);
	
	sprintf(s, "(%3d, %3d)", mx, my);	/* 输出鼠标位置数据 */
	putfonts8_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);
	sprintf(s, "memory %dMB   free : %dKB",
			memtotal / (1024 * 1024), memman_total(memman) / 1024); /* 内存使用情况信息 */
	putfonts8_asc_sht(sht_back, 0, 32, COL8_FFFFFF, COL8_008484, s, 40);

	// 循环进行中断缓冲区处理
	for (;;)
	{
		io_cli(); // 先暂停接收中断，防止获取keyfifo和mousefifo时数据被修改导致数据错乱，约等于加锁
		if (fifo32_status(&fifo) == 0)
		{
			task_sleep(task_a); // 重新打开中断并休眠
			io_sti();
		}
		else
		{
			i = fifo32_get(&fifo);
			io_sti();
			if (256 <= i && i <= 511) /* 键盘数据*/
			{
				sprintf(s, "%02X", i - 256);
				putfonts8_asc_sht(sht_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 2);
				if (i < 0x54 + 256)
				{
					/* 判断是否超出textbox范围 */
					if (keytable[i - 256] != 0 && cursor_x < 128)
					{ /* 一般字符 */
						/* 显示1个字符就前移1次光标 */
						s[0] = keytable[i - 256];
						s[1] = 0;
						putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
						cursor_x += 8;
					}
				}
				if (i == 256 + 0x0e && cursor_x > 8) /* 退格键  */
				{
					/* 用空格键把光标消去后，后移1次光标 */
					putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
					cursor_x -= 8;
            	}
				/* 光标再显示 */
				boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			}
			else if (512 <= i && i <= 767) /* 鼠标数据*/
			{
				if (mouse_decode(&mdec, i - 512) != 0)
				{ /* 已经收集了3字节的数据，所以显示出来 */
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					if ((mdec.btn & 0x01) != 0)
					{
						s[1] = 'L';
					}
					if ((mdec.btn & 0x02) != 0)
					{
						s[3] = 'R';
					}
					if ((mdec.btn & 0x04) != 0)
					{
						s[2] = 'C';
					}
					putfonts8_asc_sht(sht_back, 32, 16, COL8_FFFFFF, COL8_008484, s, 15);
					/* 鼠标指针的移动 */
					mx += mdec.x;
					my += mdec.y;
					if (mx < 0)
					{
						mx = 0;
					}
					if (my < 0)
					{
						my = 0;
					}
					if (mx > binfo->scrnx - 1)
					{
						mx = binfo->scrnx - 1;
					}

					if (my > binfo->scrny - 1)
					{
						my = binfo->scrny - 1;
					}
					sprintf(s, "(%3d, %3d)", mx, my);
					putfonts8_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);
					sheet_slide(sht_mouse, mx, my);
					if ((mdec.btn & 0x01) != 0)
					{
                        /* 按下左键、移动sht_win  */
						sheet_slide(sht_win, mx -80, my -8);
					}

				}
			}
			else if (i <= 1) /* 光标用定时器*/
			{
				if(i!=0)
				{
					timer_init(timer, &fifo, 0); /* 下面是设定0 */
					cursor_c = COL8_000000;
				}
				else
				{
					timer_init(timer, &fifo, 1); /* 下面是设定1 */
					cursor_c = COL8_FFFFFF;
				}
				timer_settime(timer, 50);
				boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			}
			
		}
	}
}

void make_textbox8(struct SHEET *sht, int x0, int y0, int sx, int sy, int c)
{
    int x1 = x0 + sx, y1 = y0 + sy;
	boxfill8(sht->buf, sht->bxsize, COL8_848484, x0 - 2, y0 - 3, x1 + 1, y0 - 3);
	boxfill8(sht->buf, sht->bxsize, COL8_848484, x0 - 3, y0 - 3, x0 - 3, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, COL8_FFFFFF, x0 - 3, y1 + 2, x1 + 1, y1 + 2);
	boxfill8(sht->buf, sht->bxsize, COL8_FFFFFF, x1 + 2, y0 - 3, x1 + 2, y1 + 2);
	boxfill8(sht->buf, sht->bxsize, COL8_000000, x0 - 1, y0 - 2, x1 + 0, y0 - 2);
	boxfill8(sht->buf, sht->bxsize, COL8_000000, x0 - 2, y0 - 2, x0 - 2, y1 + 0);
	boxfill8(sht->buf, sht->bxsize, COL8_C6C6C6, x0 - 2, y1 + 1, x1 + 0, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, COL8_C6C6C6, x1 + 1, y0 - 2, x1 + 1, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, c,           x0 - 1, y0 - 1, x1 + 0, y1 + 0);
    return;
}

void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act)
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
		"@@@@@@@@@@@@@@@@"};
	int x, y;
	char c, tc, tbc;
	if (act != 0) {
        tc = COL8_FFFFFF;
        tbc = COL8_000084;
    } else {
        tc = COL8_C6C6C6;
        tbc = COL8_848484;
    }

	boxfill8(buf, xsize, COL8_C6C6C6, 0,         0,         xsize - 1, 0        );
	boxfill8(buf, xsize, COL8_FFFFFF, 1,         1,         xsize - 2, 1        );
	boxfill8(buf, xsize, COL8_C6C6C6, 0,         0,         0,         ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1,         1,         1,         ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1,         xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0,         xsize - 1, ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2,         2,         xsize - 3, ysize - 3);
	boxfill8(buf, xsize, tbc,         3,         3,         xsize - 4, 20       );
	boxfill8(buf, xsize, COL8_848484, 1,         ysize - 2, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, 0,         ysize - 1, xsize - 1, ysize - 1);
	putfonts8_asc(buf, xsize, 24, 4, tc, title);
	for (y = 0; y < 14; y++)
	{
		for (x = 0; x < 16; x++)
		{
			c = closebtn[y][x];
			if (c == '@')
			{
				c = COL8_000000;
			}
			else if (c == '$')
			{
				c = COL8_848484;
			}
			else if (c == 'Q')
			{
				c = COL8_C6C6C6;
			}
			else
			{
				c = COL8_FFFFFF;
			}
			buf[(5 + y) * xsize + (xsize - 21 + x)] = c;
		}
	}
	return;
}

void putfonts8_asc_sht(struct SHEET *sht, int x, int y, int c, int b, char *s, int l)
{ /* x,y显示位置的坐标, c字符颜色, b背景颜色, s字符串, l字符串长度 */
	boxfill8(sht->buf, sht->bxsize, b, x, y, x + l * 8 - 1, y + 15);
	putfonts8_asc(sht->buf, sht->bxsize, x, y, c, s);
	sheet_refresh(sht, x, y, x + l * 8, y + 16);
	return;
}

void task_b_main(struct SHEET *sht_win_b)
{
    struct FIFO32 fifo;
    struct TIMER *timer_1s;
    int i, fifobuf[128], count = 0, count0 = 0;
    char s[12];

    fifo32_init(&fifo, 128, fifobuf, 0);
    timer_1s = timer_alloc();
    timer_init(timer_1s, &fifo, 100);
    timer_settime(timer_1s, 100);

    for (; ; ) {
        count++;
        io_cli();
        if (fifo32_status(&fifo) == 0) {
            io_sti();
        } else {
            i = fifo32_get(&fifo);
            io_sti();
            if (i == 100) {
                sprintf(s, "%11d", count - count0);
                putfonts8_asc_sht(sht_win_b, 24, 28, COL8_000000, COL8_C6C6C6, s, 11);
                count0 = count;
                timer_settime(timer_1s, 100);
            }
        }
    }
}

