#!/bin/bash
# Cloud sessions commit as the agent (committer Claude <noreply@anthropic.com>),
# so the DCO sign-off has to come from the human author configured in the
# person's own cloud environment. Install the commit-msg hook that enforces
# it, and tell the agent whose identity it is committing under.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

cd "$CLAUDE_PROJECT_DIR"
git config core.hooksPath tools/git-hooks

if [ -n "${GIT_AUTHOR_NAME:-}" ] && [ -n "${GIT_AUTHOR_EMAIL:-}" ]; then
  echo "DCO: commits in this session are authored and signed off by" \
       "$GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> (from the cloud environment);" \
       "the commit-msg hook adds the Signed-off-by, so do not use git commit -s."
else
  echo "DCO: GIT_AUTHOR_NAME / GIT_AUTHOR_EMAIL are not set in this cloud" \
       "environment, so the commit-msg hook will refuse commits. Before the" \
       "first commit, ask the user for the name and email they sign off with" \
       "and commit with --author=\"Name <email>\"; suggest they add both" \
       "variables to their environment settings."
fi
