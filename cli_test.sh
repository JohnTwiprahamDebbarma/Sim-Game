#!/bin/sh
#
# Feed the front end malformed, out-of-range and truncated input.  Nothing here
# may crash, hang, or trip a sanitizer.  Pass the binary to exercise:
#
#     ./cli_test.sh ./sim
#     ./cli_test.sh ./sim.asan
#
set +m                      # keep the shell from announcing killed jobs

BIN=${1:-./sim}
STATUS=0
ERR=$(mktemp)

# Portable watchdog: the original front end looped forever at end of input.
run_limited() {
    "$@" >/dev/null 2>"$ERR" &
    pid=$!

    ( sleep 20; kill -9 "$pid" 2>/dev/null ) >/dev/null 2>&1 &
    watchdog=$!
    wait "$pid"
    rc=$?
    kill -9 "$watchdog" 2>/dev/null
    return $rc
}

check() {
    desc=$1
    shift
    printf '%s\n' "$@" | { run_limited "$BIN"; echo $? > "$ERR.rc"; } 2>/dev/null
    rc=$(cat "$ERR.rc")

    if [ "$rc" -ge 128 ]; then
        echo "  FAIL  $desc -- killed by signal $((rc - 128)) (crash or hang)"
        STATUS=1
    elif grep -qE 'AddressSanitizer|runtime error|Assertion' "$ERR"; then
        echo "  FAIL  $desc -- sanitizer or assertion failure"
        sed 's/^/        /' "$ERR" | head -5
        STATUS=1
    else
        echo "  ok    $desc"
    fi
}

# Valid input must be taken: the game gets as far as `pattern`.
accepts() {
    desc=$1
    pattern=$2
    shift 2
    if printf '%s\n' "$@" | "$BIN" 2>/dev/null | grep -q "$pattern"; then
        echo "  ok    $desc"
    else
        echo "  FAIL  $desc -- not accepted"
        STATUS=1
    fi
}

# Invalid input must be refused outright -- never read as some other choice.
refuses() {
    desc=$1
    pattern=$2
    shift 2
    if printf '%s\n' "$@" | "$BIN" 2>/dev/null | grep -q "$pattern"; then
        echo "  FAIL  $desc -- read as a valid choice"
        STATUS=1
    else
        echo "  ok    $desc"
    fi
}

echo "cli tests ($BIN)"
check "out-of-range edge"        1 99 0
check "negative edge"            1 -3 0
check "non-numeric edge"         1 abc 0
check "huge edge as Blue"        2 999999 1
check "INT_MAX edge"             1 2147483647 0
check "end of input mid-game"    1
check "empty input"
check "invalid mode"             9
check "non-numeric mode"         hello
check "occupied edge"            1 0 0 1
check "repeated bad input"       1 x y z 0

# A number too big for an int used to wrap onto a real edge: 4294967301 is
# 2^32 + 5, and scanf("%d") read it as 5.
MOVED="Computer's Move"
accepts "a plain move"             "$MOVED" 1 5
accepts "a move with spaces"       "$MOVED" 1 "  5  "
accepts "a move with CRLF"         "$MOVED" 1 "$(printf '5\r')"
refuses "2^32 as a move"           "$MOVED" 1 4294967296
refuses "2^32 + 5 as a move"       "$MOVED" 1 4294967301
refuses "-(2^32 - 5) as a move"    "$MOVED" 1 -4294967291
refuses "trailing junk"            "$MOVED" 1 5abc
refuses "two numbers on a line"    "$MOVED" 1 "5 7"
refuses "hex"                      "$MOVED" 1 0x5

PROMPT="Enter your move"
accepts "side 1"                   "$PROMPT" 1
accepts "side 2"                   "$PROMPT" 2
refuses "2^32 + 1 as the side"     "$PROMPT" 4294967297
refuses "2^32 + 2 as the side"     "$PROMPT" 4294967298
refuses "trailing junk as the side" "$PROMPT" 1abc

rm -f "$ERR" "$ERR.rc"
[ "$STATUS" -eq 0 ] && echo "cli tests: ok"
exit "$STATUS"
