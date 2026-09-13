---
icon: "🔧"
modules: [core]
action: none
---
**Fleet: `nscp enroll --unenroll` leaves the fleet.** Nothing to do. Until now
leaving meant deleting the enrollment manifest, the fleet directory and the
`[/includes] fleet` entry by hand; `nscp enroll --unenroll` does all three and
reports each, then a service restart stops the sync. It is a local act: the
host stays listed on the fleet server until you remove it there. See
[Central management with NSClient Fleet](fleet.md#step-6-living-with-it).
