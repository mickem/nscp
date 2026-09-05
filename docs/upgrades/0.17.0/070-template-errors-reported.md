---
icon: "🧩"
modules: [filters]
action: conditional
---
**Errors raised while a template renders are now reported.** A function that
failed inside `detail-syntax` or `top-syntax` used to leave the placeholder
empty and say nothing; the check now returns UNKNOWN with `Filter processing
failed: …`. This surfaces template mistakes that have been silently producing
incomplete messages.
