---
icon: "🔧"
modules: [LUAScript, CheckMKClient, CheckMKServer]
action: conditional
---
**Lua `require()` and CheckMKClient scripts resolve from `${scripts}`.** Nothing
to do on a default install. `require()` in a Lua script now searches
`${scripts}/lua/lib/?.lua` and `${scripts}/lua/?.lua`, as
[Lua scripting](../extending/lua.md) always documented. It used to search
`${base-path}/scripts/lua/lib`, which is the same folder on a default Windows
install but not on a Linux package, where `${base-path}` is the directory
holding the binary: there `require()` of a shared helper in
`scripts/lua/lib` could not load before. CheckMKServer searched
`${scripts}/scripts/lua/lib`, and now uses the same two paths.

CheckMKClient's `scripts` entries are now looked up under `${scripts}`
(`${scripts}/lua/<entry>`, then `${scripts}/<entry>`) instead of under the
install base. An absolute path, or one relative to the working directory, is
found as before. Check your setup if:

- you override `${scripts}` and keep Lua helpers under
  `<install base>/scripts/lua/lib` - move them to `${scripts}/lua/lib`;
- a CheckMKClient `scripts` entry is a path relative to the install base, such
  as `scripts/lua/my_check.lua` - write it relative to `${scripts}`
  (`my_check.lua`) or as an absolute path. An entry that is not found is now
  logged as `Failed to find script: <entry>`; it used to be skipped silently.
