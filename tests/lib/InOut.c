/* InOut has no implementation module: it is written in C */
#include <stdint.h>
#include <stdio.h>
#include "InOut.h"

void InOut_WriteInt(int64_t v, int64_t width)
{
    printf("%*lld", (int)width, (long long)v);
}

void InOut_WriteLn(void)
{
    putchar('\n');
}
