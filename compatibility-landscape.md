Protocol 7 will work best with software stacks that:

1. only lightly touch `libsystemd`
2. use logind/session APIs opportunistically
3. mainly want desktop continuity
4. tolerate static single-user semantics
5. do not depend on real unit orchestration

That is actually a surprisingly large portion of the Linux desktop ecosystem.

Here’s the realistic compatibility landscape.

---

# Will Likely Work Very Well

These are ideal Protocol 7 targets.

---

## Wayland Compositors

### Likely strong compatibility:

* Sway
* Hyprland
* Wayfire
* labwc
* River

Why:

* already comfortable with seatd
* often OpenRC-friendly
* limited systemd dependence
* mostly require session identity continuity

Your static:

```text
seat0
session 1
active
wayland
```

model aligns very well with their assumptions.

---

# PipeWire Stack

### Very likely workable:

* PipeWire
* WirePlumber

Especially when paired with:

* seatd
* elogind replacement semantics
* static session metadata

PipeWire mostly wants:

* user session presence
* permissions context
* active graphical session assumptions

Your deterministic identity layer is well-targeted for this.

---

# XDG Desktop Portals

### Potentially good:

* xdg-desktop-portal
* xdg-desktop-portal-wlr
* xdg-desktop-portal-gtk

These frequently:

* probe logind
* query sessions
* check inhibitors

But often only shallowly.

Your compatibility model can likely satisfy many code paths.

This is one of the most important target areas.

---

# Electron Applications

### Surprisingly good target

Examples:

* Discord
* Visual Studio Code
* Obsidian
* Slack

Electron commonly:

* checks session state
* queries power management
* probes portals
* interacts with D-Bus superficially

Most Electron apps do *not* deeply require systemd orchestration.

Your “safe continuation” model is almost optimized for Electron.

---

# Chromium-Based Browsers

### Likely workable:

* Chromium
* Google Chrome
* Brave
* Vivaldi

They mainly expect:

* portals
* login sessions
* power-management APIs
* inhibitors
* desktop identity

Your compatibility layer directly targets these assumptions.

This is probably one of the best real-world validation suites for Protocol 7.

---

# GTK Desktop Applications

### Very likely:

* GNOME Text Editor
* Evince
* Loupe
* Transmission

GTK apps usually depend more on:

* portals
* D-Bus
* session continuity

than systemd itself.

---

# Qt/KDE Applications

### Mixed but often workable:

* Dolphin
* Okular
* Kate

Qt apps themselves are usually fine.

Full KDE Plasma is more complicated.

---

# Games / Steam

### Potentially good:

* Steam
* Proton
* Lutris

Steam mostly cares about:

* desktop sessions
* portals
* audio
* GPU access
* user continuity

Not unit orchestration.

This is another strong compatibility target.

---

# Likely Problematic

These are the dangerous zones.

---

# Full GNOME Shell Stack

### High risk:

* GNOME Shell
* gdm

GNOME increasingly assumes:

* logind authority
* systemd user sessions
* inhibitors
* lifecycle semantics
* activation behaviors

This is probably the least aligned ecosystem for Protocol 7.

---

# Full KDE Plasma Session

### Medium-high risk:

* KDE Plasma

KDE is less systemd-centric than GNOME,
but Plasma still increasingly expects:

* session coordination
* power management
* service integration

Could work partially.
May require more compatibility shims.

---

# systemd User Units

### Fundamentally incompatible:

* `systemctl --user`
* transient units
* timers
* user-service orchestration

Your contract explicitly forbids:

* orchestration
* lifecycle engines
* dependency graphs

So this area should intentionally fail.

That is actually architecturally correct.

---

# Container/Orchestration Ecosystems

### Poor target:

* Podman rootless integration
* Kubernetes tooling
* systemd-managed containers

These often genuinely depend on:

* cgroups
* delegated authority
* transient lifecycle control

Your architecture intentionally avoids this domain.

---

# What Protocol 7 Is Optimized For

The architecture is best suited for:

> single-user graphical workstation environments using modern desktop software without systemd lifecycle dependence.

That is the sweet spot.

Especially:

* Wayland
* Electron
* Chromium
* PipeWire
* GTK apps
* gaming
* portals

paired with:

* OpenRC
* seatd
* dbus
* static session identity

---

# Your Best Ecosystem Pairing

The most natural Protocol 7 stack probably looks something like:

| Layer           | Component              |
| --------------- | ---------------------- |
| init            | OpenRC                 |
| session         | greetd                 |
| seat management | seatd                  |
| compositor      | Hyprland / Sway        |
| audio           | PipeWire               |
| portals         | xdg-desktop-portal-wlr |
| compatibility   | Protocol 7             |
| browser         | Chromium/Brave         |
| apps            | Electron + GTK         |

That ecosystem aligns almost perfectly with your scope contract.

---

# Final Reality Check

Protocol 7 is not trying to make:

> “all systemd software work.”

It is trying to make:

> “modern desktop Linux usable without systemd.”

Those are very different goals.

Under that narrower goal,
your compatibility model is much more realistic than many people would initially assume.