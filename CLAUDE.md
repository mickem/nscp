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
