---
icon: "🔧"
modules: [PythonScript]
action: conditional
---
**PythonScript: `plugin_alias`, `Registry.event`, and showing or deleting a script.**
Nothing to do on a default install. A test of every documented scripting API
turned up these, now fixed:

- `init()` receives `python` as `plugin_alias` when the module is loaded
  without an alias (`PythonScript = enabled`), as
  [Python scripting](../extending/python.md#init) documents. It used to receive
  an empty string. Check your scripts if one compares `plugin_alias` with `""`
  or builds a settings path from it.
- A script that registered a handler with `Registry.event` crashed the agent
  on the first event it subscribed to. `Registry.event_pb` handlers never ran
  at all; they now run, and receive the event as `bytes`.
- A `Registry.simple_subscription` handler is given the submission's source
  (for example `check_and_forward`'s `source=`); it used to receive an empty
  string unless the result carried one of its own.
- `nscp py show --script <name>` prints the script and `nscp py delete --script
  <name>` deletes it and removes it from `/settings/python/scripts`. Both
  answered with nothing before, so `GET` and `DELETE` on
  `/api/v2/scripts/py/<name>` returned an empty `200` and changed nothing.
  They now act, on files under `${scripts}/python` only. Review custom roles
  that hold `scripts.delete.PythonScript` (or `scripts.*`) if that grant was
  not meant to remove files.
