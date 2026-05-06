# Protocol 7 — LainOS Layer 02 System

## Systemd Compatibility Layer for OpenRC on Arch Linux (Single-User Desktop Stack)

---

## Overview

Protocol 7 is a **minimal, interface-level compatibility layer** designed to run Arch Linux without systemd while preserving broad compatibility with AUR software.

It does **not replace systemd** and does **not emulate full systemd behavior**.

Instead, it provides a controlled compatibility surface that satisfies common systemd expectations in user applications while the system itself runs on a **non-systemd init and IPC stack**.

The goal is simple:

> **Run a usable, modern desktop system (Wayland + AUR ecosystem) without systemd.**

---

## Design Philosophy

Protocol 7 is built on three strict principles:

### 1. Interface over Implementation

Only the *expected surface* of systemd is provided:

* libraries
* return values
* D-Bus endpoints
* filesystem paths

No full subsystem logic is implemented.

---

### 2. Single-User Assumption

The system assumes:

* 1 user
* 1 session
* 1 seat (`seat0`)
* no session switching
* no multi-user runtime coordination

This eliminates logind-class complexity entirely.

---

### 3. Real System + Minimal Compatibility Layer

Protocol 7 is not a system replacement. It is layered on top of a real system:

* Init: **OpenRC**
* IPC: **dbus-openrc**
* Session: **seatd**
* Compositor: **Sway**

---

## System Scope

### Supported Environment

Protocol 7 is designed for:

* Wayland-based desktop environments
* AUR-heavy workflows
* Electron / Chromium applications
* PipeWire audio stacks
* Flatpak (portal-based usage)
* CLI and developer tooling

---

### Explicitly Out of Scope

The following are intentionally unsupported:

* KDE Plasma, GNOME, Cinnamon (logind/polkit dependency)
* systemd-based services (`systemctl`)
* journald
* systemd-resolved
* systemd-homed
* multi-seat or multi-user systems

---

## Architecture

Protocol 7 consists of four layers:

---

## Layer 0 — Base System (Real OS)

This is the actual operating system environment:

* Init: OpenRC
* IPC: dbus-openrc
* Session: seatd
* Audio: PipeWire / WirePlumber
* Networking: iwd + openresolv
* Privilege: doas

This layer is **fully real and authoritative**.

---

## Layer 1 — ABI Compatibility Layer

### Components

* `libsystemd-mock`
* `libdbus-mock`

### Purpose

Provides systemd-linked applications with:

* expected symbols
* expected ABI signatures
* safe no-op or minimal implementations

### Behavior Model

* returns success by default
* provides stable identity values
* avoids internal state tracking
* enforces non-dereference fake pointers

---

## Layer 2 — Runtime Compatibility Layer

### Components

* `lainos-dbus-bridge`
* `lainos-notifyd`
* `lainos-ghost-units`

### Purpose

Translates systemd-style expectations into OpenRC-compatible behavior.

### Responsibilities

| Function         | Behavior                               |
| ---------------- | -------------------------------------- |
| login1 API       | D-Bus facade mapped to OpenRC          |
| sd_notify        | logged via syslog, no system control   |
| `/run/systemd/*` | ghost directories for existence checks |

---

## Layer 3 — Session & Initialization Layer

### Components

* `lainos-init`
* `lainos-audio-init`
* `lainos-net-init`

### Purpose

Initializes a single-user Wayland session.

### Responsibilities

* environment sanitization
* runtime directory setup
* compositor launch (Sway)
* PipeWire + network initialization

---

## Layer 4 — Policy & Integrity Layer

### Components

* `pacman.conf` restrictions
* installation sealing scripts
* Protocol 7 install hooks

### Purpose

Prevents system corruption from systemd dependencies.

### Guarantees

* systemd packages cannot be installed or upgraded
* Protocol 7 shims cannot be overwritten
* ABI drift halts package transactions

---

## Execution Flow

### Application Startup

```text
Application
   ↓
libsystemd-mock (ABI satisfied)
   ↓
sd-bus call
   ↓
dbus-openrc (real IPC)
   ↓
lainos-dbus-bridge (if systemd interface required)
   ↓
OpenRC services / system utilities
```

---

## Key Design Behavior

### 1. Static System Identity

