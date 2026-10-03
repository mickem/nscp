---
title: "Settings reload races: included settings stores and the permission table"
fixed_in: next
severity: "Low"
modules: [core]
action: none
---
A concurrency review of the core found two places where a settings reload
raced with the threads still reading the configuration.

- Listing pending settings changes (the settings API's *diff* query),
  printing the settings stores and the scheduler's settings house-keeping
  walked the list of included settings stores without its lock, while a
  reload emptied and refilled that list. A store could be freed under the
  walk: a use-after-free that crashes the agent. Each walk now works on a copy
  taken under the lock. An HTTP settings store that re-downloaded a changed
  file also emptied its list and only refilled it later, so its whole remote
  configuration briefly read as defaults; it now swaps the new copy in one
  step.
- With the [permission policy](../concepts/permissions.md) enabled, a reload
  cleared the rule table and refilled it one rule at a time, so a check that
  arrived in between was denied. That failed closed, so nothing was let
  through, but every reload produced a burst of spurious denials. The table is
  now rebuilt aside and published in one step.

None is known to have been exploited, and none is known to do more than crash
the agent or deny a check.

**What to do:** nothing beyond upgrading.
