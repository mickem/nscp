---
icon: "🎚️"
modules: [WEBServer]
action: none
---
**The web UI's check page has a simple mode that starts you off with sample thresholds.**
A new *Simple mode* switch on a check's page under `Queries` writes a sample
`warning=` and `critical=` argument into the `Arguments` field - the check's
own default thresholds, spelled out where the numbers can be edited
(`"warning=load > 80" "critical=load > 90"` for `check_cpu`). A few checks
whose right value depends on the machine get a prepared sample instead
(`check_load`, `check_swap_io`), and a check that only reports says that it
has nothing to sample. A threshold already typed is left alone, switching the
mode off takes the untouched sample out again, and the choice is remembered
per browser. It only fills in the field: nothing is saved to the configuration
and checks run from a monitoring server are unaffected. See
[Simple mode](web-interface.md#simple-mode-start-from-sample-thresholds).
Nothing to do.
