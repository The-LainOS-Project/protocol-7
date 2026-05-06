#!/bin/bash
# Protocol 7 Core - Runtime Smoke Tests
# Run these after installation to verify functionality

set -euo pipefail

echo "=== Protocol 7 Core Runtime Tests ==="
echo

# Test 1: Check mock libraries are installed
echo "[1/6] Checking mock libraries..."
for lib in /usr/lib/libsystemd.so.0 /usr/lib/libdbus-1.so.3; do
    if [ -f "$lib" ]; then
        echo "  ✓ $lib exists"
        # Check ELF note
        if readelf -n "$lib" 2>/dev/null | grep -q "LainOS"; then
            echo "    ✓ ELF note verified"
        fi
    else
        echo "  ✗ $lib MISSING"
        exit 1
    fi
done

# Test 2: Check D-Bus bridge
echo
echo "[2/6] Checking D-Bus bridge..."
if rc-service lainos-dbus-bridge status 2>/dev/null | grep -q "started"; then
    echo "  ✓ lainos-dbus-bridge is running"
    # Test logind interface
    if dbus-send --system --print-reply --dest=org.freedesktop.login1 \
         /org/freedesktop/login1 org.freedesktop.DBus.Introspectable.Introspect 2>/dev/null | \
         grep -q "org.freedesktop.login1.Manager"; then
        echo "    ✓ D-Bus interface responding"
    fi
else
    echo "  ⚠ lainos-dbus-bridge not running (may need start)"
fi

# Test 3: Check notify socket
echo
echo "[3/6] Checking notify socket..."
if [ -S /run/systemd/notify ]; then
    echo "  ✓ /run/systemd/notify socket exists"
    ls -la /run/systemd/notify
else
    echo "  ✗ /run/systemd/notify socket MISSING"
fi

# Test 4: Check ghost directories
echo
echo "[4/6] Checking ghost directories..."
for dir in /run/systemd/{system,notify,private,users,sessions,machines,shutdown,inhibit}; do
    if [ -d "$dir" ]; then
        echo "  ✓ $dir"
    else
        echo "  ✗ $dir MISSING"
    fi
done

# Test 5: Check cgroup delegation
echo
echo "[5/6] Checking cgroup delegation..."
if [ -f /sys/fs/cgroup/cgroup.subtree_control ]; then
    echo "  ✓ cgroup2 mounted"
    cat /sys/fs/cgroup/cgroup.subtree_control | tr ' ' '\n' | while read ctrl; do
        if [ -n "$ctrl" ]; then
            echo "    ✓ Controller enabled: $ctrl"
        fi
    done
else
    echo "  ✗ cgroup2 not mounted"
fi

# Test 6: Verify no real systemd libraries leaked
echo
echo "[6/6] Checking for stray systemd libraries..."
stray=$(find /usr/lib -name "libsystemd*.so*" -not -path "*/lainos/*" 2>/dev/null || true)
if [ -n "$stray" ]; then
    echo "  ⚠ WARNING: Stray libsystemd found:"
    echo "$stray" | sed 's/^/    /'
else
    echo "  ✓ No stray libsystemd libraries"
fi

echo
echo "========================================"
echo "Runtime Tests Complete"
echo "========================================"
