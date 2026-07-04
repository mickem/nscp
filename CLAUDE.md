# Project conventions

## Git commits
- This project requires the [Developer Certificate of Origin](https://developercertificate.org/)
  (DCO): every commit must carry a `Signed-off-by` trailer with **your own** real
  name and email, for example:
  `Signed-off-by: Jane Doe <jane@example.com>`
  The easiest way is to commit with `git commit -s`, which appends the trailer
  from your configured git identity.
- The DCO bot accepts a `Signed-off-by` that matches **either** the commit's
  author **or** its committer, so signing off with your own identity on work you
  author is enough (and a cherry-picked commit that keeps the original author's
  sign-off still passes). If you do not already have a git identity configured
  (globally or for this repo), set one to your own name and email:
  `git config user.name "Jane Doe" && git config user.email "jane@example.com"`
  If you already have one, keep it and just make sure it is the name and email
  you sign off with.
- If an AI coding assistant helped produce a commit, declare it with an
  `Assisted-by:` trailer, using the format codified by the [Linux kernel AI
  coding-assistants policy](https://docs.kernel.org/process/coding-assistants.html):
  `Assisted-by: AGENT_NAME:MODEL_VERSION [tool ...]`, for example
  `Assisted-by: Claude Code:claude-opus-4-8`.
  Do **not** use `Co-authored-by:` for AI assistance: only a human may certify
  the DCO, so an AI must never carry a `Signed-off-by` (which co-authorship
  implies). Keep `Signed-off-by:` as the final trailer — a human always reviews,
  tests, and takes responsibility for the result.

## Check command options
- Boolean check options must be declared as
  `po::value<bool>(&x)->implicit_value(true)->default_value(false)`,
  **not** `po::bool_switch`. Checks are driven over REST, which passes the flag
  as the token `x=true`; `bool_switch` rejects that with "does not take any
  arguments". This only fails over REST, not from the CLI, so it is easy to ship
  broken — cover new boolean options with an integration test.
- Argument passing differs by transport: the CLI passes each `-a key=value` as
  two separate tokens (`--key` then `value`, plus a redundant undashed pair),
  while REST passes a single `key=value` token. Prefer the filter/option helpers
  over hand-rolled argument parsing.

## Adding a new check module
- Modules are auto-discovered: a `modules/<Name>/module.cmake` containing
  `set(BUILD_MODULE 1)` is enough (no top-level CMake edit). Re-run `cmake` on an
  existing build dir to pick up a new module directory.
- `module.json` declares the module and its `commands`. The build **generates**
  the dispatch/export glue (`module.cpp/hpp`) from it: `name` maps to a class
  `<Name>` in `<Name>.h/.cpp`, and every `commands` entry maps to a method of the
  **same name** on that class. Omit the `"metrics"` key unless the class
  implements `fetchMetrics()` — `"metrics":"produce"` generates a call to it and
  will fail to link otherwise.
- Cross-platform data acquisition uses the win/unix shim: platform-neutral
  sources plus an `if(WIN32) … _win.cpp else() … _unix.cpp` split in
  `CMakeLists.txt`, behind a shared filter/interface header (see `CheckDisk`).
  Keep the check logic, keyword registry and output builders platform-neutral;
  only the data source is `#ifdef`'d.
- Packaging: modules self-install via `NSCP_INSTALL_MODULE()` (pulled in by
  `include(${BUILD_CMAKE_FOLDER}/module.cmake)` in the module's `CMakeLists.txt`),
  so Linux CPack (DEB/RPM/ZIP) packages them automatically. The **Windows MSI
  does not** — add a `<File>` entry to `installers/installer-NSCP/Product.wxs`
  under `<Component Id="Plugins">` (the "Check Plugins" feature). Also add the
  module to the feature-hint map in `service/plugins/plugin_manager.cpp`, and
  (optionally, commented) to `files/NSC.dist`.
- A check is a `modern_filter` check: a `filter_obj` + a `filter_obj_handler`
  registering keywords (`registry_.add_string_var` / `add_int_var(...,type,...)`
  with `type_int`/`type_date`/`type_bool`, chaining `.add_int_perf("unit")` for
  perfdata), driven by `modern_filter::cli_helper` (`add_options(warn, crit,
  filter, syntax, empty_state)` + `add_syntax(top, detail, perf, empty, ok)`).
  See `CheckDisk/check_single_file.cpp` for a minimal template.
- Unit-test binaries have no generated module glue, so they must define the
  plugin singleton themselves (normally provided by `NSC_WRAP_DLL()`):
  `nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();`

## Documentation for new commands
Every new check command needs, under `docs/samples/`:
- `<Module>_<command>_samples.md` — usage examples with real captured output.
- `<Module>_<command>_desc.md` — clarifying prose (suffix is `_desc`, not `_docs`).

These are merged into the reference docs by the downstream `nscp-docs` build.
Scenario walkthroughs live in `docs/docs/scenarios/` and must be registered in
both `docs/mkdocs.yml` (nav) and `docs/docs/scenarios/index.md`.

## Integration tests
Integration tests live under `tests/` (jest + ts-jest) and drive commands over
REST against a long-lived `nscp test` instance. Run them from `tests/`:
`NSCP_SKIP_DOCKER=1 NSCP_BIN=<path>/nscp npx jest --runInBand <pattern>`.

For a check that only needs its command exercised (and where standing up the
REST web server is undesirable or unavailable), the one-shot **client-query**
path works without a web server: `nscp client --module <Mod> --boot --query
<cmd> <k=v>…` (see `checkdisk-unix.test.ts`, `checksecurity.test.ts`). It still
passes `k=v` as single tokens, so it exercises the same REST-style argument
parsing; note its output is the raw Nagios `message|perfdata` with no
status-word prefix added.

## Changelog / release notes
Release notes follow the style of the GitHub releases (e.g.
https://github.com/mickem/nscp/releases/tag/0.12.5). Derive entries from
`git log <last-tag>..HEAD` and describe user-visible behaviour, not commits.
Structure:

```markdown
# <descriptive one-line title of the release theme>

<1–2 sentence intro paragraph summarising the major changes.>

## Highlights
- **<Bold lead-in>.** <One or two sentences.> Reference GitHub issues as `#NNN` when relevant.
- ... (4–8 bullets covering the headline items)

## Detailed changes

### <Area/module> — <short subject>
<Problem → solution prose.> Use tables for keyword/option/function lists and
fenced code blocks for command / INI examples. One `###` per notable change;
group minor items under a shared subsection (e.g. `### Bug fixes`).

## Upgrade notes
- **<Breaking change / migration>:** <what changed and what the user must do.>
  Call out breaking changes explicitly; note when the default install is unaffected.

**Full Changelog**: https://github.com/mickem/nscp/compare/<prev-tag>...<this-tag>
```

Conventions: title is a theme sentence, not a version number; lead each highlight
bullet with a **bold** phrase; prefer tables for option/keyword matrices; always
end with the compare link.
