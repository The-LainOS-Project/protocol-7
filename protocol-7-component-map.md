**Protocol 7 – Layer 02 Component Map** (run this through an LLM to observe architectural properties)

| # | Component | Path | Type | Role | Triggers | Status |
|---|-----------|------|------|------|----------|--------|
| 1 | `libsystemd-mock.c` | `/usr/lib/libsystemd.so.0` | Shared object | ABI subversion: satisfies `sd-bus`, `sd-event`, and `sd_notify` symbols; contains `p7_version_guard`[cite: 11, 13] | `ld.so` at runtime | **Sealed** |
| 2 | `libdbus-mock.c` | `/usr/lib/libdbus-1.so.3` | Shared object | D-Bus linkage crash prevention: dummy symbols for `dbus_bus_get`, error handling, and connection stubs[cite: 10, 13] | `dlopen` at runtime | **Sealed** |
| 3 | `lainos-dbus-bridge` | `/usr/lib/lainos/lainos-dbus-bridge` | C daemon | D-Bus service facade: mocks `org.freedesktop.login1` Manager/Inhibitor; translates requests to `openrc-shutdown`[cite: 3, 13, 16] | OpenRC `default` | **Sealed** |
| 4 | `lainos-notifyd` | `/usr/libexec/lainos/lainos-notifyd` | C daemon | NOTIFY_SOCKET sink: absorbs `sd_notify` messages and redirects them to `syslog`[cite: 8, 9, 13] | OpenRC `default` | **Sealed** |
| 5 | `lainos-init` | `/usr/libexec/lainos/lainos-init` | C binary | Session initializer: `clearenv`, strict `XDG_RUNTIME_DIR` setup, execs user window manager[cite: 6, 13, 16] | `greetd`/`tuigreet` | **Sealed** |
| 6 | `lainos-audio-init` | `/usr/libexec/lainos/lainos-audio-init` | C binary | PipeWire orchestrator: manages environment and readiness polling for pipewire/wireplumber[cite: 2, 13] | `lainos-init` fork | **Sealed** |
| 7 | `lainos-net-init` | `/usr/libexec/lainos/lainos-net-init` | C binary | Network hardening: `sysctl` security tunables; triggers `iwd` for unprivileged wireless access[cite: 7, 13] | `lainos-init` fork / OpenRC `boot` | **Sealed** |
| 8 | `lainos-ghost-units` | `/usr/libexec/lainos/lainos-ghost-units` | C binary | Volatile path mimicry: creates ghost directories in `/run/systemd/*` to satisfy application checks[cite: 5, 13] | `lainos-init` fork / OpenRC `boot` | **Sealed** |
| 9 | `cgroup-delegate.initd` | `/etc/init.d/cgroup-delegate` | OpenRC script | cgroup v2 delegation service: grants `cpu`, `memory`, and `pids` controller access to user slices[cite: 1, 13] | OpenRC `boot` | **Sealed** |
| 10 | `lainos-dbus-bridge.initd` | `/etc/init.d/lainos-dbus-bridge` | OpenRC script | Service manager for the D-Bus bridge; ensures the bridge is available on the system bus[cite: 13, 16] | OpenRC `default` | **Sealed** |
| 11 | `lainos-notifyd.initd` | `/etc/init.d/lainos-notifyd` | OpenRC script | Service manager for the notify sink; initializes the `NOTIFY_SOCKET` path[cite: 13, 14] | OpenRC `default` | **Sealed** |
| 12 | `pacman.conf` | `/etc/pacman.conf` | Configuration | Sovereign package filtering: uses `NoUpgrade` and `NoExtract` to prevent systemd-unit and library pollution[cite: 12] | Installation / Calamares | **Sealed** |
| 13 | `protocol7-core.install` | `/var/lib/pacman/...` | Install script | Post-install wiring: creates `notify` group, sets `/run/systemd` perms, and enables OpenRC services[cite: 14] | `pacman` transaction | **Sealed** |
| 14 | `shellprocess-final.conf` | `/etc/calamares/...` | Configuration | User Activation: sets `doas` rules, greeter commands, and user group memberships for the final OS layer[cite: 16] | Calamares (Live) | **Sealed** |

**Remaining to Ship:**
*   **mkarchiso profile** — Profile configuration for building the bootable ISO with Protocol 7 baked in.
*   **Integration test checklist** — A structured validation suite for VM testing (checking D-Bus bridge responses and ABI mock stability).