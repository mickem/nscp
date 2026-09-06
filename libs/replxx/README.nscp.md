# replxx (vendored)

Upstream: <https://github.com/AmokHuginnsson/replxx>
Revision: `1f149bfe20bf6e49c1afd4154eaf0032c8c2fda2` (master, 2021-11-25 — 15
commits after `release-0.0.4`, which is upstream's last activity).
License: BSD-3-Clause, see [LICENSE.md](LICENSE.md) — with two files under
other terms that upstream embeds and documents there: `src/wcwidth.cpp`
(Markus Kuhn, `HPND-Markus-Kuhn`) and `src/ConvertUTF.{cpp,h}` (Unicode, Inc.,
`LicenseRef-Unicode-ConvertUTF`). All three are annotated per file in
`REUSE.toml`; `reuse lint` covers every file in this directory.

replxx is a readline/libedit replacement that works on both POSIX terminals and
the Win32 console. NSClient++ uses it for the interactive prompt in
`nscp test` (see `modules/CommandClient/`): line editing, persistent history,
tab completion against the command registry, syntax highlighting and hints.

The property that made it the pick over linenoise and friends is
`Replxx::print()`: called from a thread other than the one blocked in
`input()`, it queues the text and wakes the input loop, which redraws the
prompt around it. NSClient++ logs from a background thread the whole time the
prompt is up, so without that the log would smear over whatever the user is
typing.

## Local changes

None. The tree under `include/` and `src/` is a verbatim copy of upstream's
`include/` and `src/`. Only `CMakeLists.txt` is ours — upstream's carries
install/export/packaging rules and a shared-library option we do not want; ours
builds a plain static library in the project's own style.

Keeping the copy verbatim means updating is a straight re-copy:

```sh
git clone https://github.com/AmokHuginnsson/replxx.git
cp replxx/include/replxx.h replxx/include/replxx.hxx libs/replxx/include/
cp replxx/src/* libs/replxx/src/
cp replxx/LICENSE.md libs/replxx/
```

then update the revision above. The sources are excluded from
`build/python/format-code.py` and `tools/license_headers.py` so neither tool
rewrites them.
