#include "bootpack.h"
#include <stdio.h>
#include <string.h>

void console_task(struct SHEET *sheet, int memtotal)
{
    struct TASK *task = task_now();
    struct MEMMAN *memman = (struct MEMMAN *)MEMMAN_ADDR;
    int i, *fat = (int *)memman_alloc_4k(memman, 4 * 2880);
    struct CONSOLE cons;
    struct FILEHANDLE fhandle[8];
    char cmdline[30];
    
    cons.sht = sheet;
    cons.cur_x = 8;
    cons.cur_y = 28;
    cons.cur_c = -1;
    task->cons = &cons;
    task->cmdline = cmdline;

    if (cons.sht != 0)
    {
        cons.timer = timer_alloc();
        timer_init(cons.timer, &task->fifo, 1);
        timer_settime(cons.timer, 50);
    }
    file_readfat(fat, (unsigned char *)(ADR_DISKIMG + 0x000200));
    for (i = 0; i < 8; i++)
    {
        fhandle[i].buf = 0; /*未使用标记*/
    }
    task->fhandle = fhandle;
    task->fat = fat;

    /*显示提示符*/
    cons_putchar(&cons, '>', 1);

    for (;;)
    {
        io_cli();
        if (fifo32_status(&task->fifo) == 0)
        {
            task_sleep(task);
            io_sti();
        }
        else
        {
            i = fifo32_get(&task->fifo);
            io_sti();
            if (i <= 1 && cons.sht != 0) /*光标用定时器*/
            {
                if (i != 0)
                {
                    timer_init(cons.timer, &task->fifo, 0); /*下次置0 */
                    if (cons.cur_c >= 0)
                    {
                        cons.cur_c = COL8_FFFFFF;
                    }
                }
                else
                {
                    timer_init(cons.timer, &task->fifo, 1); /*下次置1 */
                    if (cons.cur_c >= 0)
                    {
                        cons.cur_c = COL8_000000;
                    }
                }
                timer_settime(cons.timer, 50);
            }
            if (i == 2) /*光标ON */
            {
                cons.cur_c = COL8_FFFFFF;
            }
            if (i == 3) /*光标OFF */
            {
                if (cons.sht != 0)
                {
                    boxfill8(cons.sht->buf, cons.sht->bxsize, COL8_000000, 
                            cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
                }
                cons.cur_c = -1;
            }
            if (i == 4) /* 点击命令行窗口的“×”按钮 */
            {
                cmd_exit(&cons, fat);
            }
            if (256 <= i && i <= 511) /*键盘数据（通过任务A）*/
            {
                if (i == 8 + 256) /*退格键*/
                {
                    if (cons.cur_x > 16)
                    {
                        /*用空格擦除光标后将光标前移一位*/
                        cons_putchar(&cons, ' ', 0);
                        cons.cur_x -= 8;
                    }
                }
                else if (i == 10 + 256) /*回车键*/
                {
                    /*将光标用空格擦除后换行 */
                    cons_putchar(&cons, ' ', 0);
                    cmdline[cons.cur_x / 8 - 2] = 0;
                    cons_newline(&cons);
                    cons_runcmd(cmdline, &cons, fat, memtotal); /*运行命令*/
                    if (cons.sht == 0)
                    {
                        cmd_exit(&cons, fat);
                    }
                    /*显示提示符*/
                    cons_putchar(&cons, '>', 1);
                }
                else /*一般字符*/
                {
                    if (cons.cur_x < 240)
                    {
                        /*显示一个字符之后将光标后移一位*/
                        cmdline[cons.cur_x / 8 - 2] = i - 256;
                        cons_putchar(&cons, i - 256, 1);
                    }
                }
            }
            /*重新显示光标*/
            if (cons.sht != 0)
            {
                if (cons.cur_c >= 0)
                {
                    boxfill8(cons.sht->buf, cons.sht->bxsize, cons.cur_c, 
                            cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
                }
                sheet_refresh(cons.sht, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y + 16);
            }
        }
    }
}

void cons_putchar(struct CONSOLE *cons, int chr, char move)
{
    char s[2];
    s[0] = chr;
    s[1] = 0;
    if (s[0] == 0x09) /*制表符*/
    {
        for (;;)
        {
            if (cons->sht != 0)
            {
                putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
            }
            cons->cur_x += 8;
            if (cons->cur_x == 8 + 240)
            {
                cons_newline(cons);
            }
            if (((cons->cur_x - 8) & 0x1f) == 0)
            {
                break; /*被32整除则break*/
            }
        }
    }
    else if (s[0] == 0x0a) /*换行*/
    {
        cons_newline(cons);
    }
    else if (s[0] == 0x0d) /*回车*/
    {
        /*先不做任何操作*/
    }
    else /*一般字符*/
    {
        if (cons->sht != 0)
        {
            putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
        }
        if (move != 0)
        {
            /* move为0时光标不后移*/
            cons->cur_x += 8;
            if (cons->cur_x == 8 + 240)
            {
                cons_newline(cons);
            }
        }
    }
    return;
}

/* 换行函数 */
void cons_newline(struct CONSOLE *cons)
{
    int x, y;
    struct SHEET *sheet = cons->sht;
    if (cons->cur_y < 28 + 112)
    {
        cons->cur_y += 16; /*到下一行*/
    }
    else
    {
        if (sheet != 0)
        {
            /*滚动*/
            for (y = 28; y < 28 + 112; y++)
            {
                for (x = 8; x < 8 + 240; x++)
                {
                    sheet->buf[x + y * sheet->bxsize] = sheet->buf[x + (y + 16) * sheet->bxsize];
                }
            }
            for (y = 28 + 112; y < 28 + 128; y++)
            {
                for (x = 8; x < 8 + 240; x++)
                {
                    sheet->buf[x + y * sheet->bxsize] = COL8_000000;
                }
            }
            sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
        }
    }
    cons->cur_x = 8;
    return;
}

void cons_putstr0(struct CONSOLE *cons, char *s)
{
    for (; *s != 0; s++)
    {
        cons_putchar(cons, *s, 1);
    }
    return;
}

void cons_putstr1(struct CONSOLE *cons, char *s, int l)
{
    int i;
    for (i = 0; i < l; i++)
    {
        cons_putchar(cons, s[i], 1);
    }
    return;
}

void cons_runcmd(char *cmdline, struct CONSOLE *cons, int *fat, int memtotal)
{
    if (strcmp(cmdline, "mem") == 0)
    {
        cmd_mem(cons, memtotal);
    }
    else if (strcmp(cmdline, "cls") == 0)
    {
        cmd_cls(cons);
    }
    else if (strcmp(cmdline, "dir") == 0)
    {
        cmd_dir(cons);
    }
    else if (strcmp(cmdline, "exit") == 0)
    {
        cmd_exit(cons, fat);
    }
    else if (strncmp(cmdline, "start ", 6) == 0)
    {
        cmd_start(cons, cmdline, memtotal);
    }
    else if (strncmp(cmdline, "ncst ", 5) == 0)
    {
        cmd_ncst(cons, cmdline, memtotal);
    }
    else if (cmdline[0] != 0)
    {
        if (cmd_app(cons, fat, cmdline) == 0) /*不是命令，也不是空行*/
        {
            cons_putstr0(cons, "Bad command. \n\n");
        }
    }
    return;
}

void cmd_mem(struct CONSOLE *cons, int memtotal)
{

    struct MEMMAN *memman = (struct MEMMAN *)MEMMAN_ADDR;
    char s[60]; /*从此开始*/
    sprintf(s, "total   %dMB\nfree %dKB\n\n", memtotal / (1024 * 1024), memman_total(memman) / 1024);
    cons_putstr0(cons, s); /*到此结束*/
    return;
}

void cmd_cls(struct CONSOLE *cons)
{
    int x, y;
    struct SHEET *sheet = cons->sht;
    for (y = 28; y < 28 + 128; y++)
    {
        for (x = 8; x < 8 + 240; x++)
        {
            sheet->buf[x + y * sheet->bxsize] = COL8_000000;
        }
    }
    sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
    cons->cur_y = 28;
    return;
}

void cmd_dir(struct CONSOLE *cons)
{
    struct FILEINFO *finfo = (struct FILEINFO *)(ADR_DISKIMG + 0x002600);
    int i, j;
    char s[30];
    for (i = 0; i < 224; i++)
    {
        if (finfo[i].name[0] == 0x00)
        {
            break;
        }
        if (finfo[i].name[0] != 0xe5)
        {
            if ((finfo[i].type & 0x18) == 0)
            {
                sprintf(s, "filename.ext   %7d\n", finfo[i].size);
                for (j = 0; j < 8; j++)
                {
                    s[j] = finfo[i].name[j];
                }
                s[9] = finfo[i].ext[0];
                s[10] = finfo[i].ext[1];
                s[11] = finfo[i].ext[2];
                cons_putstr0(cons, s);
            }
        }
    }
    cons_newline(cons);
    return;
}

int cmd_app(struct CONSOLE *cons, int *fat, char *cmdline)
{
    struct MEMMAN *memman = (struct MEMMAN *)MEMMAN_ADDR;
    struct FILEINFO *finfo;
    char name[18], *p, *q;
    struct TASK *task = task_now();
    int i, segsiz, datsiz, esp, dathrb;
    struct SHTCTL *shtctl;
    struct SHEET *sht;

    /*根据命令行生成文件名*/
    for (i = 0; i < 13; i++)
    {
        if (cmdline[i] <= ' ')
        {
            break;
        }
        name[i] = cmdline[i];
    }
    name[i] = 0; /*暂且将文件名的后面置为0*/

    /*寻找文件 */
    finfo = file_search(name, (struct FILEINFO *)(ADR_DISKIMG + 0x002600), 224);
    if (finfo == 0 && name[i - 1] != '.')
    {
        /*由于找不到文件，故在文件名后面加上“.hrb”后重新寻找*/
        name[i] = '.';
        name[i + 1] = 'H';
        name[i + 2] = 'R';
        name[i + 3] = 'B';
        name[i + 4] = 0;
        finfo = file_search(name, (struct FILEINFO *)(ADR_DISKIMG + 0x002600), 224);
    }

    if (finfo != 0)
    {
        /*找到文件的情况*/
        p = (char *)memman_alloc_4k(memman, finfo->size); /* 申请代码段的内存空间 */
        file_loadfile(finfo->clustno, finfo->size, p, fat, (char *)(ADR_DISKIMG + 0x003e00));
        if (finfo->size >= 36 && strncmp(p + 4, "Hari", 4) == 0 && *p == 0x00)
        {
            segsiz = *((int *)(p + 0x0000));
            esp = *((int *)(p + 0x000c));
            datsiz = *((int *)(p + 0x0010));
            dathrb = *((int *)(p + 0x0014));
            q = (char *)memman_alloc_4k(memman, segsiz); /* 申请数据段的内存空间 */
            task->ds_base = (int)q;
            /* 向内存段管理注册应用程序的代码段和数据段 */
            set_segmdesc(task->ldt + 0, finfo->size - 1, (int)p, AR_CODE32_ER + 0x60);
            set_segmdesc(task->ldt + 1, segsiz - 1, (int)q, AR_DATA32_RW + 0x60);
            for (i = 0; i < datsiz; i++)
            {
                q[esp + i] = p[dathrb + i];
            }
            start_app(0x1b, 0 * 8 + 4, esp, 1 * 8 + 4, &(task->tss.esp0));
            shtctl = (struct SHTCTL *)*((int *)0x0fe4);
            for (i = 0; i < MAX_SHEETS; i++)
            {
                sht = &(shtctl->sheets0[i]);
                if ((sht->flags & 0x11) == 0x11 && sht->task == task)
                {
                    /*找到被应用程序遗留的窗口*/
                    sheet_free(sht); /*关闭*/
                }
            }
            for (i = 0; i < 8; i++) /*将未关闭的文件关闭*/
            {
                if (task->fhandle[i].buf != 0)
                {
                    memman_free_4k(memman, (int) task->fhandle[i].buf, task->fhandle[i].size);
                    task->fhandle[i].buf = 0;
                }
            } 
            timer_cancelall(&task->fifo);
            memman_free_4k(memman, (int)q, segsiz);
        }
        else
        {
            cons_putstr0(cons, ".hrb file format error.\n");
        }
        memman_free_4k(memman, (int)p, finfo->size);
        cons_newline(cons);
        return 1;
    }
    /*没有找到文件的情况*/
    return 0;
}

void cmd_exit(struct CONSOLE *cons, int *fat)
{
    struct MEMMAN *memman = (struct MEMMAN *)MEMMAN_ADDR;
    struct TASK *task = task_now();
    struct SHTCTL *shtctl = (struct SHTCTL *)*((int *)0x0fe4);
    struct FIFO32 *fifo = (struct FIFO32 *)*((int *)0x0fec);
    timer_cancel(cons->timer);
    memman_free_4k(memman, (int)fat, 4 * 2880);
    io_cli();
    if (cons->sht != 0)
    {
        fifo32_put(fifo, cons->sht - shtctl->sheets0 + 768); /* 768～1023 */
    }
    else
    {
        /* taskctl已定义在头文件 */
        fifo32_put(fifo, task - taskctl->tasks0 + 1024); /*1024～2023*/
    }
    io_sti();
    for (;;)
    {
        task_sleep(task);
    }
}

void cmd_start(struct CONSOLE *cons, char *cmdline, int memtotal)
{
    struct SHTCTL *shtctl = (struct SHTCTL *)*((int *)0x0fe4);
    struct SHEET *sht = open_console(shtctl, memtotal);
    struct FIFO32 *fifo = &sht->task->fifo;
    int i;
    sheet_slide(sht, 32, 4);
    sheet_updown(sht, shtctl->top);
    /*将命令行输入的字符串逐字复制到新的命令行窗口中*/
    for (i = 6; cmdline[i] != 0; i++)
    {
        fifo32_put(fifo, cmdline[i] + 256);
    }
    fifo32_put(fifo, 10 + 256); /*回车键*/
    cons_newline(cons);
    return;
}

void cmd_ncst(struct CONSOLE *cons, char *cmdline, int memtotal)
{
    struct TASK *task = open_constask(0, memtotal);
    struct FIFO32 *fifo = &task->fifo;
    int i;
    /*将命令行输入的字符串逐字复制到新的命令行窗口中*/
    for (i = 5; cmdline[i] != 0; i++)
    {
        fifo32_put(fifo, cmdline[i] + 256);
    }
    fifo32_put(fifo, 10 + 256); /*回车键*/
    cons_newline(cons);
    return;
}

int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
    int i;
    struct TASK *task = task_now();
    int ds_base = task->ds_base;
    struct CONSOLE *cons = task->cons;
    struct SHTCTL *shtctl = (struct SHTCTL *)*((int *)0x0fe4);
    struct SHEET *sht;
    struct FIFO32 *sys_fifo = (struct FIFO32 *)*((int *)0x0fec);
    struct FILEINFO *finfo;
    struct FILEHANDLE *fh;
    struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
    int *reg = &eax + 1; /* eax后面的地址*/
    /*强行改写通过PUSHAD保存的值*/
    /* reg[0] : EDI,   reg[1] : ESI,   reg[2] : EBP,   reg[3] : ESP */
    /* reg[4] : EBX,   reg[5] : EDX,   reg[6] : ECX,   reg[7] : EAX */ /*到此结束*/

    if (edx == 1) /* 输出字符api */
    {
        cons_putchar(cons, eax & 0xff, 1);
    }
    else if (edx == 2) /* 输出字符串0api */
    {
        cons_putstr0(cons, (char *)ebx + ds_base);
    }
    else if (edx == 3) /* 输出字符串1api */
    {
        cons_putstr1(cons, (char *)ebx + ds_base, ecx);
    }
    else if (edx == 4) /* 关闭应用程序api */
    {
        return &(task->tss.esp0);
    }
    else if (edx == 5) /* 新建窗口api */
    {
        sht = sheet_alloc(shtctl);
        sht->task = task;
        sht->flags |= 0x10;
        sheet_setbuf(sht, (char *)ebx + ds_base, esi, edi, eax);
        make_window8((char *)ebx + ds_base, esi, edi, (char *)ecx + ds_base, 0);
        sheet_slide(sht, ((shtctl->xsize - esi) / 2) & ~3, (shtctl->ysize - edi) / 2); /* 使显示窗口的起始位置的X坐标为4的倍数 */
        sheet_updown(sht, shtctl->top);                                                /*将窗口图层高度指定为当前鼠标所在图层的高度，鼠标移到上层*/
        reg[7] = (int)sht;
    }
    else if (edx == 6) /* 在窗口上显示字符api */
    {
        sht = (struct SHEET *)(ebx & 0xfffffffe);
        putfonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *)ebp + ds_base);
        if ((ebx & 1) == 0)
        {
            sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
        }
    }
    else if (edx == 7) /* 在窗口上显示方块api */
    {
        sht = (struct SHEET *)(ebx & 0xfffffffe);
        boxfill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
        if ((ebx & 1) == 0)
        {
            sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
        }
    }
    else if (edx == 8) /* 初始化memman */
    {
        memman_init((struct MEMMAN *)(ebx + ds_base));
        ecx &= 0xfffffff0; /*以16字节为单位*/
        memman_free((struct MEMMAN *)(ebx + ds_base), eax, ecx);
    }
    else if (edx == 9) /* malloc */
    {
        ecx = (ecx + 0x0f) & 0xfffffff0; /*以16字节为单位进位取整*/
        reg[7] = memman_alloc((struct MEMMAN *)(ebx + ds_base), ecx);
    }
    else if (edx == 10) /* free */
    {
        ecx = (ecx + 0x0f) & 0xfffffff0; /*以16字节为单位进位取整*/
        memman_free((struct MEMMAN *)(ebx + ds_base), eax, ecx);
    }
    else if (edx == 11) /* 在窗口中画点 */
    {
        sht = (struct SHEET *)(ebx & 0xfffffffe);
        sht->buf[sht->bxsize * edi + esi] = eax;
        /* 窗口句柄即struct SHEET的地址, 一定是偶数 */
        if ((ebx & 1) == 0) /* 判断是否为偶数 */
        {                   /* 为偶数才刷新 */
            sheet_refresh(sht, esi, edi, esi + 1, edi + 1);
        }
    }
    else if (edx == 12) /* 刷新窗口api */
    {
        sht = (struct SHEET *)ebx;
        sheet_refresh(sht, eax, ecx, esi, edi);
    }
    else if (edx == 13) /* 画直线 */
    {
        sht = (struct SHEET *)(ebx & 0xfffffffe);
        hrb_api_linewin(sht, eax, ecx, esi, edi, ebp);
        if ((ebx & 1) == 0)
        {
            sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
        }
    }
    else if (edx == 14) /* 关闭窗口的API */
    {
        sheet_free((struct SHEET *)ebx);
    }
    else if (edx == 15) /* 等待键盘按键的API */
    {
        for (;;)
        {
            io_cli();
            if (fifo32_status(&task->fifo) == 0)
            {
                if (eax != 0)
                {
                    task_sleep(task); /* FIFO为空，休眠并等待*/
                }
                else
                {
                    io_sti();
                    reg[7] = -1;
                    return 0;
                }
            }
            i = fifo32_get(&task->fifo);
            io_sti();
            if (i <= 1) /*光标用定时器*/
            {
                /*应用程序运行时不需要显示光标，因此总是将下次显示用的值置为1*/
                timer_init(cons->timer, &task->fifo, 1); /*下次置为1*/
                timer_settime(cons->timer, 50);
            }
            if (i == 2) /*光标ON */
            {
                cons->cur_c = COL8_FFFFFF;
            }
            if (i == 3) /*光标OFF */
            {
                cons->cur_c = -1;
            }
            if (i == 4) /* 打开命令行 */
            {
                timer_cancel(cons->timer);
                io_cli();
                fifo32_put(sys_fifo, cons->sht - shtctl->sheets0 + 2024); /* 2024～2279 */
                cons->sht = 0;
                io_sti();
            }
            if (256 <= i && i <= 511) /*键盘数据（通过任务A）*/
            {
                reg[7] = i - 256;
                return 0;
            }
        }
    }
    else if (edx == 16) /* 申请定时器 */
    {
        reg[7] = (int)timer_alloc();
        ((struct TIMER *)reg[7])->flags2 = 1; /* 允许自动取消 */
    }
    else if (edx == 17) /* 设置定时器要发送的数据 */
    {
        timer_init((struct TIMER *)ebx, &task->fifo, eax + 256);
    }
    else if (edx == 18) /* 设置定时器时间 */
    {
        timer_settime((struct TIMER *)ebx, eax);
    }
    else if (edx == 19) /* 释放定时器 */
    {
        timer_free((struct TIMER *)ebx);
    }
    else if (edx == 20) /* 蜂鸣器 */
    {
        if (eax == 0)
        {
            i = io_in8(0x61);
            io_out8(0x61, i & 0x0d);
        }
        else
        {
            i = 1193180000 / eax;
            io_out8(0x43, 0xb6);

            io_out8(0x42, i & 0xff);
            io_out8(0x42, i >> 8);
            i = io_in8(0x61);
            io_out8(0x61, (i | 0x03) & 0x0f);
        }
    }
    else if (edx == 21) /* 打开文件 */
    {
        for (i = 0; i < 8; i++)
        {
            if (task->fhandle[i].buf == 0)
            {
                break;
            }
        }
        fh = &task->fhandle[i];
        reg[7] = 0;
        if (i < 8)
        {
            finfo = file_search((char *)ebx + ds_base,
                                (struct FILEINFO *)(ADR_DISKIMG + 0x002600), 224);
            if (finfo != 0)
            {
                reg[7] = (int)fh;
                fh->buf = (char *)memman_alloc_4k(memman, finfo->size);
                fh->size = finfo->size;
                fh->pos = 0;
                file_loadfile(finfo->clustno, finfo->size, fh->buf, task->fat, (char *)(ADR_DISKIMG + 0x003e00));
            }
        }
    }
    else if (edx == 22) /* 关闭文件 */
    {
        fh = (struct FILEHANDLE *)eax;
        memman_free_4k(memman, (int)fh->buf, fh->size);
        fh->buf = 0;
    }
    else if (edx == 23) /* 文件定位 */
    {
        fh = (struct FILEHANDLE *)eax;
        if (ecx == 0)
        {
            fh->pos = ebx;
        }
        else if (ecx == 1)
        {
            fh->pos += ebx;
        }
        else if (ecx == 2)
        {
            fh->pos = fh->size + ebx;
        }
        if (fh->pos < 0)
        {

            fh->pos = 0;
        }
        if (fh->pos > fh->size)
        {
            fh->pos = fh->size;
        }
    }
    else if (edx == 24) /* 获取文件大小 */
    {
        fh = (struct FILEHANDLE *)eax;
        if (ecx == 0)
        {
            reg[7] = fh->size;
        }
        else if (ecx == 1)
        {
            reg[7] = fh->pos;
        }
        else if (ecx == 2)
        {
            reg[7] = fh->pos - fh->size;
        }
    }
    else if (edx == 25) /* 读取文件 */
    {
        fh = (struct FILEHANDLE *)eax;
        for (i = 0; i < ecx; i++)
        {
            if (fh->pos == fh->size)
            {
                break;
            }
            *((char *)ebx + ds_base + i) = fh->buf[fh->pos];
            fh->pos++;
        }
        reg[7] = i;
    }
    else if (edx == 26) /* 获取命令行的完整命令 */
    {
        i = 0;
        for (; ; ) {
            *((char *) ebx + ds_base + i) =  task->cmdline[i];
            if (task->cmdline[i] == 0) {
                break;
            }
            if (i >= ecx) {
                break;
            }
            i++;
        }
        reg[7] = i;
    }
    return 0;
}