The system always presents:

* one session
* one seat (`seat0`)
* one active user session

---

### 2. Deterministic Responses

System-level queries return stable values:

* no runtime mutation of identity
* no session switching logic

---

### 3. Compatibility-First Responses

APIs return:

* success when safe
* error codes when necessary (`-ENODATA`, `-ENOENT`)

---

### 4. Non-Orchestration Model

Protocol 7 does NOT:

* manage services dynamically
* track dependencies
* simulate systemd lifecycle behavior

---

## Compatibility Profile

### Works Well

* Electron applications
* Chromium / Firefox
* CLI tooling
* Wayland-native applications
* PipeWire audio stack
* Flatpak (portal-based workflows)

---

### Works With Limitations

* applications querying login1 APIs
* software expecting optional systemd features

---

### Not Supported

* systemctl-based workflows
* polkit-dependent desktop environments
* systemd-resolved / journald / homed ecosystems

---

## System Guarantees

Protocol 7 guarantees:

* No systemd runtime dependency
* No systemd package contamination
* Deterministic single-user environment
* Stable ABI compatibility surface
* No hidden system orchestration layer

---

## Failure Model

Failures occur only when:

* software requires full systemd semantics
* new upstream systemd APIs are not yet covered
* applications rely on multi-user/session coordination

Failure behavior:

* install-time blocking preferred
* runtime fallback otherwise

---

## Summary

Protocol 7 is:

> A real OpenRC-based Linux system with a minimal compatibility layer that satisfies systemd-dependent applications without implementing systemd.

---

## One-Line Definition

> **Protocol 7 = Real Linux system + constrained systemd compatibility surface for single-user AUR desktop operation**

---


## Overview

Protocol 7 is an early stage Constraint-driven compatibility layer that reinterprets systemd as a shallow interface contract, not a runtime dependency. It is designed to let you run Arch Linux with OpenRC while maintaining 90%+ access to the Arch User Repository. Many AUR packages assume systemd is present—they list it as a dependency, link against its libraries, or check for its runtime directories. Without systemd, these packages fail to install or crash at runtime. Protocol 7 solves this by providing just enough of systemd's interface to keep them happy.

Protocol 7 does not replace systemd for all possible use cases. It is designed for a specific, minimal userland stack that deliberately excludes polkit, KDE, GNOME, and any software requiring deep logind or polkit integration. The supported environment is documented in full in the layer 02 system table below. Substitutions outside this list are untested and likely incompatible.

The layer consists of eight components: mock libraries that satisfy the dynamic linker, a D-Bus shim that answers login manager queries, ghost directories that fake systemd's runtime paths, a notification sink that absorbs sd_notify messages, session initializers for Wayland compositors, and pacman configuration that prevents real systemd from ever touching the system.

---

## Why It Exists

Before Protocol 7, OpenRC users on Arch had three options when encountering AUR packages that required systemd: patch every affected PKGBUILD, maintain personal forks of those packages, or avoid the AUR entirely. Each option was labor-intensive, fragile, or self-defeating. Protocol 7 is designed to be the fourth way: satisfy the dependency without providing the full functionality, because the functionality was never truly needed.

---

## What Should Work

Most AUR packages install and run without modification, including Electron applications, Chromium, Discord, Signal, Spotify, Flatpak with desktop portals, PipeWire audio, and Sway or other Wayland compositors.

---

## What Will Not Work

Packages that call systemctl directly, require systemd-journald, or depend on systemd-resolved or systemd-homed will fail. Use OpenRC alternatives: rc-service for service management, syslog-ng for logging, and openresolv for DNS.

The following are explicitly out of scope and will not function:

    Desktop environments requiring logind or polkit: KDE Plasma, GNOME, Cinnamon, and similar stacks are architecturally incompatible. lainOS uses seatd for session management and doas for privilege escalation; polkit is not present.
    Software calling systemctl directly: Use rc-service and OpenRC service scripts instead.
    systemd-journald: Use syslog-ng or another OpenRC-compatible logging daemon.
    systemd-resolved: Use openresolv.
    systemd-homed: Not supported.

---

## How It Works

