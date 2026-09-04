---
icon: "💬"
modules: [core]
---
**Client commands shorter than eight characters work again.** A command
such as `cpu` or `run` answered `Exception processing command line:
basic_string::substr …` instead of running, in every module built on the
shared client machinery (NRPE, NSCA, NRDP, Graphite, …). Remove any
workaround that renamed such commands to a longer alias; no configuration
change is needed.
