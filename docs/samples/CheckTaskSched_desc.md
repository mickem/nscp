## Scheduled task facts

```ini
[/settings/task schedule/facts]
tasks.scheduled = true
```

If the module is loaded under a custom alias, use
`[/settings/<alias>/facts]` instead of `[/settings/task schedule/facts]`.

This opt-in inventory includes disabled and hidden local tasks in every folder.
The agent's account must have permission to enumerate every folder and read each
returned task (normally the LocalSystem service account). An access-denied error
fails collection and retains the last successful inventory; inaccessible folders
are not skipped. Each record has the full task path as its `id`, plus `name`,
`folder`, `enabled` and `hidden` (omitted on the legacy API). Actions, arguments,
accounts and run results are excluded. Collection starts on the first scheduled
facts round or a manual `facts refresh`. See [Host Facts](../../concepts/facts.md#tasksscheduled)
for limits and failure behavior. Turning the switch off on reload removes the set.
