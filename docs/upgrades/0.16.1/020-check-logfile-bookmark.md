---
icon: "📄"
modules: [CheckLogFile]
---
**`check_logfile`** is unchanged unless you opt in to `bookmark` / `max-lines`.
Adopting `bookmark` is a trade-off: a line is consumed when the check runs
(not when its result is submitted) and positions are saved on clean shutdown,
so a crash re-reports the backlog. Prefer an explicit bookmark name for a
check whose filter changes often.
