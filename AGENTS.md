# Agents

## Working on this project

- Update `SPEC.md` when changing behavior.
- Update `README.md` when changing user-facing features.
- Add tests for new functionality and run tests before committing.

## Testing

- **Any change to executable behavior adds or updates a test.** New
  functionality gets a test that exercises its behavior; a bug fix gets a
  regression test that fails before the fix and passes after. Changes with no
  behavior to exercise — documentation, comments, this file — add no test;
  don't manufacture test churn to satisfy the rule. Run the suite either way.
- Run `make test` (`cargo test --release`) after any change and before
  committing. **The top-level target does not cover `legacy/`** — the C
  implementation has its own suite (`loggingtest`, `pathtest`, `argstest`), so
  a change under `legacy/` also needs `make -C legacy test`. That code is the
  setuid fallback; an untested change there is a security change.
- **Fix any preexisting test failures as the *first* commit of the series.**
  Don't stack new work on a red baseline. If the failure is genuinely
  unrelated and out of scope, say so up front and confirm before skipping it.
- **Don't paper over flaky/racy tests** with sleeps, retry loops, or bumped
  timeouts. Make the ordering explicit, or fix the underlying race. A test
  that passes "most of the time" is broken.
- **Don't disable a failing check** (a test, `cargo clippy`, a lint) to make
  it pass — fix the underlying issue. This ships as a setuid binary, so a
  check silenced for convenience is a privilege-escalation bug waiting to
  happen.

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
- **One commit per logical change.** Rewrite unmerged commits freely — amend,
  `git commit --fixup` + autosquash, squash, reorder, split — so each commit
  that lands is one coherent change, with fix-ups and review responses folded
  into the commit they belong to. `wip` / `address review` churn doesn't
  survive into `main`.
- **These rules assume an `origin` remote.** Without one you can't fetch,
  branch from `origin/main`, push, or open a PR — say so and stop rather than
  improvising a local substitute. **Exception:** in a sandbox that
  intentionally provides no remote Git support (Codex cloud, say), follow the
  normal branch rules from the current `HEAD` — a pre-created working branch
  counts — commit locally, and report that fetch, push, and pull requests are
  unavailable, using the sandbox's own PR handoff if it has one. That exception
  outranks every `origin`-dependent step below it — the merge-cue fetch, cutting
  a branch off `origin/main`, the closing PR link — so work from the current
  `HEAD` and name what wasn't possible instead of faking it. One limit: a merge
  cue needs a base that *contains* the merge, and an offline sandbox can't fetch
  one. Say the follow-up needs a fresh sandbox or a synced checkout rather than
  branching off a `HEAD` whose commits just landed upstream.
- **Branch naming.** Feature branches are prefixed with the agent's own short
  name: `<agent>/<short-topic>` (`claude/...` for Claude Code, `codex/...` for
  Codex, and so on). Branch off `origin/main`, one topic per branch; never
  commit to `main`. The placeholder `<agent>` stands in for whichever prefix
  you use — don't hard-code `claude/` unless you *are* Claude Code.
- **Merge cue (`merged` / `I merged` / `landed` / merge webhook) runs hygiene
  *before* engaging with the rest of the message:** `git fetch origin`, cut a
  fresh `<agent>/<short-topic>` branch off `origin/main`, announce the switch.
- **After a merge, take a fresh `<agent>/<short-topic>`** — don't reset the
  merged name onto the new base. Its remote ref still points at the pre-merge
  tip, so `origin/<branch>..HEAD` keeps spanning the merged commits and
  unpushed-work checks report your own merged history back at you. When a
  sandbox pins the branch name, reset it and `--force-with-lease` in the same
  turn — that's routine on merged history, not something to ask about.
- **The agent authors and the repo owner merges**, so a squash or rebase merge
  rewrites the committer to them. That's expected — never re-author or amend
  already-merged commits to "fix" authorship or signing.
- **Unshallow before answering anything that depends on git history depth.**
  The sandbox clones shallow, so `git rev-list --count`, `git log` past the
  shallow boundary, and blame return wrong answers without warning. If
  `git rev-parse --is-shallow-repository` says `true`, run
  `git fetch --unshallow` first, then re-check — it exits 0 even when
  it deepened nothing, so if `--is-shallow-repository` is still `true`, say the
  history is truncated instead of quoting a count.

## Error handling

- **Don't silently swallow errors.** A discarded exit status, a bare
  `2>/dev/null`, or an empty catch hides real failures and burns hours when
  something eventually breaks. Report the failure with enough context to
  identify what failed and why, clean up whatever the failed step created, and
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
- **Program output and the syslog audit trail are not those artifacts.** Output
  prints on the user's own terminal. The audit record goes to whoever
  administers the machine — often forwarded to a central collector, so don't
  assume it stays local — and that reader is entitled to it: naming the calling
  user and the resolved command is the whole point of an `LOG_AUTHPRIV` record,
  and scrubbing those two fields would defeat the auditability this tool exists
  to provide. They are also the *only* fields exempt. Arguments are deliberately
  outside the record (`src/main.rs` logs the resolved path only) because they can
  carry a password or an API key with no redaction mechanism to catch it, and
  forwarding is precisely why widening the record to arbitrary content would be
  a mistake. Quoting output into a commit, PR, or fixture republishes it, and the
  bullet above governs again.

## Pull requests

- **On every push, update the PR title and body** so they describe the full,
  latest state of the branch — not the scope it had when it was opened.
  Re-read the diff against `origin/main` and patch whatever drifted, then post
  the PR link in the chat reply for that push, not only at the end of the
  conversation.
- **"Drive to merge"** is shorthand for the whole loop: open the PR, wait for
  the automatic Codex review, address every review comment — fix it if you
  agree, reply on the thread saying why if you don't — and merge once CI is
  green and Codex has left its thumbs up.
- **Codex is the automated reviewer** — not Copilot. Its reviews are triggered
  automatically; you don't request them. Address its comments without being
  asked, folding each fix into the commit it belongs to rather than tacking on
  an "address review" commit.
- **Judge every review comment on merit, whoever wrote it.** Verify the claim
  before acting; if it doesn't hold up, reply saying why and decline.
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
- When a feature has multiple open PRs, list **every** open PR by URL, one per
  line — the "View PR" chip sticks to the first link and hides the rest
  (anthropics/claude-code#46625).
- End every reply with the open-PR link (or `.../compare/main...<branch>`
  until a PR exists). Never link to a closed or merged PR — except when the
  reply *is* post-merge follow-up on that PR, where linking it is correct. In an
  offline sandbox with no `origin` there's no URL to end with — say that, rather
  than inventing a link that resolves to nothing.

## Language and spelling

- Use **US English** everywhere people read English: user-facing output and
  the man page, commit subjects and bodies, PR titles and descriptions,
  comments, docs (`SPEC.md`, `README.md`), and identifiers — `color` not
  `colour`, `behavior` not `behaviour`, `canceled` not `cancelled`, `gray`
  not `grey`. Third-party API spellings stay as those APIs spell them.

## Cost and reliability

- **Call out cost and reliability up front** when recommending a new
  dependency or external call. Include a rough dollar figure where one
  applies, and note reliability implications: new failure modes, added
  latency, and extra points of failure. On a setuid binary a new dependency
  is also new attack surface running as root — say so explicitly, and prefer
  the standard library where it will do. If the impact is effectively zero,
  say so rather than omitting the note.
