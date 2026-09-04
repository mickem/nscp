---
icon: "🪟"
modules: [CheckSystem]
---
**`check_pending_reboot`'s default message now names the pending-since
time.** When the reboot was queued by Component Based Servicing or Windows
Update, the message gains a suffix: `Reboot required: Windows Update` became
`Reboot required: Windows Update (pending since 2026-08-16 09:41:12)`.
Notification pipelines that match the exact message text (an anchored regex,
a string equality) need their pattern relaxed; thresholds, states and
existing keywords are unchanged.
