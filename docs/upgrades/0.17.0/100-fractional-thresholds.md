---
icon: "🔢"
modules: [filters]
action: conditional
---
**Fractional numbers in thresholds are no longer truncated or rounded.**
`count > 2.5` used to evaluate as `count > 3` (the literal was rounded
into the counter's integer domain); unit literals lost their fraction
entirely, so `working_set > 1.5g` meant 1g and `uptime < 2.5h` meant 2h.
Fractions now mean what they say. Whole-number thresholds are unchanged;
only expressions that already used a decimal point can behave differently.
