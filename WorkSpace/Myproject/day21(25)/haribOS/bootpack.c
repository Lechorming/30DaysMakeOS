#include "bootpack.h"
#include <stdio.h>

#define KEYCMD_LED		0xed

void keywin_off(struct SHEET *key_win);
void keywin_on(struct SHEET *key_win);

static char keytable0[0x80] = {
	0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0x08, 0,
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 0x0a, 0, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', 0, '\\', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0x5c, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x5c, 0, 0};

static char keytable1[0x80] = {
	0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0x08, 0,
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0x0a, 0, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, '_', 0, 0, 0, 0, 0, 0, 0, 0, 0, '|', 0, 0};

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct SHTCTL *shtctl;
	char s[40];
	struct FIFO32 fifo, keycmd;
	int fifobuf[128], keycmd_buf[32], *cons_fifo[2];
	int mx, my, i;
	unsigned int memtotal;
	struct MOUSE_DEC mdec;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	unsigned char *buf_back, buf_mouse[256], *buf_cons[2];
	struct SHEET *sht_back, *sht_mouse, *sht_cons[2];
	struct TASK *task_a, *task_cons[2], *task;
	int key_shift = 0, key_leds = (binfo->leds >> 4) & 7, keycmd_wait = -1;
	int j, x, y, mmx = -1, mmy = -1;
	struct SHEET *sht = 0, *key_win;


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
	/* 向内存中放入一些重要的地址 */
	*((int *)0x0fe4) = (int)shtctl;

	/* 初始化back图层 */
	sht_back = sheet_alloc(shtctl);													  /* 为背景申请一个图层 */
	buf_back = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny); /* 为back的buf申请内存 */
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);				  /* 设置back图层buf指针和大小，没有透明色 */
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);								  /* 初始化背景的画面 */
	
	/* 初始化console图层 */
    for (i = 0; i < 2; i++) {
        sht_cons[i] = sheet_alloc(shtctl);
        buf_cons[i] = (unsigned char *) memman_alloc_4k(memman, 256 * 165);
        sheet_setbuf(sht_cons[i], buf_cons[i], 256, 165, -1); /*没有透明色*/
        make_window8(buf_cons[i], 256, 165, "console", 0);
        make_textbox8(sht_cons[i], 8, 28, 240, 128, COL8_000000);
        task_cons[i] = task_alloc();
        task_cons[i]->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024-12;
        task_cons[i]->tss.eip = (int) &console_task;
        task_cons[i]->tss.es = 1 * 8;
        task_cons[i]->tss.cs = 2 * 8;
        task_cons[i]->tss.ss = 1 * 8;
        task_cons[i]->tss.ds = 1 * 8;
        task_cons[i]->tss.fs = 1 * 8;
        task_cons[i]->tss.gs = 1 * 8;
        *((int *) (task_cons[i]->tss.esp + 4)) = (int) sht_cons[i];
        *((int *) (task_cons[i]->tss.esp + 8)) = memtotal;
        task_run(task_cons[i], 2, 2); /* level=2, priority=2 */
        sht_cons[i]->task = task_cons[i];
        sht_cons[i]->flags |= 0x20; /*有光标*/
		cons_fifo[i] = (int *) memman_alloc_4k(memman, 128 * 4);              /*从此开始*/
        fifo32_init(&task_cons[i]->fifo, 128, cons_fifo[i], task_cons[i]);  /*到此结束*/
    }

	/* 初始化鼠标图层 */
	sht_mouse = sheet_alloc(shtctl);												  // 为鼠标申请一个图层
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);									  /* 设置mouse图层buf指针和大小，透明色号99 */
	init_mouse_cursor8(buf_mouse, 99);												  /* 初始化鼠标的画面，背景色号99 */
	mx = (binfo->scrnx - 16) / 2;													  /* 计算画面中心坐标 */
	my = (binfo->scrny - 28 - 16) / 2;
	
	/* 设置各个图层的xy位置和高度 */
	sheet_slide(sht_back,  0,  0);
	sheet_slide(sht_cons[1], 56,  6);
	sheet_slide(sht_cons[0],  8,  2);
	sheet_slide(sht_mouse, mx, my);
	sheet_updown(sht_back,		0);
    sheet_updown(sht_cons[1],	1);
    sheet_updown(sht_cons[0],	2);
    sheet_updown(sht_mouse,     3);
	key_win = sht_cons[0];
	keywin_on(key_win);

	
	/* 为了避免和键盘当前状态冲突，在一开始先进行设置 */
	fifo32_put(&keycmd, KEYCMD_LED);
	fifo32_put(&keycmd, key_leds);

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
                keywin_on(key_win);
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
                    fifo32_put(&key_win->task->fifo, s[0] + 256);
				}
				if (i == 256 + 0x0f) /* Tab键*/
				{
                    keywin_off(key_win);
                    j = key_win->height -1;
                    if (j == 0) {
                        j = shtctl->top -1;
                    }
                    key_win = shtctl->sheets[j];
					keywin_on(key_win);
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
				if (i == 256 + 0x3b &&key_shift != 0 && task_cons[0]->tss.ss0 != 0) /* Shift+F1 */
				{
                    task = key_win->task;
                    if (task != 0 && task->tss.ss0 != 0) {  /* Shift+F1 */
                      cons_putstr0(task->cons, "\nBreak(key) :\n");
                      io_cli();   /*强制结束处理时禁止任务切换*/
                      task->tss.eax = (int) &(task->tss.esp0);
                      task->tss.eip = (int) asm_end_app;
                      io_sti();
					}
				}
				if (i == 256 + 0x57 && shtctl->top > 2) /* F11 */
				{
                    sheet_updown(shtctl->sheets[1], shtctl->top -1);
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
											keywin_off(key_win);
											key_win = sht;
											keywin_on(key_win);
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
											/*是否由应用程序生成的窗口*/
											if ((sht->flags & 0x10) != 0)
											{
												task = sht->task;
												cons_putstr0(task->cons, "\nBreak(mouse) :\n");
												io_cli(); /*强制结束处理时禁止任务切换*/
												task->tss.eax = (int)&(task->tss.esp0);
												task->tss.eip = (int)asm_end_app;
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
		}
	}
}

void keywin_off(struct SHEET *key_win)
{
    change_wtitle8(key_win, 0);
    if ((key_win->flags & 0x20) != 0) {
        fifo32_put(&key_win->task->fifo, 3); /*命令行窗口光标OFF */
    }
    return;                                                                                /*到此结束*/
}

void keywin_on(struct SHEET *key_win)                                                  /*从此开始*/
{
    change_wtitle8(key_win, 1);
    if ((key_win->flags & 0x20) != 0) {
        fifo32_put(&key_win->task->fifo, 2); /*命令行窗口光标ON */
    }
    return;                                                                                /*到此结束*/
}


