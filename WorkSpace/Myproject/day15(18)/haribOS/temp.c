struct FILEINFO {
    unsigned char name[8], ext[3], type;
    char reserve[10];
    unsigned short time, date, clustno;
    unsigned int size;
};

void console_task(struct SHEET *sheet, unsigned int memtotal)
{
   （中略）
    struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);  /*这里！*/

   （中略）

    for (; ; ) {
        io_cli();
        if (fifo32_status(&task->fifo) == 0) {
           （中略）
        } else {
           （中略）
            if (256 <= i && i <= 511) { /*键盘数据（通过任务A）*/
                if (i == 8 + 256) {
                    /*退格键*/
                   （中略）
                } else if (i == 10 + 256) {
                    /*回车键*/
                   （中略）
                    /*执行命令*/
                    if (strcmp(cmdline, "mem") == 0) {
                      /* mem命令*/
                       （中略）
                    } else if (strcmp(cmdline, "cls") == 0) {
                      /* cls命令*/
                       （中略）
/*从此开始*/         } else if (strcmp(cmdline, "dir") == 0) {
                      /* dir命令  */
                      for (x = 0; x < 224; x++) {
                          if (finfo[x].name[0] == 0x00) {
                              break;
                          }
                          if (finfo[x].name[0] ! = 0xe5) {
                              if ((finfo[x].type & 0x18) == 0) {
                                  sprintf(s, "filename.ext   %7d", finfo[x].size);
                                  for (y = 0; y < 8; y++) {
                                      s[y] = finfo[x].name[y];
                                  }
                                  s[ 9] = finfo[x].ext[0];
                                  s[10] = finfo[x].ext[1];
                                  s[11] = finfo[x].ext[2];
                                  putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF,
                                      COL8_000000, s, 30);
                                  cursor_y = cons_newline(cursor_y, sheet);
                            }
                        }
                    }
                    cursor_y = cons_newline(cursor_y, sheet);

                    } else if (cmdline[0] ! = 0) {
                      /*不是命令，也不是空行*/
                       （中略）
                    }
                   （中略）
                } else {
                    /*一般字符*/
                   （中略）
                }
            }
           （中略）
        }
    }
}