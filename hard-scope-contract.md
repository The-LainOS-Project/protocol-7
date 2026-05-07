# Protocol 7 — Hard Scope Contract (Compatibility-First Edition)

## 1. Purpose

This document defines the architectural and behavioral boundaries of Protocol 7.

Protocol 7 is a **compatibility surface for systemd-linked software running on a non-systemd system (OpenRC-based Arch Linux)**.

It provides:

* ABI compatibility
* expected D-Bus interfaces
* minimal system identity semantics
* controlled compatibility responses

It does **not implement systemd**, and it does **not attempt to replicate systemd as a system manager**.

---

## 2. Core Principles

### 2.1 OpenRC is the System Authority

* OpenRC is the only service manager and boot authority.
* Protocol 7 does not replace, extend, or reimplement OpenRC behavior.

---

### 2.2 Interface Compatibility, Not System Emulation

* systemd interfaces may be exposed (ABI, D-Bus, paths)
* internal systemd behavior is not implemented
* only the *minimum behavior required for application compatibility* is provided

---

### 2.3 Minimal Deterministic System State

Protocol 7 may maintain **minimal, fixed system identity state** for compatibility purposes:

* single user
* single session
* single seat (`seat0`)
* stable machine identity representation

This state is:

* deterministic
* non-evolving
* not a lifecycle system

It does not represent a session manager or orchestration engine.

---

### 2.4 Compatibility-First Return Policy

When handling systemd-related calls:

* Return success when:

  * no real system side-effect exists
  * behavior is informational or cosmetic
  * a harmless no-op satisfies caller expectations

* Return errors when:

  * the operation requires real systemd subsystem behavior
  * it implies unsupported orchestration or multi-user logic
  * it depends on missing kernel/system features

Errors must be accurate, not defensive.

---

### 2.5 No Systemd Reimplementation

Protocol 7 must not implement:

* unit lifecycle management
* dependency graphs
* service orchestration engines
* transaction systems
* multi-seat/session management systems

It may respond to queries about these concepts, but not simulate them.

---

### 2.6 Single-User System Model

The system assumes:

* exactly one user
* exactly one active session
* exactly one seat (`seat0`)

Multi-user semantics are not modeled.

---

## 3. Authority Hierarchy

| Level | Component                               | Role                        | Forbidden                   |
| ----- | --------------------------------------- | --------------------------- | --------------------------- |
| 0     | Kernel                                  | Hardware execution          | —                           |
| 1     | OpenRC                                  | Service management          | systemd lifecycle logic     |
| 2     | system services (PipeWire, seatd, etc.) | real system functionality   | systemd assumptions         |
| 3     | init/session scripts                    | environment setup           | cross-service orchestration |
| 4     | libsystemd-mock / libdbus-mock          | ABI compatibility layer     | system modeling, scheduling |
| 5     | dbus-bridge                             | interface translation       | service management logic    |
| 6     | ghost layers                            | filesystem compatibility    | persistent state storage    |
| 7     | pacman constraints                      | system integrity protection | runtime behavior changes    |

---

## 4. Behavior Model

### 4.1 Compatibility Over Strict Failure

* Prefer safe no-op success when:

  * operation is informational
  * no real system effect exists
  * applications expect success for continuation

* Prefer error return when:

  * operation requires unsupported system semantics
  * returning success would imply real functionality exists

---

### 4.2 No Persistent System Simulation

Protocol 7 must not:

* simulate evolving system state
* track lifecycle transitions over time
* model service dependency graphs
* maintain “system truth” beyond static identity

---

### 4.3 Deterministic Identity Values

The system may provide stable values for:

* machine-id
* session ID
* seat name
* user identity

These values:

* must remain constant across runtime
* must not encode behavior or relationships
* must not imply orchestration capability

---

### 4.4 Opaque Success Rule

A function may return success with a synthetic value if:

* the value is non-semantic or informational
* it does not encode system structure
* it does not imply lifecycle state

Examples:

* `"active"`, `"running"` (safe labels)
* `"seat0"`, `"1"` (static identifiers)

Not allowed:

* hierarchical unit trees
* dependency graphs
* dynamic service state reasoning

---

### 4.5 Suspend Exception (Preserved)

`Suspend` and `CanSuspend`:

* return success
* perform no real suspend action

This exception remains explicitly defined and limited.

---

## 5. Evolution Rules

### 5.1 ABI Expansion Allowed

* new systemd symbols may be added
* implementations must remain minimal and compatibility-focused

---

### 5.2 No Control Logic Growth

Mock layers must not evolve into:

* schedulers
* service managers
* policy engines
* dependency resolvers

They may only:

* return static values
* return safe no-op results
* provide minimal contextual responses

---

### 5.3 Compatibility Drift Handling

If upstream software introduces new systemd expectations:

* extend ABI surface minimally
* avoid introducing systemic logic
* prefer stubbed compatibility responses

---

## 6. Enforcement Model

### 6.1 Core Rule

> If a component begins *deciding system behavior*, it has exceeded Protocol 7 scope.

---

### 6.2 Review Checklist

Before changes are accepted:

* [ ] Does this introduce lifecycle tracking?
* [ ] Does this introduce dependency reasoning?
* [ ] Does this simulate systemd internals?
* [ ] Does this require persistent state evolution?
* [ ] Does this replace OpenRC responsibility?

If yes → reject or redesign.

---

## 7. System Guarantee

Protocol 7 guarantees:

* No systemd runtime dependency
* No systemd service manager replacement
* Stable single-user system model
* ABI compatibility for common desktop software
* Controlled, deterministic behavior surface

---

## 8. Summary

Protocol 7 is:

> A real OpenRC-based system with a minimal, compatibility-first systemd interface layer that avoids reimplementing systemd while satisfying application expectations.

---

If you want next step refinement, the real improvement from here isn’t contract tightening—it’s **mapping each libsystemd-mock function into one of three categories:** (TODO)

* static identity
* safe no-op success
* real OS passthrough

