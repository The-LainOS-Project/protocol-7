# Protocol 7 — Reverse Engineered Compatibility Surface

## I. systemd ABI surface
- Full `libsystemd` symbol table — what AUR binaries actually import, not what the headers declare
- `libdbus-1` symbol table for the `dlopen` consumer path
- Return value semantics per symbol — what callers do with 0, -1, a pointer, or a specific integer; whether return values are checked at all
- Opaque handle pointer patterns — which functions return handles passed between calls vs handles that get dereferenced directly
- Out-of-scope symbol identification — which symbols no real AUR binary in the target stack actually calls

## II. D-Bus interface contracts
- `org.freedesktop.login1` — full Manager/Session/Seat/User property and method surface as actually queried by AUR software
- Which properties and methods AUR software actually queries versus what the spec formally defines

## III. Runtime directory layout
- The `/run/systemd/*` directory tree — which paths software probes at runtime
- `/run/user/$UID` creation semantics, permissions, and ownership expectations

## IV. sd_notify protocol
- Datagram format, socket path, and message semantics that software sends to the notify socket

## V. cgroup2 delegation expectations
- What the cgroup2 tree must look like for Wayland compositors and desktop software to function without systemd managing it

## VI. Pacman/AUR dependency graph
- Which packages pull in systemd transitively and through which paths — to define the complete `IgnorePkg`, `NoUpgrade`, `NoExtract`, `provides`, and `conflicts` rules