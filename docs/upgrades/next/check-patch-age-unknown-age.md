---
icon: "🩹"
modules: [CheckSystem]
action: conditional
---
**`check_patch_age` reports an unknown age as `unknown`, with an `age_known` flag.**
When Windows recorded no parseable install date for any hotfix, `age` used to
be `-1`. That slipped under every `age > N` threshold, so a host that reports
no dates looked freshly patched. `age` now has no value in that case: it
renders as `unknown`, emits no `patch_age` perfdata, and every numeric
comparison on it is false. The new `age_known` keyword is `1` when the age is
known and `0` when it is not. Replace any `age < 0` or `age = -1` test with
`age_known = 0`, and add `or age_known = 0` to an age threshold that should
also fire on a host without install dates. Nothing to do otherwise.
