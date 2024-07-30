int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
   （中略）

    if (edx == 1) {
        cons_putchar(cons, eax & 0xff, 1);
    } else if (edx == 2) {
        cons_putstr0(cons, (char *) ebx + ds_base);
    } else if (edx == 3) {
cons_putstr1(cons, (char *) ebx + ds_base, ecx);

    } else if (edx == 4) {
        return &(task->tss.esp0);
    } else if (edx == 5) {
       （中略）
    } else if (edx == 6) {                                       /*从此开始*/
        sht = (struct SHEET *) ebx;
        putfonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *) ebp + ds_base);
        sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
    } else if (edx == 7) {
        sht = (struct SHEET *) ebx;
        boxfill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
        sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);      /*到此结束*/
    }
    return 0;
}