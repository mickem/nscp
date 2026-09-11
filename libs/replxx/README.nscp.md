# replxx (vendored)

Upstream: <https://github.com/AmokHuginnsson/replxx>
Revision: `1f149bfe20bf6e49c1afd4154eaf0032c8c2fda2` (master, 2021-11-25 — 15
commits after `release-0.0.4`, which is upstream's last activity).
License: BSD-3-Clause, see [LICENSE.md](LICENSE.md) — with one file under
other terms that upstream embeds and documents there: `src/wcwidth.cpp`
(Markus Kuhn, `HPND-Markus-Kuhn`). Both licenses are annotated per file in
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

The tree under `include/` and `src/` is a copy of upstream's `include/` and
`src/` with one change, described below. `CMakeLists.txt` and everything under
`nscp/` are ours — upstream's build file carries install/export/packaging rules
and a shared-library option we do not want; ours builds a plain static library
in the project's own style.

**`src/ConvertUTF.{cpp,h}` is not carried.** Upstream embeds Unicode, Inc.'s
2001-2004 UTF conversion routines, whose notice grants use only "in the creation
of products supporting the Unicode Standard" and says nothing about
modification. That is not DFSG-free, so it keeps the Debian package out of main
and Lintian flags it as `license-problem-old-unicode` — the same complaint that
SimpleIni's copy drew in #1278 and that #1294 settled by replacing the code.
This copy does the same: `nscp/utf8_conversion.{cpp,hpp}` is our own UTF-8 <->
UTF-32 conversion, written from the encoding definition in the Unicode standard,
and `src/conversion.hxx` includes it in place of `"ConvertUTF.h"` — that
include, plus the `<string>` the dropped header used to pull in for
`unicodestring.hxx`, is all that differs from upstream. Its section of
`LICENSE.md` went with the file.
`nscp/utf8_conversion_test.cpp` covers it; see the header for the two malformed
sequences the replaced code accepted and this one rejects.

Updating is therefore a re-copy plus that one line:

```sh
git clone https://github.com/AmokHuginnsson/replxx.git
cp replxx/include/replxx.h replxx/include/replxx.hxx libs/replxx/include/
cp replxx/src/* libs/replxx/src/
cp replxx/LICENSE.md libs/replxx/
rm libs/replxx/src/ConvertUTF.cpp libs/replxx/src/ConvertUTF.h
# then re-apply the include swap in src/conversion.hxx and drop the
# ConvertUTF.cpp section of LICENSE.md again
git diff libs/replxx
```

then update the revision above. `tools/license_headers.py` skips the upstream
sources so it does not rewrite them, and `build/python/format-code.py` skips the
whole directory — `nscp/` included, so keep those files in the project's style by
hand.
