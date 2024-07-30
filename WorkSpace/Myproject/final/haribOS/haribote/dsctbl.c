/*GDT和IDT的descriptor table*/
#include "bootpack.h"

void init_gdtidt(void)
{
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;
	struct GATE_DESCRIPTOR *idt = (struct GATE_DESCRIPTOR *)ADR_IDT;
	int i;

	/* 初始化GDT */
	for (i = 0; i < LIMIT_GDT / 8; i++)
	{
		set_segmdesc(gdt + i, 0, 0, 0);
	}
	/* 内核能使用的数据段, 为整个内存，起始地址0, 限制4GB */
	set_segmdesc(gdt + 1, 0xffffffff, 0x00000000, AR_DATA32_RW);
	/* 内核代码段, 存放着bootpack.hrb程序, 即内核主程序 */
	set_segmdesc(gdt + 2, LIMIT_BOTPAK, ADR_BOTPAK, AR_CODE32_ER);
	load_gdtr(LIMIT_GDT, ADR_GDT);

	/* 初始化IDT */
	for (i = 0; i < LIMIT_IDT / 8; i++)
	{
		set_gatedesc(idt + i, 0, 0, 0);
	}
	load_idtr(LIMIT_IDT, ADR_IDT);

	/* 注册IDT */
	set_gatedesc(idt + 0x0c, (int)asm_inthandler0c, 2 << 3, AR_INTGATE32); /* 栈异常 */
	set_gatedesc(idt + 0x0d, (int)asm_inthandler0d, 2 << 3, AR_INTGATE32); /* 通常保护异常 */
	set_gatedesc(idt + 0x20, (int)asm_inthandler20, 2 << 3, AR_INTGATE32); /* 定时器中断 */
	set_gatedesc(idt + 0x21, (int)asm_inthandler21, 2 << 3, AR_INTGATE32); /* 键盘中断 */
	set_gatedesc(idt + 0x2c, (int)asm_inthandler2c, 2 << 3, AR_INTGATE32); /* 鼠标中断 */
	set_gatedesc(idt + 0x40, (int)asm_hrb_api, 2 << 3, AR_INTGATE32 + 0x60); /* 系统API中断 */

	return;
}

/* 将base, limit, AccessRight设置到段描述符sd中 */
void set_segmdesc(struct SEGMENT_DESCRIPTOR *sd, unsigned int limit, int base, int ar)
{
	/* 先检查limit是否大于1MB */
	if (limit > 0xfffff)
	{
		/* 大于1MB要将ar的G位置1 */
		ar |= 0x8000; /* G_bit = 1 */
		/* 寻址方式粒度变为页, 1页=4KB */
		/* limit要除以4KB */
		limit /= 0x1000;
	}
	/* limit低16位 */
	sd->limit_low = limit & 0xffff;
	/* base低16位 */
	sd->base_low = base & 0xffff;
	/* base中间8位 */
	sd->base_mid = (base >> 16) & 0xff;
	/* ar低8位 */
	sd->access_right = ar & 0xff;
	/* limit_high的低4位=limit的高4位, 高4位=ar的高4位. ar的中间4位舍弃 */
	sd->limit_high = ((limit >> 16) & 0x0f) | ((ar >> 8) & 0xf0);
	/* base的高8位 */
	sd->base_high = (base >> 24) & 0xff;
	return;
}

/* 将中断响应程序的offset, selector表示响应程序所属的段, AccessRight设置到中断描述符gd中 */
void set_gatedesc(struct GATE_DESCRIPTOR *gd, int offset, int selector, int ar)
{
	gd->offset_low = offset & 0xffff;
	gd->selector = selector;
	gd->dw_count = (ar >> 8) & 0xff;
	gd->access_right = ar & 0xff;
	gd->offset_high = (offset >> 16) & 0xffff;
	return;
}
