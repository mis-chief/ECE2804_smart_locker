#ifndef sha256_h
#define sha256_h

/* SHA-256 (FIPS 180-4), written for this project: no libraries and no
 * #includes, so the same sha256.c builds for the AVR and on a PC for
 * testing (test/sha256_test.c).
 *
 * One-shot only: the whole message is passed in a single call. Inputs
 * here are tiny (a salt plus a passcode), so there is no streaming API.
 * `len` is limited to what an unsigned int holds (65535 bytes on AVR).
 *
 * Cost on the ATmega328P is a few ms per 64-byte block, and a message of
 * up to 55 bytes is one block. Uses about 300 bytes of stack while
 * running (the 64-word message schedule). */

#define SHA256_DIGEST_LEN 32   /* bytes of output */

#ifdef __cplusplus
extern "C" {
#endif

/* Hash `len` bytes at `msg` into `out` (SHA256_DIGEST_LEN bytes). */
void sha256(const unsigned char *msg, unsigned int len, unsigned char *out);

#ifdef __cplusplus
}
#endif

#endif
