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
