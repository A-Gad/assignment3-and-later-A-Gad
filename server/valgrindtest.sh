#!/bin/bash
# valgrind_test.sh — memory leak test for aesdsocket
# Usage: ./valgrind_test.sh
# Requires: valgrind, netcat (nc)

BINARY="./aesdsocket"
PORT=9000
VALGRIND_LOG="valgrind_output.log"
TEST_MESSAGES=("hello world" "foo bar" "embedded linux" "test packet")

# ─── Checks ────────────────────────────────────────────────────────────────────

if ! command -v valgrind &> /dev/null; then
    echo "[ERROR] valgrind not found. Install with: sudo apt-get install valgrind"
    exit 1
fi

if ! command -v nc &> /dev/null; then
    echo "[ERROR] netcat not found. Install with: sudo apt-get install netcat"
    exit 1
fi

if [ ! -f "$BINARY" ]; then
    echo "[ERROR] Binary '$BINARY' not found. Run make first."
    exit 1
fi

# ─── Cleanup from previous run ─────────────────────────────────────────────────

rm -f "$VALGRIND_LOG"
rm -f /var/tmp/aesdsocketdata

# ─── Start server under valgrind ───────────────────────────────────────────────

echo "[*] Starting aesdsocket under valgrind..."
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --error-exitcode=1 \
    --log-file="$VALGRIND_LOG" \
    "$BINARY" &

VALGRIND_PID=$!
echo "[*] Valgrind PID: $VALGRIND_PID"

# Give server time to bind
sleep 1

# ─── Send test packets ─────────────────────────────────────────────────────────

echo "[*] Sending test packets..."
for msg in "${TEST_MESSAGES[@]}"; do
    echo "[>] Sending: '$msg'"
    response=$(echo -e "${msg}\n" | nc -q 1 localhost $PORT)
    echo "[<] Response: '$response'"
    sleep 0.2
done

# ─── Graceful shutdown ─────────────────────────────────────────────────────────

echo "[*] Sending SIGINT to trigger graceful shutdown..."
kill -SIGINT $VALGRIND_PID

# Wait for valgrind to finish writing the log
wait $VALGRIND_PID
EXIT_CODE=$?

# ─── Results ───────────────────────────────────────────────────────────────────

echo ""
echo "══════════════════════════════════════════"
echo "         VALGRIND RESULTS"
echo "══════════════════════════════════════════"
cat "$VALGRIND_LOG"
echo "══════════════════════════════════════════"

if grep -q "no leaks are possible" "$VALGRIND_LOG" || grep -q "All heap blocks were freed" "$VALGRIND_LOG"; then
    echo "[PASS] No memory leaks detected"
else
    echo "[FAIL] Memory leaks detected — see $VALGRIND_LOG for details"
fi

if grep -q "ERROR SUMMARY: 0 errors" "$VALGRIND_LOG"; then
    echo "[PASS] No valgrind errors"
else
    echo "[FAIL] Valgrind errors found — see $VALGRIND_LOG for details"
fi

echo "══════════════════════════════════════════"
exit $EXIT_CODE