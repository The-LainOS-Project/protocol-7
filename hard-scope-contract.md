# Protocol 7 — Hard Scope Contract (Layer 02 System)

This document defines the **non-negotiable architectural boundaries** of Protocol 7.

It is not guidance.
It is a constraint system.

Any deviation from this contract means the system is no longer Protocol 7.

---

# 1. Core Principle

> **OpenRC is the only system authority. Everything else is translation, compatibility, or surface emulation.**

No other component may:

* schedule services
* resolve dependencies
* define system state
* enforce policy
* manage sessions globally

---

# 2. Authority Hierarchy (Strict)

## Level 0 — Kernel (Immutable)

* Linux kernel
* cgroups v2
* device drivers

**Role:** hardware execution only
**Cannot be influenced by Protocol 7**

---

## Level 1 — System Authority (Single Source of Truth)

### OpenRC

**This is the ONLY authority in the system**

Allowed responsibilities:

* start/stop services
* define boot order (static)
* manage system daemons
* maintain system runtime state

Disallowed:

* systemd-style dependency graph solving
* session management logic
* IPC orchestration beyond service control

---

## Level 2 — Real Runtime Services

These are **actual system components**, not emulation:

* dbus-openrc
* seatd
* PipeWire / WirePlumber
* iwd
* openresolv

Allowed:

* real IPC
* real device/session control
* real networking/audio stack behavior

Disallowed:

* interpreting systemd semantics
* managing services outside their domain

---

## Level 3 — Session Bootstrap

* `lainos-init`
* `lainos-audio-init`
* `lainos-net-init`

Allowed:

* set environment
* launch session compositor (e.g. Sway)
* initialize user runtime directories

Disallowed:

* service lifecycle management
* cross-service coordination logic
* persistent system state tracking

---

## Level 4 — Compatibility Layer (STRICT LIMIT)

### Includes:

* `libsystemd-mock`
* `libdbus-mock`

### Allowed Behavior:

* satisfy linker symbols
* return safe static values
* provide ABI compatibility
* avoid crashes
* provide deterministic fake responses

### STRICTLY FORBIDDEN:

* maintaining internal system state
* simulating systemd lifecycle behavior
* tracking units, services, or dependencies
* making scheduling decisions
* adapting behavior based on runtime history

> This layer must behave like a **dictionary of answers**, not a system.

---

## Level 5 — D-Bus Translation Layer

* `lainos-dbus-bridge`

Allowed:

* translate known systemd D-Bus interfaces → OpenRC equivalents
* provide static or computed responses based on real system state
* act as compatibility facade for login1-style queries

Forbidden:

* implementing a service manager
* enforcing policy decisions
* inventing lifecycle states beyond OpenRC reality
* becoming a second init system

---

## Level 6 — Filesystem Expectation Layer

* `lainos-ghost-units`

Allowed:

* create expected paths (`/run/systemd/*`)
* satisfy file existence checks

Forbidden:

* storing state
* reflecting system behavior
* syncing with service runtime
* acting as configuration authority

---

## Level 7 — Package Integrity Layer

* pacman configuration (NoUpgrade / NoExtract rules)
* install scripts (`protocol7-core.install`)

Allowed:

* prevent systemd installation
* protect compatibility shims
* enforce deterministic system composition

Forbidden:

* modifying runtime behavior
* influencing service execution
* injecting dynamic logic into system operation

---

# 3. Absolute Prohibitions

These are **system-level violations**:

## ❌ No secondary init system

No component may:

* replicate systemd behavior
* act as PID 1 substitute logic
* simulate unit graphs

---

## ❌ No hidden state model

The system must NOT:

* track sessions internally
* track services internally
* store lifecycle history
* infer system state beyond real OS truth

---

## ❌ No policy engine

Forbidden entirely:

* authorization frameworks (polkit-like behavior)
* rule evaluation systems
* permission negotiation layers

---

## ❌ No multi-user assumptions

Protocol 7 assumes:

* 1 user
* 1 session
* 1 seat

Anything else is out of scope.

---

## ❌ No orchestration layer creep

No component may evolve into:

* dependency solver
* runtime supervisor
* system event engine

---

# 4. Allowed System Reality Model

The only valid runtime model is:

```text
OpenRC == system truth
dbus-openrc == communication layer
applications == consumers
mock layers == compatibility illusion surface
```

No other model is permitted.

---

# 5. Failure Semantics

When something breaks:

* ❌ do NOT adapt system behavior
* ❌ do NOT emulate missing systemd features
* ❌ do NOT extend compatibility dynamically

Instead:

* return failure
* block package operation (if needed)
* surface ABI mismatch

> Stability comes from refusal to expand scope.

---

# 6. Evolution Rule

Protocol 7 may evolve ONLY by:

* adding new **static ABI stubs**
* extending **translation mappings in dbus-bridge**
* improving **compatibility return correctness**

It may NOT evolve by:

* increasing system intelligence
* adding runtime decision-making
* introducing orchestration logic

---

# 7. Final Definition

> Protocol 7 is a **single-user OpenRC Linux system with a strictly bounded compatibility surface that satisfies systemd-dependent software without implementing systemd behavior.**

---

# 8. One-Line Enforcement Rule

> If a component starts making decisions about the system, it is no longer Protocol 7.

---

If you want, the next useful artifact would be a:

* **“drift detection checklist” (to prevent scope creep in code)**
* or a **formal ABI coverage map (what systemd APIs are safe to ignore vs must stub)**
