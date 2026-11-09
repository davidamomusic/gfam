/* Minimal freestanding runtime: the compiler may emit calls to these. */
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *dp = d; const uint8_t *sp = s;
    while (n--) *dp++ = *sp++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *dp = d; const uint8_t *sp = s;
    if (dp < sp) { while (n--) *dp++ = *sp++; }
    else { dp += n; sp += n; while (n--) *--dp = *--sp; }
    return d;
}

void *memset(void *d, int c, size_t n)
{
    uint8_t *dp = d;
    while (n--) *dp++ = (uint8_t)c;
    return d;
}

void __aeabi_memcpy(void *d, const void *s, size_t n)  { memcpy(d, s, n); }
void __aeabi_memcpy4(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memcpy8(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memmove(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memset(void *d, size_t n, int c)          { memset(d, c, n); }
void __aeabi_memset4(void *d, size_t n, int c)         { memset(d, c, n); }
void __aeabi_memset8(void *d, size_t n, int c)         { memset(d, c, n); }
void __aeabi_memclr(void *d, size_t n)                 { memset(d, 0, n); }
void __aeabi_memclr4(void *d, size_t n)                { memset(d, 0, n); }
void __aeabi_memclr8(void *d, size_t n)                { memset(d, 0, n); }

/* ---- integer division (ARM7TDMI has no divide instruction) ---- */
uint32_t __aeabi_uidiv(uint32_t n, uint32_t d)
{
    uint32_t q = 0, bit = 1;
    if (d == 0) return 0;
    while (d < n && !(d & 0x80000000u)) { d <<= 1; bit <<= 1; }
    while (bit) {
        if (n >= d) { n -= d; q |= bit; }
        d >>= 1; bit >>= 1;
    }
    return q;
}

int32_t __aeabi_idiv(int32_t n, int32_t d)
{
    int neg = (n < 0) != (d < 0);
    uint32_t q = __aeabi_uidiv(n < 0 ? 0u - (uint32_t)n : (uint32_t)n,
                               d < 0 ? 0u - (uint32_t)d : (uint32_t)d);
    return neg ? (int32_t)(0u - q) : (int32_t)q;
}

/* divmod helpers return {quotient, remainder} in r0, r1 */
__asm__(
    ".syntax unified\n.thumb\n.text\n"
    ".global __aeabi_uidivmod\n.thumb_func\n__aeabi_uidivmod:\n"
    "  push {r0, r1, lr}\n  bl __aeabi_uidiv\n  pop {r1, r2}\n"
    "  muls r2, r0, r2\n  subs r1, r1, r2\n  pop {pc}\n"
    ".global __aeabi_idivmod\n.thumb_func\n__aeabi_idivmod:\n"
    "  push {r0, r1, lr}\n  bl __aeabi_idiv\n  pop {r1, r2}\n"
    "  muls r2, r0, r2\n  subs r1, r1, r2\n  pop {pc}\n"
);
