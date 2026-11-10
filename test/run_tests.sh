#!/bin/bash
# Host tests for the passcode hashing. Run from anywhere:
#   bash test/run_tests.sh
# Needs gcc and coreutils' sha256sum.
set -e
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

CFLAGS="-Wall -Wextra -Werror -std=c99"

echo "== sha256 known-answer vectors"
gcc $CFLAGS -o "$OUT/sha256_test" test/sha256_test.c sha256.c
"$OUT/sha256_test"

echo "== sha256 vs sha256sum, every length 0..300 (random bytes)"
for n in $(seq 0 300); do
    head -c "$n" /dev/urandom > "$OUT/msg"
    want=$(sha256sum < "$OUT/msg" | cut -d' ' -f1)
    got=$("$OUT/sha256_test" - < "$OUT/msg")
    if [ "$got" != "$want" ]; then
        echo "FAIL at length $n: got $got want $want"
        exit 1
    fi
done
echo "ok   301 lengths match"

echo "== sha256 vs sha256sum, 60000 bytes (near the AVR's 16-bit limit)"
head -c 60000 /dev/urandom > "$OUT/msg"
[ "$("$OUT/sha256_test" - < "$OUT/msg")" = "$(sha256sum < "$OUT/msg" | cut -d' ' -f1)" ]
echo "ok"

echo "== locker (stubbed EEPROM + entropy)"
gcc $CFLAGS -o "$OUT/locker_test" test/locker_test.c locker.c sha256.c
"$OUT/locker_test"

echo "== locker hash vs sha256sum: stored hash == SHA-256(salt || \"1234\")"
read -r salt hash < <("$OUT/locker_test" --dump)
want=$( { printf "$(echo "$salt" | sed 's/../\\x&/g')"; printf 1234; } | sha256sum | cut -d' ' -f1)
if [ "$hash" != "$want" ]; then
    echo "FAIL: stored $hash, sha256sum says $want"
    exit 1
fi
echo "ok   salt $salt"
echo "     hash $hash"

echo
echo "ALL HOST TESTS PASSED"
