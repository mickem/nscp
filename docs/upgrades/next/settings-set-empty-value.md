---
icon: "🔧"
modules: [core, LUAScript, PythonScript, DotnetPlugins]
action: conditional
---
**Setting a key to an empty value from a module or script now clears it instead of deleting it.**
A settings update with an empty value was read as a removal, so
`set_string(path, key, "")` - from a C++ module, a Lua or Python script, or a
.NET plugin - deleted the key and the next read fell back to the default.
Setters now mark the update as a set (`store_empty` on the settings `Update`
message), and the key is stored with an empty value. Removal is unchanged:
`remove_key`, `erase` and REST `DELETE` still delete, and an update without
the flag - every sender built against an older API, and REST `PUT` with an
empty `value` - still removes as before. A script that wrote an empty string
to get a key back to its default should call the new `remove_key(path, key)`
instead, which Lua (`config:remove_key`) and Python (`config.remove_key`)
scripts now have alongside `set_string`.
