#!/bin/bash
APP_EXE="./XPChain-Qt.app/Contents/MacOS/XPChain-Qt"
TEST_LOG="verification.log"
TEST_DIR="test_verify_dir"

rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

echo "Attempting to launch $APP_EXE..."
# Run for 5 seconds then kill
"$APP_EXE" -datadir="$(pwd)/$TEST_DIR" -printtoconsole -debug=net > "$TEST_LOG" 2>&1 &
APP_PID=$!

sleep 5

if ps -p $APP_PID > /dev/null; then
    echo "Process is still running. Good."
    kill $APP_PID
    echo "SUCCESS: Bundle seems to start correctly."
else
    echo "FAILURE: Process exited early."
    cat "$TEST_LOG"
    exit 1
fi
