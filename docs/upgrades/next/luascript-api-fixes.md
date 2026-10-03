---
icon: "🔧 🔒"
modules: [LUAScript]
action: conditional
---
**LUAScript: one broken script no longer takes the others down, and the documented API works as written.**
Nothing to do on a default install. A test of every documented Lua API turned up these, now fixed;
check your scripts against the ones marked *check*:

- A script that did not parse, or whose top-level code raised an error, failed the whole module: the
  core reported `Plugin refused to load: LUAScript` and no Lua script on the host ran. The error is
  now logged and the other scripts load. An `on_start` that raises no longer stops the scripts after
  it from starting.
- Handlers registered with `Registry:simple_cmdline` never ran - neither from
  `nscp client --module LUAScript --exec <name>` nor from `Core:simple_exec`. They now do.
- A reload of LUAScript now runs `on_start` again for the freshly loaded scripts (it ran only once per
  agent lifetime), and takes back the queries and channels of a script that was removed. *Check* a
  script whose `on_start` must only ever run once.
- *Check:* `Settings:get_bool` reads `true`, `1` and `yes` as `true`; it used to read the value as an
  integer, so `true` came back as the default. `Settings:set_bool` writes `true`/`false`.
- *Check:* `Core:simple_exec` returns `"unknown"` and `Failed to execute <command> on <target>` when
  nothing could run the command; it returned `"warning"` and `Command failed.`.
- *Check:* `Core:simple_submit` on a channel nobody listens to returns `false` and
  `Failed to submit message: <channel>`; it raised a Lua error.
- A check handler that returns no status answers `unknown` with
  `Invalid return from <command>: expected (code, message, perf)`; a missing message or performance
  data is empty, where it used to read `NIL`. A numeric status outside `0`-`3` reads as `unknown`.
- A call into the API with one argument too few raises `Incorrect syntax: ...`; the check counted the
  object itself as an argument, so the call went ahead with the arguments shifted by one.
- A subscription handler that raises fails the submission with
  `Failed to handle channel: <channel>: <error>` rather than with no answer at all.
- `Core:exec`, `Core:submit`, `Registry:cmdline` and `Registry:subscription` are still not
  implemented; the error they raise now names the call (`Unsupported API called: Core:exec`).
- `nscp lua show --script <name>` prints the script and `nscp lua delete --script <name>` deletes it
  and removes every `/settings/lua/scripts` entry that loads it. Both answered with nothing before,
  so `GET` and `DELETE` on `/api/v2/scripts/lua/<name>` returned an empty `200` and changed nothing.
  They act on files under `${scripts}/lua` only, symlinks resolved. Review custom roles that hold
  `scripts.get.LUAScript`, `scripts.delete.LUAScript` or `scripts.*`; see the
  [security notice](../security/notices.md#luascript-reading-and-deleting-scripts-over-rest-and-the-cli).
- `nscp lua add --import` asked for `--overwrite` when the file already existed; the option is
  `--replace`. `nscp lua` with no verb printed the `nscp py` usage.

[Lua scripting](../extending/lua.md) now documents `Core:query_target` and `Core:query_forward`, what
each handler answers when it fails, and the calls that are not implemented.
