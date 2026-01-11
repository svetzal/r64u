#!/bin/bash
#
# Build and test script for r64u
# Usage: ./build_test.sh [--quick]
#
# Options:
#   --quick    Skip cmake configure step (faster for incremental builds)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
QT_PATH="${QT_PATH:-$HOME/Qt/6.10.1/macos}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
QUICK_BUILD=false
for arg in "$@"; do
    case $arg in
        --quick)
            QUICK_BUILD=true
            ;;
    esac
done

echo "========================================"
echo "  r64u Build & Test"
echo "========================================"
echo ""

# Configure step
if [ "$QUICK_BUILD" = false ] || [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo -e "${YELLOW}Configuring...${NC}"
    cmake -B "$BUILD_DIR" -DCMAKE_PREFIX_PATH="$QT_PATH" -DCMAKE_BUILD_TYPE=Debug
    echo ""
fi

# Build step
echo -e "${YELLOW}Building...${NC}"
cmake --build "$BUILD_DIR" --parallel
echo ""

# Test step
echo "========================================"
echo "  Running Tests"
echo "========================================"
echo ""

TOTAL_PASSED=0
TOTAL_FAILED=0
TOTAL_SKIPPED=0
FAILED_TESTS=""

for test_exe in "$BUILD_DIR"/tests/test_*; do
    # Skip non-executables and directories
    [[ -x "$test_exe" && ! -d "$test_exe" ]] || continue

    test_name=$(basename "$test_exe")

    # Skip autogen directories
    [[ "$test_name" == *_autogen ]] && continue

    echo -n "  $test_name: "

    # Run test and capture output
    output=$("$test_exe" 2>&1) || true

    # Parse results - format: "Totals: X passed, Y failed, Z skipped, W blacklisted, Nms"
    totals=$(echo "$output" | grep "^Totals:" || echo "")

    if [ -n "$totals" ]; then
        # Extract numbers using awk
        passed=$(echo "$totals" | awk '{print $2}')
        failed=$(echo "$totals" | awk '{print $4}')
        skipped=$(echo "$totals" | awk '{print $6}')

        # Default to 0 if empty
        passed=${passed:-0}
        failed=${failed:-0}
        skipped=${skipped:-0}

        TOTAL_PASSED=$((TOTAL_PASSED + passed))
        TOTAL_FAILED=$((TOTAL_FAILED + failed))
        TOTAL_SKIPPED=$((TOTAL_SKIPPED + skipped))

        if [ "$failed" -gt 0 ]; then
            echo -e "${RED}FAILED${NC} ($passed passed, $failed failed, $skipped skipped)"
            FAILED_TESTS="$FAILED_TESTS\n  - $test_name"
            # Show failure details
            echo "$output" | grep -A2 "^FAIL!" | head -20
        else
            echo -e "${GREEN}OK${NC} ($passed passed, $skipped skipped)"
        fi
    else
        echo -e "${RED}ERROR${NC} - no test output"
        FAILED_TESTS="$FAILED_TESTS\n  - $test_name (no output)"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
    fi
done

echo ""
echo "========================================"
echo "  Summary"
echo "========================================"
echo ""
echo "  Passed:  $TOTAL_PASSED"
echo "  Failed:  $TOTAL_FAILED"
echo "  Skipped: $TOTAL_SKIPPED"
echo ""

if [ $TOTAL_FAILED -gt 0 ]; then
    echo -e "${RED}FAILED TESTS:${NC}"
    echo -e "$FAILED_TESTS"
    echo ""
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    echo ""
    exit 0
fi
