int cmd_app(struct CONSOLE *cons, int *fat, char *cmdline)
{
   （中略）
    char name[18], *p, *q;       /*这里！*/

   （中略）

    if (finfo ! = 0) {
        /*找到文件的情况*/
        p = (char *) memman_alloc_4k(memman, finfo->size);
        q = (char *) memman_alloc_4k(memman, 64 * 1024);     /*这里！*/
        *((int *) 0xfe8) = (int) p;
        file_loadfile(finfo->clustno, finfo->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
        set_segmdesc(gdt + 1003, finfo->size -1, (int) p, AR_CODE32_ER);
        set_segmdesc(gdt + 1004, 64 * 1024-1,   (int) q, AR_DATA32_RW);   /*这里！*/
       （中略）
        start_app(0, 1003 * 8, 64 * 1024, 1004 * 8);         /*这里！*/
        memman_free_4k(memman, (int) p, finfo->size);
        memman_free_4k(memman, (int) q, 64 * 1024);          /*这里！*/
        cons_newline(cons);
        return 1;
    }
    /*没有找到文件的情况*/
    return 0;
}