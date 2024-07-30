#include "bootpack.h"

// 申请图层管理结构的内存空间，并初始化图层管理的值
struct SHTCTL *shtctl_init(struct MEMMAN *memman, unsigned char *vram, int xsize, int ysize)
{
    struct SHTCTL *ctl;
    int i;
    ctl = (struct SHTCTL *) memman_alloc_4k(memman, sizeof (struct SHTCTL)); //分配一个图层控制变量的内存空间
    if (ctl == 0) {
        goto err;
    }
    ctl->map = (unsigned char *) memman_alloc_4k(memman, xsize * ysize);
    if(ctl->map == 0)
    {
		memman_free_4k(memman, (int) ctl, sizeof (struct SHTCTL));
		goto err;
    }
    ctl->vram = vram;
    ctl->xsize = xsize;
    ctl->ysize = ysize;
    ctl->top = -1; /*一个SHEET没都有 */
    for (i = 0; i < MAX_SHEETS; i++) {
        ctl->sheets0[i].flags = 0; /* 将所有图层标记为未使用 */
        ctl->sheets0[i].ctl = ctl; //记录图层控制的地址
    }
err:
    return ctl;
}

#define SHEET_USE	1
// 申请一个图层，仅初始化了高度为隐藏和切换状态为已使用，正常返回一个图层结构体的地址，图层用满了返回0
struct SHEET *sheet_alloc(struct SHTCTL *ctl)
{
    struct SHEET *sht;
    int i;
    for (i = 0; i < MAX_SHEETS; i++)
    {
        if (ctl->sheets0[i].flags == 0)
        {
            sht = &ctl->sheets0[i];
            sht->flags = SHEET_USE; /*正在使用的标记*/
            sht->height = -1; /*不显示*/
            sht->task = 0; /*不使用自动关闭功能*/ 
            return sht;
        }
    }
    return 0; /*所有的图层都正在使用*/
}
// 设置图层缓冲区大小和透明色
void sheet_setbuf(struct SHEET *sht, unsigned char *buf, int xsize, int ysize, int col_inv)
{
    sht->buf = buf;
    sht->bxsize = xsize;
    sht->bysize = ysize;
    sht->col_inv = col_inv;
    return;
}

// 刷新从h0开始往上所有刷新该区域(vx0,vy0)-(vx1,vy1)的图层map
void sheet_refreshmap(struct SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0)
{
    int h, bx, by, vx, vy, bx0, by0, bx1, by1;
    unsigned char *buf, sid, *map = ctl->map;
    struct SHEET *sht;
    if (vx0 < 0) { vx0 = 0; }
    if (vy0 < 0) { vy0 = 0; }
    if (vx1 > ctl->xsize) { vx1 = ctl->xsize; }
    if (vy1 > ctl->ysize) { vy1 = ctl->ysize; }
    for (h = h0; h <= ctl->top; h++) {
        sht = ctl->sheets[h];
        sid = sht - ctl->sheets0; /* 将进行了减法计算的地址作为图层号码使用 */
        buf = sht->buf;
        bx0 = vx0- sht->vx0;
        by0 = vy0- sht->vy0;
        bx1 = vx1- sht->vx0;
        by1 = vy1- sht->vy0;
        if (bx0 < 0) { bx0 = 0; }
        if (by0 < 0) { by0 = 0; }
        if (bx1 > sht->bxsize) { bx1 = sht->bxsize; }
        if (by1 > sht->bysize) { by1 = sht->bysize; }
        for (by = by0; by < by1; by++) {
            vy = sht->vy0 + by;
            for (bx = bx0; bx < bx1; bx++) {
                vx = sht->vx0 + bx;
                if (buf[by * sht->bxsize + bx] != sht->col_inv) {
                    map[vy * ctl->xsize + vx] = sid;
                }
            }
        }
    }
    return;
}

