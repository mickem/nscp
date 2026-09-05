---
icon: "📤"
modules: [CheckHelpers]
action: conditional
---
**`check_and_forward` now actually submits the result.** The command ran the
wrapped check, answered `Message submitted` and delivered nothing: the
submission was built in a form the channels could not read, so NSCA, NRDP,
Graphite and every other client module received a message with no results in
it. It now submits like a scheduled check does. If you had given up on the
command, it works — and if you built a workaround around its silence (for
instance a schedule that exists only to be triggered), that workaround is no
longer needed. The command also gained `channel` (`target` still works as a
synonym), `alias`, `destination` and `source` options, and names the result
after the command when no alias is given.

**Breaking:** the command no longer accepts positional arguments for the
wrapped command. `check_and_forward command=check_cpu warn=load>80` used to
(try to) hand the bare trailing tokens to the wrapped command; it now fails
to parse. Pass each wrapped-command argument through `arguments=` instead,
one per argument:

```
check_and_forward command=check_cpu "arguments=warn=load>80" "arguments=crit=load>90"
```

The positional form had to go because its parser also swallowed the CLI's
own `--argument key=value` tokens, which is what fed the wrapped command
garbage and made it fail. If a submission is rejected by the channel the
command now reports that failure instead of `Message submitted` — including
when only one channel of a comma list (`channel=NSCA,GRAPHITE`) fails,
which previously was silently reported as success.
