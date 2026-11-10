#include "sha256.h"

/* Word type: SHA-256 works on 32-bit words that wrap around mod 2^32.
 * unsigned long is exactly 32 bits on avr-gcc but 64 bits on a 64-bit
 * PC, so every operation that can carry above bit 31 (adds, left shifts)
 * is masked back to 32 bits. On the AVR the masks compile to nothing; on
 * the PC they keep the result identical, which is what lets the host
 * test prove the AVR code correct. */
typedef unsigned long sha_word_t;

#define MASK32       0xFFFFFFFFUL
#define ADD32(a, b)  (((a) + (b)) & MASK32)
#define ROTR(x, n)   ((((x) >> (n)) | ((x) << (32 - (n)))) & MASK32)

/* FIPS 180-4 section 4.1.2 functions */
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z) & MASK32))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x)     (ROTR(x, 2)  ^ ROTR(x, 13) ^ ROTR(x, 22))
#define BSIG1(x)     (ROTR(x, 6)  ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SSIG0(x)     (ROTR(x, 7)  ^ ROTR(x, 18) ^ ((x) >> 3))
#define SSIG1(x)     (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

/* Round constants, section 4.2.2: first 32 bits of the fractional parts
 * of the cube roots of the first 64 primes. On the AVR a const table is
 * copied into SRAM at boot (256 bytes); moving it to flash would need an
 * lpm reader, not worth it while RAM is plentiful. */
static const sha_word_t K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

/* Initial hash value, section 5.3.3 */
static const sha_word_t H0[8] = {
    0x6a09e667UL, 0xbb67ae85UL, 0x3c6ef372UL, 0xa54ff53aUL,
    0x510e527fUL, 0x9b05688cUL, 0x1f83d9abUL, 0x5be0cd19UL
};

/* Overwrite a buffer that held message-derived data. volatile so the
 * compiler can't drop the stores as "dead" right before a return. */
static void wipe(void *p, unsigned int n)
{
    volatile unsigned char *b = (volatile unsigned char *)p;

    while (n--) {
        *b++ = 0;
    }
}

/* Process one 64-byte block into the running hash `h` (section 6.2.2). */
static void compress(sha_word_t *h, const unsigned char *block)
{
    sha_word_t w[64];
    sha_word_t a, b, c, d, e, f, g, hh, t1, t2;
    unsigned char i;

    /* 1. message schedule: 16 big-endian words, then expand to 64 */
    for (i = 0; i < 16; i++) {
        w[i] = ((sha_word_t)block[4 * i]     << 24)
             | ((sha_word_t)block[4 * i + 1] << 16)
             | ((sha_word_t)block[4 * i + 2] << 8)
             |  (sha_word_t)block[4 * i + 3];
    }
    for (i = 16; i < 64; i++) {
        w[i] = ADD32(ADD32(SSIG1(w[i - 2]), w[i - 7]),
                     ADD32(SSIG0(w[i - 15]), w[i - 16]));
    }

    /* 2. working variables */
    a = h[0]; b = h[1]; c = h[2]; d = h[3];
    e = h[4]; f = h[5]; g = h[6]; hh = h[7];

    /* 3. 64 rounds */
    for (i = 0; i < 64; i++) {
        t1 = ADD32(ADD32(ADD32(hh, BSIG1(e)), ADD32(CH(e, f, g), K[i])), w[i]);
        t2 = ADD32(BSIG0(a), MAJ(a, b, c));
        hh = g;
        g = f;
        f = e;
        e = ADD32(d, t1);
        d = c;
        c = b;
        b = a;
        a = ADD32(t1, t2);
    }

    /* 4. fold into the running hash */
    h[0] = ADD32(h[0], a); h[1] = ADD32(h[1], b);
    h[2] = ADD32(h[2], c); h[3] = ADD32(h[3], d);
    h[4] = ADD32(h[4], e); h[5] = ADD32(h[5], f);
    h[6] = ADD32(h[6], g); h[7] = ADD32(h[7], hh);

    wipe(w, sizeof w);
}

void sha256(const unsigned char *msg, unsigned int len, unsigned char *out)
{
    sha_word_t h[8];
    unsigned char block[64];
    unsigned long bits = (unsigned long)len * 8;   /* < 2^32 for any unsigned int len we support */
    unsigned int done = 0;
    unsigned char n, i;

    for (i = 0; i < 8; i++) {
        h[i] = H0[i];
    }

    /* every full 64-byte block straight from the message */
    while (len - done >= 64) {
        compress(h, msg + done);
        done += 64;
    }

    /* Padding (section 5.1.1): the remaining bytes, a 0x80 byte, zeros,
     * then the message length in bits as a 64-bit big-endian number.
     * If fewer than 8 bytes are left after the 0x80, the length goes in
     * an extra block. */
    n = (unsigned char)(len - done);
    for (i = 0; i < n; i++) {
        block[i] = msg[done + i];
    }
    block[n++] = 0x80;
    if (n > 56) {
        while (n < 64) {
            block[n++] = 0;
        }
        compress(h, block);
        n = 0;
    }
    while (n < 56) {
        block[n++] = 0;
    }
    block[56] = 0;                 /* top 32 bits of the length are 0 */
    block[57] = 0;
    block[58] = 0;
    block[59] = 0;
    block[60] = (unsigned char)(bits >> 24);
    block[61] = (unsigned char)(bits >> 16);
    block[62] = (unsigned char)(bits >> 8);
    block[63] = (unsigned char)bits;
    compress(h, block);

    /* output: the 8 words, big-endian */
    for (i = 0; i < 8; i++) {
        out[4 * i]     = (unsigned char)(h[i] >> 24);
        out[4 * i + 1] = (unsigned char)(h[i] >> 16);
        out[4 * i + 2] = (unsigned char)(h[i] >> 8);
        out[4 * i + 3] = (unsigned char)h[i];
    }

    wipe(block, sizeof block);
    wipe(h, sizeof h);
}
