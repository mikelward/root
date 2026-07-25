# Agents

## Working on this project

- Update `SPEC.md` when changing behavior.
- Update `README.md` when changing user-facing features.
- Add tests for new functionality and run tests before committing.

## Talking to the user

- **One question at a time.** Never stack multiple questions in a single turn —
  ask the most important one, wait for the answer, then ask the next if you
  still need it.
- **Don't interrupt.** Never fire off a question while the user is still
  typing. Let them finish; a half-typed message isn't an invitation to jump in.
- **Keep replies short — don't dump a full page.** Lead with the single most
  important point and stop. If there's more, say the first point and ask
  whether they're ready for the next one.

## Asking questions

- Ask questions as plain chat messages. Claude specifically: never use
  `AskUserQuestion`, Claude Code's multiple-choice question prompt — it's
  broken in the Claude mobile app, so a question asked through it may be
  unanswerable. Chat keeps the question, its context, and the answer in one
  readable thread.
- After asking, stop and wait for the answer. Don't proceed on an assumed
  answer, pick a "recommended" option yourself, or keep working on the part the
  question affects.

## Git

- Use `git worktree` when it's available. Give each branch its own worktree
  instead of switching branches in place, so work in progress on one branch
  isn't disturbed by work on another.

## Error handling

- **Don't silently swallow errors.** A discarded exit status, a bare
  `2>/dev/null`, or an empty catch hides real failures and burns hours when
  something eventually breaks. Report the failure with enough context to
  identify what failed and why — sanitized context only, since a message can
  easily carry a hostname, a token, or a path with the user's real name, and
  the Privacy rule applies to logs too — clean up whatever the failed step
  created, and
  decide explicitly what the caller sees rather than letting control fall
  through. If you genuinely want to ignore a specific failure, name the reason
  in a one-line comment.

## Privacy

- **Never put user data in any artifact that leaves this machine** — commit
  subjects and bodies, PR titles / descriptions / comments, review replies,
  branch names, code comments, or test fixtures. That covers hostnames,
  absolute paths containing the user's real name, private remote URLs, tokens,
  and shell or command history. Use generic placeholders (`/home/user`,
  `host1`) in examples and fixtures. If a bug report contains any of it,
  paraphrase in the commit / PR — don't quote verbatim.

## Pull requests

- **"Drive to merge"** is shorthand for the whole loop: open the PR, send it
  for Codex review, address every review comment — fix it if you agree, reply
  on the thread saying why if you don't — and merge once CI is green and Codex
  has left its thumbs up.
- **Codex is the automated reviewer** — not Copilot. Its reviews are triggered
  automatically; you don't request them. Address its comments without being
  asked, folding each fix into the commit it belongs to rather than tacking on
  an "address review" commit.
- **Never leave a review comment thread silently dismissed.** Either reply on
  the thread *or* resolve it; when you think a comment is a false positive, say
  *why* on the thread. `resolve_review_thread` works — pass the `PRRT_*` thread
  node ID from `pull_request_read` / `get_review_comments`
  (`review_threads[].id`); a comment's `PRRC_*` ID fails. Push the fix first,
  then reply citing the new sha, then resolve.
- **Skip echo events silently.** Replies posted via the GitHub MCP come back
  moments later as webhook events authored by the same identity; if the body
  matches a comment you just posted, it's your own echo — continue without
  comment.
- **Keep watching merged PRs for late review comments.** Stay subscribed after
  the merge and handle each new comment per the reply-or-resolve rule; stop
  once every post-merge comment is handled or after ~24h of silence.
