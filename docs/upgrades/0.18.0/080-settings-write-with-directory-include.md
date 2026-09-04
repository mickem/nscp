---
icon: "🔧"
modules: [core]
---
**Settings writes work again when `[/includes]` names a directory.** Saving
handed the directory path to the INI writer, and the resulting "Is a
directory" error aborted the whole save — so `nscp settings --set` and the
web UI failed *and the main file was never written*. Directory includes are
read-only by nature and are now skipped when saving. If you worked around
this by removing a directory include, you can put it back.
