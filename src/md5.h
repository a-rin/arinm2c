/* md5: the MD5 digest (RFC 1321) of a text, as 32 hexadecimal digits */
#ifndef ARINM2C_MD5_H
#define ARINM2C_MD5_H

#include <stddef.h>

/* ponytail: MD5 only names the version of a definition module (it is not
   a defence against an attacker); ceiling: a deliberate collision; upgrade:
   SHA-256 */
void md5_hex(const char *data, size_t n, char out[33]);

#endif
