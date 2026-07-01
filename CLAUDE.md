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
