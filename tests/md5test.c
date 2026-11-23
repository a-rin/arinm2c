/* cc -o md5test tests/md5test.c src/md5.c: the digests of RFC 1321 */
#include <assert.h>
#include <string.h>
#include "../src/md5.h"
static void t(const char *s, const char *want)
{
    char o[33];
    md5_hex(s, strlen(s), o);
    assert(!strcmp(o, want));
}
int main(void)
{
    t("", "d41d8cd98f00b204e9800998ecf8427e");
    t("abc", "900150983cd24fb0d6963f7d28e17f72");
    t("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
      "57edf4a22be3c955ac49da2e2107b67a");
    return 0;
}