int *inthandler0c(int *esp) /* 栈异常 */
{
	struct TASK *task = task_now();
	struct CONSOLE *cons = task->cons;
	char s[30];
	cons_putstr0(cons, "\nINT 0C :\n Stack Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
    return &(task->tss.esp0); /*强制结束程序*/
}

int *inthandler0d(int *esp) /* 通用保护异常 */
{
	struct TASK *task = task_now();
	struct CONSOLE *cons = task->cons;
	char s[30];
	cons_putstr0(cons, "\nINT 0D :\n General Protected Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
    return &(task->tss.esp0); /*强制结束程序*/
}

void hrb_api_linewin(struct SHEET *sht, int x0, int y0, int x1, int y1, int col)
{ /* 画直线的函数 */
    int i, x, y, len, dx, dy;

    dx = x1 - x0;
    dy = y1 - y0;
    x = x0 << 10;
    y = y0 << 10;
    if (dx < 0)
    {
        dx = -dx;
    }
    if (dy < 0)
    {
        dy = -dy;
    }
    if (dx >= dy)
    {
        len = dx + 1;
        if (x0 > x1)
        {
            dx = -1024;
        }
        else
        {
            dx = 1024;
        }

        if (y0 <= y1)
        {
            dy = ((y1 - y0 + 1) << 10) / len;
        }
        else
        {
            dy = ((y1 - y0 - 1) << 10) / len;
        }
    }
    else
    {
        len = dy + 1;
        if (y0 > y1)
        {
            dy = -1024;
        }
        else
        {
            dy = 1024;
        }
        if (x0 <= x1)
        {
            dx = ((x1 - x0 + 1) << 10) / len;
        }
        else
        {
            dx = ((x1 - x0 - 1) << 10) / len;
        }
    }

    for (i = 0; i < len; i++)
    {
        sht->buf[(y >> 10) * sht->bxsize + (x >> 10)] = col;
        x += dx;
        y += dy;
    }

    return;
}
