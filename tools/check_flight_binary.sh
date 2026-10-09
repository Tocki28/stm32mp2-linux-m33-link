#!/bin/sh
# Guard 2: the finished flight program must contain no test code.
# Guard 1 checks what goes into the build; this checks what came out, so it also
# catches routes nobody thought of (for example someone defining LINK_TEST_BUILD
# in the flight build, which switches guard 1 off).
# It fails closed: if the program's names cannot be read (missing file, stripped
# program), that is a failure, not a pass. Run it before any stripping.
# On failure the program is deleted, so it cannot be deployed by accident.
#
# Usage: check_flight_binary.sh PROGRAM

# Names that exist only in test code. A new test-only class must be added here.
TEST_ONLY_NAMES='LoopbackTransport|PseudoTerminal'

# Positive control: a name every flight program contains. If the search cannot
# see this, it cannot see test code either, so the check proves nothing.
# Found by testing (8 Oct 2026): macOS `strip` keeps a few system names (main,
# poll, read), so "no names at all" was not enough to detect a stripped program.
FLIGHT_NAME='RpmsgPort'

program="$1"

fail() {
  echo "GUARD 2 FAILED: $program: $1 - program deleted" >&2
  rm -f "$program"
  exit 1
}

[ -n "$program" ] || { echo "usage: $0 PROGRAM" >&2; exit 2; }
[ -f "$program" ] || fail "file not found"

symbols=$(nm -C "$program") || fail "cannot read its names"
printf '%s\n' "$symbols" | grep -q "$FLIGHT_NAME" ||
  fail "flight name $FLIGHT_NAME not visible (stripped?), so it cannot be checked"

found=$(printf '%s\n' "$symbols" | grep -E "$TEST_ONLY_NAMES" | head -3)
[ -z "$found" ] || fail "contains test code:
$found"

echo "guard 2: $program contains no test code"
