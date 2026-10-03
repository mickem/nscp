---
title: "Settings reload races: included settings stores and the permission table"
fixed_in: next
severity: "Low; Medium where an HTTP settings source enables the permission policy"
modules: [core]
action: none
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
  remote file enabled - until the remote file next changed. It now rebuilds
  on its cached copy. A copy that cannot be loaded (antivirus still holding
  the file just downloaded, say) no longer replaces the previous one, and is
  tried again on the next pass.
- With the [permission policy](../concepts/permissions.md) enabled, a reload
  cleared the rule table and refilled it one rule at a time, so a check that
  arrived in between was denied. That failed closed, so nothing was let
  through, but every reload produced a burst of spurious denials. The table is
  now rebuilt aside and published in one step. A load that fails part-way
  still fails closed: the rules read before the failure are enforced, every
  other call is denied, and `allow exec` counts as `false` unless it was read,
  until the policy loads. Only an `enabled = false` that was actually read
  turns the policy off.

None is known to have been exploited. Beyond crashing the agent or denying a
check, the only effect is the HTTP case above: settings read as their defaults
after a plugin reload.

**What to do:** nothing beyond upgrading.