Protocol 7 lies to the package manager and the linker just enough that applications proceed past their systemd checks without crashing. The dynamic linker receives mock libraries with the correct SONAME. File existence checks find ghost directories already in place. D-Bus calls receive plausible replies from stub services. Notification calls get absorbed and logged. Pacman is configured to ignore real systemd, never extract unit files, and never overwrite Protocol 7 components.


Most applications treat systemd interactions as best-effort and ignore return values, making them naturally tolerant of stub implementations. Protocol 7 exploits this tolerance.


OpenRC service packages are provided by the Artix `system` repo, meaning that this is a hybrid Arch/Artix setup(for the time being).

---

## System Resilience

Protocol 7 leverages the strict dependency logic of Arch Linux to maintain a stable, derterministic system state. When upstream updates introduce "symbol drift"--new functions required by software that the current mock libraries do not yet contain--the package manager detects the mismatch and automatically aborts the transaction. Because the environment is governed by a "Pacman Seal" of `IgnorePkg` and `NoUpgrade` rules, it is architecturally impossible for an update to overwrite custom shims or force-install systemd. The system does not crash; it simply holds the update at the gate until the ABI mock is extended to satisfy the new requirements. In some ways, this configuration should be more stable than Arch itself.

---

## Architecture

The complete component map is documented in the repository. Core components include libsystemd-mock for linker satisfaction, libdbus-mock for dlopen crash prevention, lainos-dbus-bridge as a login1 D-Bus facade (including power actions and session property stubs), lainos-notifyd for absorbing sd_notify messages, lainos-init for session environment setup, lainos-audio-init for PipeWire orchestration, lainos-net-init for network hardening, lainos-ghost-units for creating volatile systemd paths, cgroup-delegate for cgroup v2 controller delegation, a hardened pacman.conf that seals the system against systemd leakage, and a Calamares shellprocess-final.conf file which wires OpenRC services during ISO installation.


---

## 2. The lainOS layer 02 system


    The following components constitute the complete supported userland for Protocol 7. This table is not a list of suggestions—it is the operational boundary of the system. Each component was selected specifically because it functions without systemd, logind, or polkit. Also, as of right now, this system is only intended for single user setups.



| Category | Component | Role / Purpose |
| --- | --- | --- |
| **Init System** | **OpenRC** | Primary service supervision (hybrid cgroup mode); replaces systemd as PID 1. |
| **Initramfs** | **dracut** | Modern initramfs generator; handles early-boot RAM-wipe and decryption logic. |
| **Device Mgmt** | **eudev** | Standalone device node manager; a systemd-free fork of the udev daemon. |
| **IPC Bus** | **dbus-openrc** | Standard D-Bus message bus with native OpenRC init integration. |
| **API Bridge** | **openrc-settingsd** | Provides hostnamed, localed, and timedated D-Bus APIs for desktop compatibility. |
| **Session Mgmt** | **seatd** | Minimalist seat and session handling for unprivileged Wayland access. |
| **Compositor** | **Sway** | Primary Wayland tiling compositor; functions independently of `logind`. |
| **Login Mgmt** | **greetd / tuigreet** | Secure TUI-based greeter portal for session initialization. |
| **Interface** | **yambar / alacritty** | Minimalist C-based status monitoring and high-performance Wayland terminal. |
| **Notifications** | **mako** | Lightweight notification daemon for the Wayland environment. |
| **Audio Engine** | **Pipewire / wireplumber** | Modern audio/video routing; operates via sovereign session management. |
| **Connectivity** | **iwd** | Standalone wireless daemon (iwctl); ensures network autonomy. |
| **DNS / Resolver** | **openresolv** | Hardened `resolv.conf` management to prevent DNS leakage and entropy. |
| **Media Autonomy** | **udevil / devmon** | Sovereign auto-mount logic; provides Thunar sidebar support without `udisks2`. |
| **Media Support** | **thunar-volman / gvfs** | Enhanced volume management and filesystem abstraction for the userland. |
| **Authentication** | **doas** | Minimalist privilege escalation; replaces `sudo` for reduced attack surface. |
| **Entropy** | **haveged** | Provides high-entropy for the system RNG to ensure cryptographic strength. |




Protocol 7: ≈ around 2,500 lines of actual working code 

systemd: ≈ 1.2 – 1.7 million lines of code 