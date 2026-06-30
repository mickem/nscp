# Project conventions

## Git commits
- Always add a DCO `Signed-off-by` trailer to every commit (this project requires DCO):
  `Signed-off-by: Michael Medin <michael@medin.name>`
  Equivalent to committing with `git commit -s`.
- The DCO bot requires the sign-off to match the commit author/committer.
  Set the git identity so author = committer = sign-off:
  `git config user.name "Michael Medin" && git config user.email "michael@medin.name"`
