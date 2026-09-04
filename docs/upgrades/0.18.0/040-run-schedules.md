---
icon: "📅"
modules: [Scheduler]
action: none
---
**New: run scheduled checks on demand with `run_schedules`.** After editing
`nsclient.ini` you no longer have to wait out the interval to see the new
result on the monitoring server — `nscp client --boot --query run_schedules`
(optionally `--argument schedule=<alias>`) runs the configured schedules now
and submits their results on their normal channel. It is a regular check
command, so it also works over NRPE, REST and in `nscp test`. Nothing changes
for existing configurations; the timers are untouched. See
[Passive monitoring → Step 6](../scenarios/passive-monitoring-nsca.md#step-6-send-a-result-without-waiting-for-the-interval).
