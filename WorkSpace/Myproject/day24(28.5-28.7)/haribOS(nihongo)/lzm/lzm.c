#include "apilib.h"

void HariMain(void)
{
    // static char s[5] = {0xA3, 0xB6, 0x21, 0x0a, 0x00};
    // static char s[9] = { 0xb2, 0xdb, 0xca, 0xc6, 0xce, 0xcd, 0xc4, 0x0a, 0x00 };
    static char s[] = "CS201 ‚è‚å‚¤‚µ‚å‚¤‚ß‚¢\n";
        /*shift_JIS???????p???????C???n?j?z?w?g?I??????+??s+0 */
    api_putstr0(s);
    api_end();
}