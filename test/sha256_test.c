/* Host-side test for ../sha256.c. Not part of the sketch: the Arduino
 * IDE only builds the sketch folder's top level and src/, never test/.
 *
 * Build and run from the repo root:
 *   gcc -Wall -Wextra -std=c99 -o sha256_test test/sha256_test.c sha256.c
 *   ./sha256_test          known-answer vectors (NIST / FIPS 180-4 examples)
 *   ./sha256_test -        hash stdin, print hex (compare with sha256sum)
 * test/run_tests.sh does both, across every length from 0 to 300 bytes. */

#include <stdio.h>
#include <string.h>
#include "../sha256.h"

static void to_hex(const unsigned char *d, char *hex)
{
    static const char digits[] = "0123456789abcdef";
    int i;

    for (i = 0; i < SHA256_DIGEST_LEN; i++) {
        hex[2 * i]     = digits[d[i] >> 4];
        hex[2 * i + 1] = digits[d[i] & 0x0F];
    }
    hex[2 * SHA256_DIGEST_LEN] = '\0';
}

static int check(const char *name, const unsigned char *msg, unsigned int len,
                 const char *expect)
{
    unsigned char d[SHA256_DIGEST_LEN];
    char hex[2 * SHA256_DIGEST_LEN + 1];

    sha256(msg, len, d);
    to_hex(d, hex);
    if (strcmp(hex, expect) != 0) {
        printf("FAIL %s\n  got  %s\n  want %s\n", name, hex, expect);
        return 1;
    }
    printf("ok   %s\n", name);
    return 0;
}

int main(int argc, char **argv)
{
    static unsigned char buf[100000];
    int fails = 0;

    if (argc > 1 && strcmp(argv[1], "-") == 0) {
        unsigned char d[SHA256_DIGEST_LEN];
        char hex[2 * SHA256_DIGEST_LEN + 1];
        size_t n = fread(buf, 1, sizeof buf, stdin);

        sha256(buf, (unsigned int)n, d);
        to_hex(d, hex);
        printf("%s\n", hex);
        return 0;
    }

    fails += check("empty", (const unsigned char *)"", 0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    fails += check("abc", (const unsigned char *)"abc", 3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    fails += check("448-bit (2 blocks)",
        (const unsigned char *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    fails += check("896-bit (2 blocks)",
        (const unsigned char *)"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                               "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 112,
        "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");

    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails != 0;
}
