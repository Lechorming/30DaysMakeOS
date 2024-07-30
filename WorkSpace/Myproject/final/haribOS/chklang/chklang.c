#include "apilib.h"

void HariMain(void)
{
    int langmode = api_getlang();
    static char s1[] = "langmode 1";
    static char s2[] = "langmode 2";
    if (langmode == 0) {
        api_putstr0("English ASCII mode\n");
    }
    if (langmode == 1) {
        api_putstr0(s1);
    }
    if (langmode == 2) {
        api_putstr0(s2);
    }
    api_end();
}
