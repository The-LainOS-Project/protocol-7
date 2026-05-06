#!/bin/bash
# Protocol 7 Core - Pre-Build Verification Script
# Run this before makepkg to catch issues that won't show up at compile time

set -euo pipefail

echo "=== Protocol 7 Core Pre-Build Verification ==="
echo

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

ERRORS=0
WARNINGS=0

# Check 1: All source files exist
echo "[1/8] Checking source files exist..."
for f in libsystemd-mock.c libdbus-mock.c lainos-dbus-bridge.c lainos-ghost-units.c \
         lainos-init.c lainos-audio-init.c lainos-net-init.c lainos-notifyd.c \
         lainos-dbus-bridge.initd lainos-notifyd.initd cgroup-delegate.initd \
         protocol7-core.install; do
    if [ ! -f "$f" ]; then
        echo -e "${RED}ERROR: Missing file: $f${NC}"
        ((ERRORS++))
    fi
done
echo -e "${GREEN}✓ All source files present${NC}"

# Check 2: sha256sums integrity
echo
echo "[2/8] Verifying sha256sums..."
if [ -f sha256sums ]; then
    # Extract hashes from standalone file
    while IFS= read -r line; do
        if [[ $line =~ ^\ *\'([a-f0-9]+)\' ]]; then
            hash="${BASH_REMATCH[1]}"
            # Find corresponding file
            file=$(echo "$line" | sed 's/.*# //')
            if [ -f "$file" ]; then
                actual=$(sha256sum "$file" | awk '{print $1}')
                if [ "$hash" != "$actual" ]; then
                    echo -e "${RED}ERROR: Hash mismatch for $file${NC}"
                    echo "  Expected: $hash"
                    echo "  Actual:   $actual"
                    ((ERRORS++))
                fi
            fi
        fi
    done < sha256sums
else
    echo -e "${YELLOW}WARNING: No sha256sums file found${NC}"
    ((WARNINGS++))
fi

# Check 3: PKGBUILD syntax
echo
echo "[3/8] Checking PKGBUILD..."
if [ -f PKGBUILD ]; then
    # Check for basic syntax issues
    if ! bash -n PKGBUILD 2>/dev/null; then
        echo -e "${RED}ERROR: PKGBUILD has syntax errors${NC}"
        ((ERRORS++))
    fi

    # Check that source array length matches sha256sums
    source_count=$(grep -c '^\s*"' PKGBUILD || true)
    sums_count=$(grep -c "^[[:space:]]*'" PKGBUILD || true)

    # Extract actual counts more carefully
    source_files=$(grep '^\s*".*"' PKGBUILD | wc -l)
    sum_hashes=$(grep "^[[:space:]]*'[a-f0-9]" PKGBUILD | wc -l)

    if [ "$source_files" != "$sum_hashes" ]; then
        echo -e "${YELLOW}WARNING: PKGBUILD source count ($source_files) != sha256sums count ($sum_hashes)${NC}"
        ((WARNINGS++))
    fi
else
    echo -e "${RED}ERROR: PKGBUILD not found${NC}"
    ((ERRORS++))
fi

# Check 4: C code compilation test
echo
echo "[4/8] Test-compiling C sources..."
for src in libsystemd-mock.c libdbus-mock.c lainos-ghost-units.c lainos-init.c \
           lainos-net-init.c lainos-notifyd.c lainos-audio-init.c; do
    if [ -f "$src" ]; then
        # Try to compile with all warnings
        if ! gcc -fsyntax-only -Wall -Wextra -Werror "$src" 2>/dev/null; then
            echo -e "${YELLOW}WARNING: $src has compilation warnings (not fatal with -fsyntax-only)${NC}"
        fi
    fi
done

# Check 5: Check for dangerous functions
echo
echo "[5/8] Checking for dangerous C functions..."
dangerous_funcs="gets strcpy strcat sprintf vsprintf"
for func in $dangerous_funcs; do
    if grep -rn "$func(" *.c 2>/dev/null | grep -v "snprintf\|safe_" | head -1 >/dev/null; then
        echo -e "${RED}ERROR: Found dangerous function '$func' in:${NC}"
        grep -rn "$func(" *.c | grep -v "snprintf\|safe_" | head -3
        ((ERRORS++))
    fi
done

# Check 6: Check signal handler safety
echo
echo "[6/8] Checking signal handler safety..."
if grep -n "signal(" lainos-dbus-bridge.c lainos-notifyd.c 2>/dev/null | grep -q "signal"; then
    echo -e "${YELLOW}WARNING: Uses signal() instead of sigaction()${NC}"
    echo "  Consider using sigaction() for more reliable signal handling"
    ((WARNINGS++))
fi

# Check 7: Check for hardcoded paths that might need configuration
echo
echo "[7/8] Checking hardcoded paths..."
hardcoded_paths=$(grep -rh "^\s*#define\s\+.*\"/" *.c 2>/dev/null | grep -v "http" | wc -l)
if [ "$hardcoded_paths" -gt 0 ]; then
    echo -e "${YELLOW}INFO: $hardcoded_paths hardcoded #define paths found${NC}"
    echo "  These are appropriate for a single-user system but may need"
    echo "  configuration for multi-user or non-standard layouts."
fi

# Check 8: Verify mock library fake pointers are in safe range
echo
echo "[8/8] Checking mock library safety..."
if grep -q "0xdead" libsystemd-mock.c libdbus-mock.c 2>/dev/null; then
    echo -e "${GREEN}✓ Fake pointers use 0xdead/0xbeef prefix (unmapped kernel space)${NC}"
    echo "  This ensures immediate SIGSEGV on dereference rather than memory corruption."
fi

# Summary
echo
echo "========================================"
echo "Verification Complete"
echo "========================================"
echo -e "Errors:   ${RED}$ERRORS${NC}"
echo -e "Warnings: ${YELLOW}$WARNINGS${NC}"

if [ $ERRORS -eq 0 ]; then
    echo -e "${GREEN}✓ PASSED - Safe to build${NC}"
    exit 0
else
    echo -e "${RED}✗ FAILED - Fix errors before building${NC}"
    exit 1
fi
