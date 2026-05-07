# Protocol 7 — Current Status (v4.3.0)

**Last updated:** 2026-05-06

The project is in **active stabilization** following a major fix wave. Most previously identified issues have been resolved.

---

## Component Audit & Status (v4.3)

### 1. `lainos-dbus-bridge.c` — **Resolved**
- Added `CanHibernate` / `CanHybridSleep` properties (return `"no"`).
- Implemented `Hibernate` / `HybridSleep` methods (return `NotSupported`).
- Full `org.freedesktop.login1.Manager`, `Session`, and `Seat` interfaces.
- Improved introspection XML, power actions (Reboot/PowerOff → OpenRC), and session/user handling.
- **Status: Good** — Solid for AUR/Electron compatibility.

### 2. `lainos-notifyd.c` — **Resolved**
- Added `sd_notify` state parsing (`READY=1`, `STATUS=`, `WATCHDOG=1`, etc.).
- Implemented rate limiting (100 msg/sec, burst 10).
- Increased buffer to 4096 bytes.
- Seccomp-hardened, privilege drop to `nobody:notify`.
- **Status: Good**

### 3. `lainos-init.c` — **Resolved**
- Compositor fallback: `P7_COMPOSITOR` env → `sway` → `dwl` → `river`.
- Comprehensive environment setup:
  - `XDG_SESSION_DESKTOP`, `XDG_CURRENT_DESKTOP`
  - `WAYLAND_DISPLAY=wayland-1`
  - `MOZ_ENABLE_WAYLAND=1`
  - Proper `XDG_RUNTIME_DIR` and `DBUS_SESSION_BUS_ADDRESS`
- TOCTOU-safe `/run/user/$UID` creation.
- **Status: Good**

### 4. `lainos-audio-init.c` — **Resolved**
- Spawns `pipewire` → `wireplumber` → `pipewire-pulse` (non-fatal).
- Socket readiness polling with timeout.
- Optional `pipewire-jack` via env var.
- **Status: Good** — PulseAudio compatibility restored.

### 5. `lainos-net-init.c` — **Solid**
- Strong IPv4 hardening via sysctls.
- Minor optional: `net.ipv4.tcp_timestamps=0` could still be added (low priority).
- **Status: Excellent**

### 6. `libdbus-mock.c` — **Resolved**
- Fixed dlopen stability for modern apps.
- **Status: Good**

### 7. `libsystemd-mock.c` — **Stable**
- v4.3 ABI surface.
- **Status: Good**

### 8. Supporting Components
- `lainos-ghost-units.c`: Creates required `/run/systemd/*` paths — **Good**
- `cgroup-delegate.initd`: Early cgroup v2 delegation — **Good**
- pacman seals / hooks: Prevent systemd contamination — **Strong**

---

## Overall Project Status

- **Maturity**: Rapidly maturing. v4.3.0 marks significant stability improvements.
- **Scope Adherence**: Strict compliance with `hard-scope-contract.md` (OpenRC is sole authority).
- **Compatibility**: Strong for single-user Wayland desktop + AUR (Electron, Chromium, PipeWire, Flatpak portals, etc.).
- **Out of Scope**: Full DEs (GNOME/KDE), multi-user, `systemctl`, journald, etc.

**Next Steps (Low Priority)**
- Broader AUR testing and edge-case monitoring.
- Optional: Add `tcp_timestamps=0` to net-init.
- Documentation / ISO Calamares integration polish.

---

**Philosophy Reminder**: Interface over Implementation. Provide the minimal surface needed — never become a second init system.