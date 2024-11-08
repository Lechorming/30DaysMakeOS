#include "bootpack.h"
#include <stdio.h>

#define KEYCMD_LED		0xed

int keywin_off(struct SHEET *key_win, struct SHEET *sht_win, int cur_c, int cur_x);
int keywin_on(struct SHEET *key_win, struct SHEET *sht_win, int cur_c);


static char keytable0[0x80] = {
	0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 0, 0, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', 0, '\\', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0x5c, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x5c, 0, 0
	};

static char keytable1[0x80] = {
	0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0,
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, 0, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, '_', 0, 0, 0, 0, 0, 0, 0, 0, 0, '|', 0, 0
	};

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct SHTCTL *shtctl;
	char s[40];
	struct FIFO32 fifo, keycmd;
	int fifobuf[128], keycmd_buf[32];
	int mx, my, cursor_x, cursor_c, i, j, x, y, mmx = -1, mmy = -1;
	unsigned int memtotal;
	struct MOUSE_DEC mdec;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	unsigned char *buf_back, buf_mouse[256], *buf_win, *buf_cons;
	struct SHEET *sht_back, *sht_mouse, *sht_win, *sht_cons, *sht = 0, *key_win;
	struct TASK *task_a, *task_cons;
	struct TIMER *timer;
	int key_to = 0, key_shift = 0, key_leds = (binfo->leds >> 4) & 7, keycmd_wait = -1;
	struct CONSOLE *cons;


	init_gdtidt();					  		/* 初始化段和中断描述符,函数实现在dsctbl.c*/
	init_pic();						  		/* 初始化PIC(可编程中断控制器),函数实现在int.c */
	io_sti();						  		/* 解除CPU的中断屏蔽,函数实现在naskfunc.nas*/
	fifo32_init(&fifo, 128, fifobuf, 0);	/* 初始化中断数据缓冲区,函数实现在fifo.c */
	init_pit();						  		/* 初始化PIT */
	init_keyboard(&fifo, 256);		  		/* 初始化键盘的控制电路,激活键盘,开始接收键盘中断 */
	enable_mouse(&fifo, 512, &mdec);  		/* 激活鼠标，开始接收鼠标中断 */
	io_out8(PIC0_IMR, 0xf8);		  		/* 打开PIC1(2)和键盘的中断(1)和PIT(0)许可(11111000) */
	io_out8(PIC1_IMR, 0xef);		  		/* 打开鼠标的中断许可(11101111) */
	fifo32_init(&keycmd, 32, keycmd_buf, 0);

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
	
	/* 初始化console图层 */
	sht_cons = sheet_alloc(shtctl);
	buf_cons = (unsigned char *) memman_alloc_4k(memman, 256 * 165);
	sheet_setbuf(sht_cons, buf_cons, 256, 165, -1); /* 没有透明色 */
	make_window8(buf_cons, 256, 165, "console", 0);
	make_textbox8(sht_cons, 8, 28, 240, 128, COL8_000000);
	task_cons = task_alloc();
	task_cons->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 8;
	task_cons->tss.eip = (int) &console_task;
	task_cons->tss.es = 1 * 8;
	task_cons->tss.cs = 2 * 8;
	task_cons->tss.ss = 1 * 8;
	task_cons->tss.ds = 1 * 8;
	task_cons->tss.fs = 1 * 8;
	task_cons->tss.gs = 1 * 8;
	*((int *) (task_cons->tss.esp + 4)) = (int) sht_cons;
	*((int *) (task_cons->tss.esp + 8)) = memtotal; /*这里！ */
	task_run(task_cons, 2, 2); /* level=2, priority=2 */
	sht_cons->task = task_cons;
	sht_cons->flags |= 0x20;     /*有光标*/

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
	key_win = sht_win;

	/* 初始化鼠标图层 */
	sht_mouse = sheet_alloc(shtctl);												  // 为鼠标申请一个图层
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);									  /* 设置mouse图层buf指针和大小，透明色号99 */
	init_mouse_cursor8(buf_mouse, 99);												  /* 初始化鼠标的画面，背景色号99 */
	mx = (binfo->scrnx - 16) / 2;													  /* 计算画面中心坐标 */
	my = (binfo->scrny - 28 - 16) / 2;
	
	/* 设置各个图层的xy位置和高度 */
	sheet_slide(sht_back,  0,  0);
	sheet_slide(sht_cons, 32,  4);
	sheet_slide(sht_win,  64, 56);
	sheet_slide(sht_mouse, mx, my);
	sheet_updown(sht_back,  0);
	sheet_updown(sht_cons,  1);
	sheet_updown(sht_win,   2);
	sheet_updown(sht_mouse, 3);
	
	/* 为了避免和键盘当前状态冲突，在一开始先进行设置 */
	fifo32_put(&keycmd, KEYCMD_LED);
	fifo32_put(&keycmd, key_leds);

	/* 向内存中放入一些重要的地址 */
	*((int *)0x0fe4) = (int)shtctl;

	// 循环进行中断缓冲区处理
	for (;;)
	{
		/*如果存在向键盘控制器发送的数据，则发送它  */
		if (fifo32_status(&keycmd) > 0 && keycmd_wait < 0) 
		{
            keycmd_wait = fifo32_get(&keycmd);
            wait_KBC_sendready();
            io_out8(PORT_KEYDAT, keycmd_wait);
        }
		io_cli(); // 先暂停接收中断，防止获取keyfifo和mousefifo时数据被修改导致数据错乱，约等于加锁
		if (fifo32_status(&fifo) == 0) /* 数据缓冲区为空 */
		{
			task_sleep(task_a);
			io_sti();	/* 休眠并重新打开中断 */
		}
		else /* 数据缓冲区有数据 */
		{
			i = fifo32_get(&fifo);
			io_sti();
			if (key_win->flags == 0) /*输入窗口被关闭*/
			{
                key_win = shtctl->sheets[shtctl->top -1];
                cursor_c = keywin_on(key_win, sht_win, cursor_c);
            }
			if (256 <= i && i <= 511) /* 键盘数据*/
			{
				if (i < 0x80 + 256) /*将按键编码转换为字符编码*/
				{
					if (key_shift == 0)
					{
						s[0] = keytable0[i - 256];
					}
					else
					{
						s[0] = keytable1[i - 256];
					}
				}
				else
				{
					s[0] = 0;
				}
				if ('A' <= s[0] && s[0] <= 'Z') /* 如果是字母的话 */
				{
					if (((key_leds & 4) == 0 && key_shift == 0) ||
						((key_leds & 4) != 0 && key_shift != 0))
					{
						s[0] += 0x20; /* 进行大小写转换 */
					}
				}
				if (s[0] != 0) /*一般字符*/
				{
					if (key_win == sht_win) /*发送至任务A */
					{
						if (cursor_x < 128) /*显示一个字符并将光标后移一位*/
						{
							s[1] = 0;
							putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
							cursor_x += 8;
						}
					}
					else /*发送至命令行窗口*/
					{
						fifo32_put(&key_win->task->fifo, s[0] + 256);
					}
				}
				if (i == 256 + 0x0e) /*退格键*/
				{
					if (key_win == sht_win)
					{ /*发送至任务A */
						if (cursor_x > 8)
						{
							/*用空格擦除光标后将光标前移一位*/
						  putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
                          cursor_x -= 8;
						}
					}
					else
					{ /*发送至命令行窗口*/
						fifo32_put(&key_win->task->fifo, 8 + 256);
					}
				}
				if (i == 256 + 0x1c) /*回车键*/
				{
					if (key_win != sht_win)
					{ /*发送至命令行窗口*/
						fifo32_put(&key_win->task->fifo, 10 + 256);
					}
				}
				if (i == 256 + 0x0f) /* Tab键*/
				{
					cursor_c = keywin_off(key_win, sht_win, cursor_c, cursor_x);
					j = key_win->height - 1;
					if (j == 0)
					{
						j = shtctl->top - 1;
					}
					key_win = shtctl->sheets[j];
					cursor_c = keywin_on(key_win, sht_win, cursor_c);
				}
				if (i == 256 + 0x2a) /*左Shift ON */
				{
					key_shift |= 1;
				}
				if (i == 256 + 0x36) /*右Shift ON */
				{
					key_shift |= 2;
				}
				if (i == 256 + 0xaa) /*左Shift OFF */
				{
					key_shift &= ~1;
				}
				if (i == 256 + 0xb6) /*右Shift OFF */
				{
					key_shift &= ~2;
				}
				if (i == 256 + 0x3a) /* CapsLock */
				{
					key_leds ^= 4;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0x45) /* NumLock */
				{
					key_leds ^= 2;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0x46) /* ScrollLock */
				{
					key_leds ^= 1;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				if (i == 256 + 0xfa) /*键盘成功接收到数据*/
				{
					keycmd_wait = -1;
				}
				if (i == 256 + 0xfe) /*键盘没有成功接收到数据*/
				{
					wait_KBC_sendready();
					io_out8(PORT_KEYDAT, keycmd_wait);
				}
				if (i == 256 + 0x3b && key_shift != 0 && task_cons->tss.ss0 != 0) /* Shift+F1 */
				{
                    cons = (struct CONSOLE *) *((int *) 0x0fec);
                    cons_putstr0(cons, "\nBreak(key):\n");
                    io_cli();   /*不能在改变寄存器值时切换到其他任务*/
                    task_cons->tss.eax = (int) &(task_cons->tss.esp0);
                    task_cons->tss.eip = (int)asm_end_app;
                    io_sti();
                }
				if (i == 256 + 0x57 && shtctl->top > 2) /* F11 */
				{
                    sheet_updown(shtctl->sheets[1], shtctl->top -1);
				}
				if (cursor_c>=0) /* 光标再显示 */
				{
					boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				}
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			}
			else if (512 <= i && i <= 767) /* 鼠标数据*/
			{
				if (mouse_decode(&mdec, i - 512) != 0)
				{ 
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
					sheet_slide(sht_mouse, mx, my);
					if ((mdec.btn & 0x01) != 0) /* 按下了鼠标左键 */
					{
						if (mmx < 0)
						{
							/*如果处于通常模式*/
							/*按照从上到下的顺序寻找鼠标所指向的图层*/
							for (j = shtctl->top - 1; j > 0; j--)
							{
								sht = shtctl->sheets[j];
								x = mx - sht->vx0;
								y = my - sht->vy0;
								if (0 <= x && x < sht->bxsize && 0 <= y && y < sht->bysize)
								{
									if (sht->buf[y * sht->bxsize + x] != sht->col_inv)
									{
										sheet_updown(sht, shtctl->top - 1);
										
										if (sht != key_win)
										{
											cursor_c = keywin_off(key_win, sht_win, cursor_c, cursor_x);
											key_win = sht;
											cursor_c = keywin_on(key_win, sht_win, cursor_c);
										}
										/* 点击标题 */
										if (3 <= x && x < sht->bxsize - 3 && 3 <= y && y < 21)
										{
											mmx = mx; /*进入窗口移动模式*/
											mmy = my;
										}
										/*点击“×”按钮*/
										if (sht->bxsize -21 <= x && x < sht->bxsize -5 && 5 <= y && y < 19)
										{
											/*是由应用程序生成的窗口吗？*/
											if ((sht->flags & 0x10) != 0)
											{
												cons = (struct CONSOLE *)*((int *)0x0fec);
												cons_putstr0(cons, "\nBreak(mouse) :\n");
												io_cli(); /*强制结束处理中禁止切换任务*/
												task_cons->tss.eax = (int)&(task_cons->tss.esp0);
												task_cons->tss.eip = (int)asm_end_app;
												io_sti();
											}
										}
										break;
									}
								}
							}
						}
						else
						{
							/*如果处于窗口移动模式*/
							x = mx - mmx; /*计算鼠标的移动距离*/
							y = my - mmy;
							sheet_slide(sht, sht->vx0 + x, sht->vy0 + y);
							mmx = mx; /*更新为移动后的坐标*/
							mmy = my;
						}
					}
					else /*没有按下左键*/
					{
						mmx = -1;   /*返回通常模式*/
                    }
				}
			}
			else if (i <= 1) /* 光标用定时器*/
			{
				if (i != 0) {
                    timer_init(timer, &fifo, 0); /*下次置0 */
                    if (cursor_c >= 0) {
                      cursor_c = COL8_000000;
                    }
                } else {
                    timer_init(timer, &fifo, 1); /*下次置1 */
                    if (cursor_c >= 0) {
                      cursor_c = COL8_FFFFFF;
                    }
                }
                timer_settime(timer, 50);
                if (cursor_c >= 0) {
                    boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
                    sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
				}
			}
		}
	}
}

int keywin_off(struct SHEET *key_win, struct SHEET *sht_win, int cur_c, int cur_x)
{
    change_wtitle8(key_win, 0);
    if (key_win == sht_win) {
        cur_c = -1; /*删除光标*/
        boxfill8(sht_win->buf, sht_win->bxsize, COL8_FFFFFF, cur_x, 28, cur_x + 7, 43);
    } else {
        if ((key_win->flags & 0x20) != 0) {
            fifo32_put(&key_win->task->fifo, 3); /*命令行窗口光标OFF */
        }
    }
    return cur_c;
}

int keywin_on(struct SHEET *key_win, struct SHEET *sht_win, int cur_c)
{
    change_wtitle8(key_win, 1);
    if (key_win == sht_win) {
        cur_c = COL8_000000; /*显示光标*/
    } else {
        if ((key_win->flags & 0x20) != 0) {
            fifo32_put(&key_win->task->fifo, 2); /*命令行窗口光标ON */
        }
    }
    return cur_c;
}


