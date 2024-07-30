#include "bootpack.h"
#include <stdio.h>

#define MEMMAN_FREES		4090	/* 大约32KB */
#define MEMMAN_ADDR		  0x003c0000

struct FREEINFO {	/* 可用信息 */
	unsigned int addr, size;
};

struct MEMMAN {		/* 内存管理 */
	int frees, maxfrees, lostsize, losts;
	struct FREEINFO free[MEMMAN_FREES];
};

unsigned int memman_total(struct MEMMAN *man);
void memman_init(struct MEMMAN *man);
unsigned int memman_alloc(struct MEMMAN *man, unsigned int size);
int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size);
unsigned int memtest(unsigned int start, unsigned int end);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40], mcursor[256], keybuf[32], mousebuf[128];
	int mx, my, i;
	struct MOUSE_DEC mdec;
	unsigned int memtotal;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;

	init_gdtidt(); /*初始化段和中断描述符,函数实现在dsctbl.c*/
	init_pic(); //初始化PIC(可编程中断控制器),函数实现在int.c
	io_sti(); /*解除CPU的中断屏蔽,函数实现在naskfunc.nas*/
	fifo8_init(&keyfifo, 32, keybuf); //初始化键盘的中断数据缓冲区，函数实现在fifo.c
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
	init_screen8(binfo->vram, binfo->scrnx, binfo->scrny); //初始化要显示的画面
	mx = (binfo->scrnx - 16) / 2; /* 计算画面中心坐标 */
	my = (binfo->scrny - 28 - 16) / 2;
	init_mouse_cursor8(mcursor, COL8_008484); //初始化鼠标
	putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16); //将鼠标画在屏幕上
	sprintf(s, "(%d, %d)", mx, my); //鼠标位置数据
	putfonts8_asc(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, s); //将鼠标位置数据显示在屏幕上
	
	//sprintf(s, "memory %dMB   free : %dKB",
	//memtotal / (1024 * 1024), memman_total(memman) / 1024); //内存使用情况信息
	//putfonts8_asc(binfo->vram, binfo->scrnx, 0, 32, COL8_FFFFFF, s); //将内存使用情况显示出来
	
	sprintf(s, "memory %dMB   free : %dKB",
	memtotal / (1024 * 1024), memman_total(memman) / 1024); //内存使用情况信息
	putfonts8_asc(binfo->vram, binfo->scrnx, 0, 32, COL8_FFFFFF, s); //将内存使用情况显示出来
	
	// 循环进行中断缓冲区处理
	for (; ; ) {
		io_cli(); // 先暂停接收中断，防止获取keyfifo和mousefifo时数据被修改导致数据错乱，约等于加锁
		if (fifo8_status(&keyfifo) + fifo8_status(&mousefifo) == 0) {
			io_stihlt(); // 重新打开中断并睡眠
		} else {
			if (fifo8_status(&keyfifo) != 0) {
				i = fifo8_get(&keyfifo);
				io_sti(); // 数据处理完了，重新打开中断
				sprintf(s, "%02X", i);
				boxfill8(binfo->vram, binfo->scrnx, COL8_008484,  0, 16, 15, 31);
				putfonts8_asc(binfo->vram, binfo->scrnx, 0, 16, COL8_FFFFFF, s);
			} else if (fifo8_status(&mousefifo) != 0) {
				i = fifo8_get(&mousefifo);
				io_sti(); // 数据处理完了，重新打开中断
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

#define EFLAGS_AC_BIT		0x00040000
#define CR0_CACHE_DISABLE	0x60000000

unsigned int memtest(unsigned int start, unsigned int end) 
// 测试内存是否可用
{
	char flg486 = 0;
	unsigned int eflg, cr0, i;
	/* 确认CPU是386还是486以上的 */
	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT; /* AC-bit = 1 */
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if ((eflg & EFLAGS_AC_BIT) != 0) { /* 如果是386，即使设定AC=1, AC的值还会自动回到0 */
		flg486 = 1;
	}
	eflg &= ~EFLAGS_AC_BIT; /* AC-bit = 0 */
	io_store_eflags(eflg);

	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; /* 禁止缓存 */
		store_cr0(cr0);
	}

	i = memtest_sub(start, end);

	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; /* 允许缓存 */
		store_cr0(cr0);
	}
	return i;
}

void memman_init(struct MEMMAN *man)
// 初始化内存管理信息，将内存设为全部不可用
{
	man->frees = 0;			/* 可用信息数目 */
	man->maxfrees = 0;		/* 用于观察可用状况：frees的最大值  */
	man->lostsize = 0;		/* 释放失败的内存的大小总和  */
	man->losts = 0;			/* 释放失败次数 */
	return;
}

unsigned int memman_total(struct MEMMAN *man)
/* 报告空余内存大小的合计 */
{
	unsigned int i, t = 0;
	for (i = 0; i < man->frees; i++) {
		t += man->free[i].size;
	}
	return t;
}

unsigned int memman_alloc(struct MEMMAN *man, unsigned int size)
/* 分配 */
{
	unsigned int i, a;
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].size >= size) {
			/* 找到了足够大的内存 */
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0) {
				/* 如果free[i]变成了0，就减掉一条可用信息 */
				man->frees--;
				for (; i < man->frees; i++) {
					man->free[i] = man->free[i + 1]; /* 代入结构体 */
				}
			}
			return a;
		}
	}
	return 0; /* 没有可用空间 */
}

int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size)
/* 释放 */
{
	int i, j;
	/* 为便于归纳内存，将free[]按照addr的顺序排列 */
	/* 所以，先决定应该放在哪里 */
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].addr > addr) {
			break;
		}
	}
	/* free[i -1].addr < addr < free[i].addr */
	if (i > 0) {
		/* 前面有可用内存 */
		if (man->free[i -1].addr + man->free[i -1].size == addr) {
			/* 可以与前面的可用内存归纳到一起 */
			man->free[i -1].size += size;
			if (i < man->frees) {
				/* 后面也有 */
				if (addr + size == man->free[i].addr) {
					/* 也可以与后面的可用内存归纳到一起 */
					man->free[i -1].size += man->free[i].size;
					/* man->free[i]删除  */
					/* free[i]变成0后归纳到前面去 */
					man->frees--;
					for (; i < man->frees; i++) {
					  man->free[i] = man->free[i + 1]; /* 结构体赋值 */
					}
				}
			}
			return 0; /* 成功完成 */
		}
	}
	/* 不能与前面的可用空间归纳到一起 */
	if (i < man->frees) {
		/* 后面还有 */
		if (addr + size == man->free[i].addr) {
			/* 可以与后面的内容归纳到一起 */
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0; /* 成功完成 */
		}
	}
	/* 既不能与前面归纳到一起，也不能与后面归纳到一起 */
	if (man->frees < MEMMAN_FREES) {
		/* free[i]之后的，向后移动，腾出一点可用空间 */
		for (j = man->frees; j > i; j--) {
			man->free[j] = man->free[j -1];
		}
		man->frees++;
		if (man->maxfrees < man->frees) {
			man->maxfrees = man->frees; /* 更新最大值 */
		}
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0; /* 成功完成 */
	}
	/* 不能往后移动 */
	man->losts++;
	man->lostsize += size;
	return -1; /* 失败 */
}

