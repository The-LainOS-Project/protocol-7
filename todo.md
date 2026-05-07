Make sure libsystemd-mock.c is consistent with this list.

---

## 1. sd_bus_call() — improve method coverage accuracy

**Why it matters:** This is one of the highest-impact entry points for Electron + system services.

**Improve:**

* Add correct *per-method return shaping* for:

  * `GetSession`
  * `ListSessions`
  * `GetSeat`
  * `GetUser`
  * `CanSuspend / CanReboot / CanPowerOff`
* Ensure reply *structure matches expected DBus signatures*, not just “success”

---

## 2. Session identity consistency cluster (sd_session_* APIs)

**Why it matters:** Desktop environments validate session coherence.

**Improve:**

* Ensure these always align:

  * `sd_session_get_uid`
  * `sd_session_get_username`
  * `sd_session_get_seat`
  * `sd_session_get_type`
  * `sd_session_get_state`

Even if static, they should form a **consistent identity bundle**.

---

## 3. PID / peer / pidfd API convergence

**Why it matters:** Electron and sandboxing layers compare these.

**Improve:**

* Ensure consistency between:

  * `sd_pid_get_*`
  * `sd_peer_get_*`
  * `sd_pidfd_get_*`

**Key improvement:**

* Same PID → same UID, cgroup, unit, slice everywhere

---

## 4. sd_bus_message_* metadata accuracy

**Why it matters:** Some DBus clients validate message metadata.

**Improve:**

* `sd_bus_message_get_type` should reflect real usage context (not always METHOD_CALL if used as signal/return internally)
* Improve:

  * sender consistency (`":1.xx"` stable per bus session)
  * interface/member pairing realism

---

## 5. sd_bus_error_* semantics tightening

**Why it matters:** Electron and system services branch on error types.

**Improve:**

* Ensure:

  * correct errno mapping
  * meaningful DBus error names where used
* Avoid generic failure where specific DBus errors are expected (e.g. `org.freedesktop.DBus.Error.Timeout`, `AccessDenied` patterns)

---

## 6. sd_id128_* correctness (machine identity cluster)

**Why it matters:** Some desktop components validate machine identity consistency.

**Improve:**

* Ensure:

  * `sd_id128_get_machine` stays stable across calls
  * `sd_id128_to_string` round-trips consistently
* Avoid partial/undefined hex parsing behavior (currently slightly fragile)

---

## 7. sd_watchdog_enabled behavior realism

**Why it matters:** systemd-aware apps sometimes branch based on watchdog presence.

**Improve:**

* If enabled, return plausible nonzero timeout instead of always 0 when appropriate

---

## 8. sd_is_socket* family — tighten correctness

**Why it matters:** Used heavily in service detection logic.

**Improve:**

* Ensure:

  * family/type/listening checks are strictly consistent
  * avoid false positives on partial matches
* Improve sockaddr validation edge cases (especially UNIX sockets)

---

## 9. sd_event_* lifecycle simplification consistency

**Why it matters:** Some apps assume event sources behave coherently.

**Improve:**

* Ensure:

  * event source IDs behave consistently across add/remove operations (even if fake)
  * avoid conflicting fd semantics across `sd_event_get_fd` and sources

---

## 10. Session/UID lookup coherence across all APIs

**Why it matters:** This is one of the most commonly cross-checked domains.

**Improve:**

* Align all:

  * `sd_uid_*`
  * `sd_session_*`
  * `sd_pid_*`
  * `sd_peer_*`

into a single coherent “identity snapshot”

---

## 11. Reduce contradiction risk in returned strings

**Why it matters:** Some desktop stacks sanity-check string consistency.

**Improve examples:**

* `desktop = "sway"` should align with session type `wayland`
* `seat0`, `session-1.scope`, etc. should not conflict across APIs

---

## 12. Improve DBus call success realism (sd_bus_call / sd_bus_call_method)

**Why it matters:** This is where Electron-heavy apps actually break.

**Improve:**

* Return:

  * correct reply shape per method
  * not just “success + FAKE_MESSAGE”
* Add minimal method-aware payload shaping for:

  * login1
  * org.freedesktop.systemd1 (even if mocked)

---

### Highest-impact priorities (if you only do a few)

1. `sd_bus_call()` method-aware responses
2. session identity consistency cluster
3. PID/peer/pidfd coherence
4. DBus error semantics
5. socket detection correctness

---

If you want next step, I can turn this into a **patch plan ordered by impact (top 10 fixes that will immediately improve Electron startup reliability)** or map it directly onto your file line-by-line.