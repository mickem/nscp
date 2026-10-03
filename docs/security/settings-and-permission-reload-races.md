---
title: "Settings reload races: included settings stores and the permission table"
fixed_in: next
severity: "Low; Medium where an HTTP settings source enables the permission policy"
modules: [core]
action: conditional
---
A concurrency review of the core found places where a settings reload raced
with the threads still reading the configuration, or left it unreadable.

- Listing pending settings changes (the settings API's *diff* query),
  printing the settings stores and the scheduler's settings house-keeping
  walked the list of included settings stores without its lock, while a
  reload emptied and refilled that list. A store could be freed under the
  walk: a use-after-free that crashes the agent. Each walk now works on a copy
  taken under the lock. An HTTP settings store that re-downloaded a changed
  file also emptied its list and only refilled it later, so its whole remote
  configuration briefly read as defaults; it now swaps the new copy in one
  step.
- A plugin reload cleared every included settings store, and an HTTP store
  never rebuilt what it held: from then on every value it supplied read as its
  default - security settings included, such as a permission policy the
  remote file enabled - until the remote file next changed. It now keeps
  the configuration it has across the reload. A newly downloaded copy that
  cannot be loaded (antivirus still holding the file, say), or that times out
  waiting for the store's lock, no longer replaces the previous one, and is
  tried again on the next pass.
- With the [permission policy](../concepts/permissions.md) enabled, a reload
  cleared the rule table and refilled it one rule at a time, so a check that
  arrived in between was denied. That failed closed, so nothing was let
  through, but every reload produced a burst of spurious denials. The table is
  now rebuilt aside and published in one step. A load that fails part-way
  used to leave whatever it had got to - usually an emptied table with the
  previous `allow exec` still in force, or the whole previous table, rules
  the operator had just removed included, when it failed while registering
  the settings keys. Now, under an enabled policy, the rules read before the
  failure are enforced, every other call is denied, and `allow exec` counts
  as `false` unless it was read, until the policy loads; registering the keys
  can no longer fail the load. A failure before `enabled` is read leaves the
  policy as it was - off on a host that never enabled it, enforced on one
  that did - and at boot that means off until a reload reads it.

None is known to have been exploited. Beyond crashing the agent or denying a
check, the only effect is the HTTP case above: settings read as their defaults
after a plugin reload.

**What to do:** nothing beyond upgrading, unless you enable the permission
policy: a reload that fails part-way now denies exec as well as every call
no rule read so far allows, so watch for `permissions: failed to load` in the
log, which names the failure.
