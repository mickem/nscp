---
icon: "📤"
modules: [Scheduler]
action: conditional
---
**A schedule's `report` list now accepts spaces and any case.**
`report` was split on commas but not trimmed or lower-cased, so
`report = critical, warning` read the second entry as ` warning`, matched
nothing, and only critical results were submitted; `report = Critical` submitted
nothing at all. Each entry is now trimmed and compared without regard to case.
A schedule written that way starts submitting the states it lists - check the
receiving end if it was tuned to the smaller volume.