// 刷新图层h0高度往上到h1的所有图层的某个区域
void sheet_refreshsub(struct SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0, int h1)
{
    int h, bx, by, vx, vy, bx0, by0, bx1, by1;
    unsigned char *buf, *vram = ctl->vram, *map = ctl->map, sid;
    struct SHEET *sht;
    // 如果刷新的区域超出了画面则修正
    if (vx0 < 0) { vx0 = 0; }
    if (vy0 < 0) { vy0 = 0; }
    if (vx1 > ctl->xsize) { vx1 = ctl->xsize; }
    if (vy1 > ctl->ysize) { vy1 = ctl->ysize; }
    for (h = h0; h <= h1; h++) { //遍历所有图层
        sht = ctl->sheets[h];
        buf = sht->buf; //拿到图层的显示信息
        sid = sht - ctl->sheets0; //拿到当前图层的ID
        /* 使用vx0～vy1，对bx0～by1进行倒推 */
        //计算图层和刷新区域的重复区域的大小
        bx0 = vx0- sht->vx0;
        by0 = vy0- sht->vy0;
        bx1 = vx1- sht->vx0;
        by1 = vy1- sht->vy0;
        if (bx0 < 0) { bx0 = 0; } //比较左上角是否在刷新区域内，是则赋0
        if (by0 < 0) { by0 = 0; }
        if (bx1 > sht->bxsize) { bx1 = sht->bxsize; } //比较右下角点是否在刷新区域内，是则赋为图层的右下角点
        if (by1 > sht->bysize) { by1 = sht->bysize; }
        // 至此拿到图层buf和刷新区域的重复区域的左上角点(bx0,by0)和右下角点(bx1,by1)(相对系是图层的buf)
        for (by = by0; by < by1; by++) {//遍历重复区域的y轴
            vy = sht->vy0 + by; //计算实际的y显存的位置
            for (bx = bx0; bx < bx1; bx++) { //遍历重复区域的x轴
                vx = sht->vx0 + bx; //计算实际的x显存位置
                if (map[vy * ctl->xsize + vx] == sid) { //比较图层map中该点的图层ID和当前图层ID是否相等
                    vram[vy * ctl->xsize + vx] = buf[by * sht->bxsize + bx]; //覆盖新的颜色
                }
            }
        }
    }
    return;
}

// 改变图层的高度
void sheet_updown(struct SHEET *sht, int height)
{
	struct SHTCTL *ctl = sht->ctl;
    int h, old = sht->height; /* 存储设置前的高度信息 */

    /*  如果指定的高度过高或过低，则进行修正 */
    if (height > ctl->top + 1) {
        height = ctl->top + 1;
    }
    if (height < -1) {
        height = -1;
    }
    sht->height = height; /* 设定高度 */

    /* 下面主要是进行sheets[ ]的重新排列 */
    if (old > height) { /* 比以前低 */
        if (height >= 0) {
            /* 把中间的往上提 */
            for (h = old; h > height; h--) {
                ctl->sheets[h] = ctl->sheets[h -1];
                ctl->sheets[h]->height = h;
            }
            ctl->sheets[height] = sht;
            sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height + 1);
            sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height + 1, old); /* 重新绘制图层区域的画面 */

        } else {	/* 隐藏状态 */
            if (ctl->top > old) {
                /* 把上面的降下来 */
                for (h = old; h < ctl->top; h++) {
                    ctl->sheets[h] = ctl->sheets[h + 1];
                    ctl->sheets[h]->height = h;
                }
            }
            ctl->top--; /* 由于显示中的图层减少了一个，所以最上面的图层高度下降 */
			sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0);
            sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0, old - 1); /* 重新绘制图层区域的画面 */
        }
    } else if (old < height) {  /* 比以前高 */
        if (old >= 0) {
            /* 把中间的拉下去 */
            for (h = old; h < height; h++) {
                ctl->sheets[h] = ctl->sheets[h + 1];
                ctl->sheets[h]->height = h;
            }
            ctl->sheets[height] = sht;
        } else {     /* 由隐藏状态转为显示状态 */
            /* 将已在上面的提上来 */
            for (h = ctl->top; h >= height; h--) {
                ctl->sheets[h + 1] = ctl->sheets[h];
                ctl->sheets[h + 1]->height = h + 1;
            }
            ctl->sheets[height] = sht;
            ctl->top++; /* 由于已显示的图层增加了1个，所以最上面的图层高度增加 */
        }
		sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height);
        sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height, height); /* 重新绘制图层区域的画面 */
    }
    return;
}

// 刷新当前图层的某个区域的画面
void sheet_refresh(struct SHEET *sht, int bx0, int by0, int bx1, int by1)
{
    if (sht->height >= 0) { /* 如果正在显示，刷新图层画面*/
        sheet_refreshsub(sht->ctl, sht->vx0 + bx0, sht->vy0 + by0, sht->vx0 + bx1, sht->vy0 + by1, sht->height, sht->height);
    }
    return;
}

// 不改变图层高度，上下左右移动图层
void sheet_slide(struct SHEET *sht, int vx0, int vy0)
{
	struct SHTCTL *ctl = sht->ctl;
    int old_vx0 = sht->vx0, old_vy0 = sht->vy0;
    sht->vx0 = vx0;
    sht->vy0 = vy0;
    if (sht->height >= 0) { /* 如果正在显示，则按新图层的信息刷新画面 */
        sheet_refreshmap(ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0);                   /* 刷新移动前区域的所有图层map */
        sheet_refreshmap(ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height);                         /* 刷新移动后从该图层开始的图层map */
        sheet_refreshsub(ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0, sht->height - 1);  /* 刷新就图层区域的画面 */
        sheet_refreshsub(ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height, sht->height);            /* 刷新新图层区域的画面 */
    }
    return;
}

// 释放已使用完的图层
void sheet_free(struct SHEET *sht)
{
    if (sht->height >= 0) {
        sheet_updown(sht, -1); /* 如果处于显示状态，则先设定为隐藏 */
    }
    sht->flags = 0; /* "未使用"标志  */
    return;
}


