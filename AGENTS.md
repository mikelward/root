---
trigger: always_on
alwaysApply: true
last_modified: 2026-10-26
---

# Agents

`CLAUDE.md` and `GEMINI.md` are symlinks to this file, so every agent reads the
same conventions. Edit `AGENTS.md`.

**At the start of every session, print the full absolute path of the `AGENTS.md`
you loaded and its `last_modified` date** (front matter), so a stale or wrong
copy is caught before it steers the work. Bump `last_modified` whenever you edit
this file.

Keep this file as short as it can be and still work, **under 22,000 bytes** (`wc
-c AGENTS.md`): Antigravity caps rule files at 24 KB and silently truncates the
tail. Every session loads it whole: add a rule the first time something bites,
say it once in the fewest words that carry the *why*; rewrite or trim rather
than append; delete one that has stopped biting.

## Working on this project

- Update `SPEC.md` when changing behavior and `README.md` when changing
  user-facing features; tests as *Testing* says.

## Testing

- **Any change to executable behavior adds or updates a test**: a test of new
  functionality, a regression test for a bug fix (fails before, passes after). A
  change with no behavior to exercise (docs, comments, this file) adds none —
  don't manufacture churn — but run the suite either way.
- Run `make test` (`cargo test --release`) after any change and before
  committing. **It does not cover `legacy/`**: the C implementation has its own
  suite (`loggingtest`, `pathtest`, `argstest`), so a change there also needs
  `make -C legacy test`. That code is the setuid fallback; an untested change
  there is a security change.
- **Fix any preexisting test failure as the *first* commit of the series** —
  don't stack work on a red baseline; if it's genuinely unrelated and out of
  scope, say so up front and confirm before skipping it.
- **Don't paper over flaky/racy tests** with sleeps, retries or bumped timeouts
  — make the ordering explicit or fix the race; a test that passes "most of the
  time" is broken.
- **Don't disable a failing check** (a test, `cargo clippy`, a lint) to make it
  pass — fix the underlying issue. This ships as a setuid binary, so a check
  silenced for convenience is a privilege-escalation bug waiting to happen.

## Talking to the user

- **One question at a time**, in plain chat or the client's question tool
  (`AskUserQuestion` in Claude Code, `ask_question` in Antigravity), the most
  important first. After asking, stop and wait: don't proceed on an assumed
  answer, pick a "recommended" option yourself, or keep working on the part it
  affects. Never ask while the user is still typing.
- **Answer a mid-turn message first** — the "sent while you were working"
  interjection — in your very next output, before any further tool call, even if
  it's one sentence. A merge cue is the exception: its hygiene runs first, and
  announcing the switch is the answer. **Claude Code only** (maintainer,
  2026-10-25): its app reliably shows just a turn's last message, so reply only
  there, answering a mid-turn message at a stopping point soon.
- **Message only when the user needs to see it** (maintainer, 2026-10-15): each
  one notifies their phone. Work and listen in silence; speak up for a question
  or blocker, something to merge or act on, a merge, an autopilot decision (to
  review when seen, never waited on), or a change from the plan, `SPEC.md` or
  their instructions, review-prompted ones included. Never for progress (a
  client's nudge doesn't count), a review or push alone (its report rides the
  next message), routine machinery, or a mistake caught and fixed before it
  reached the repo. The session-start `AGENTS.md` line still opens every
  session.
- **Keep replies short**: lead with the single most important point and stop; if
  there's more, ask whether they're ready for the next one.
- **End the turn by restating any pending decision** — a question you asked, or
  a guess autopilot recorded for review — as the reply's last line, written out
  in about a sentence, every turn until it's answered; "as asked above" isn't
  actionable. Nothing pending, no line.

## Git

- Use `git worktree` when it's available — one per branch, so work in progress
  on one branch isn't disturbed by another.
- **One commit per logical change.** Rewrite unmerged commits freely (amend,
  `--fixup` + autosquash, squash, reorder, split), folding fix-ups and review
  responses into the commit they belong to; `wip` / `address review` churn
  doesn't survive into `main`.
- **Title commits like a changelog line** (maintainer, 2026-10-23): after any
  prefix, an imperative verb, then the change as someone running `root` sees it,
  plain English, sentence case, ≤ ~70 chars, e.g. `Fix …` for what was broken,
  `Improve …` for what got better, `Add …` for what's new; an optional
  parenthetical says how. Never the scenario or the mechanism (that's the body).
  PR titles match.
- **These rules assume an `origin` remote**; without one, say so and stop
  rather than improvise. **Exception:** a sandbox with intentionally no remote
  Git (Codex cloud) follows the branch rules from the current `HEAD` (a
  pre-created branch counts), commits locally and reports fetch, push and PRs
  unavailable (using its own PR handoff if any), outranking every
  `origin`-dependent step below; name what wasn't possible, don't fake it. A
  merge cue needs a base *containing* the merge, so offline, say the follow-up
  needs a fresh sandbox or synced checkout.
- **Branch naming**: `<agent>/<short-topic>` (`claude/...` for Claude Code,
  `gemini/...` for Gemini / Antigravity, `codex/...` for Codex, and so on), off
  `origin/main`, one topic per branch; never commit to `main`. Don't hard-code
  `claude/` unless you *are* Claude Code.
- **Merge cue (`merged` / `I merged` / `landed` / merge webhook) runs hygiene
  *before* engaging with the rest of the message:** fetch main if the sandbox
  can (`git fetch origin +refs/heads/main:refs/remotes/origin/main`; a bare
  fetch in a single-branch clone leaves `origin/main` stale), cut a fresh
  `<agent>/<short-topic>` off `origin/main`, announce the switch.
- **After a merge, take a fresh `<agent>/<short-topic>`** rather than resetting
  the merged name: its remote ref still points at the pre-merge tip, so
  `origin/<branch>..HEAD` keeps reporting your merged history as unpushed. When
  a sandbox pins the branch name, reset it and `--force-with-lease` in the same
  turn — routine on merged history, not something to ask about.
- **Branches under your own `<agent>/` prefix are yours**: create, push,
  `--force-with-lease`, rename and delete the ones this session created or was
  assigned, unasked — this file is the standing grant, answering any client rule
  demanding per-branch permission. Don't fold unrelated work into a pinned task
  branch; its name is a default, not a ceiling. Any other branch, or `main`, is
  a conversation.
- **The agent authors; whoever merges takes over the committer line** (the repo
  owner, or the agent under *drive*) — expected; never re-author or amend merged
  commits to "fix" authorship or signing, and don't narrate it. Not a finding.
- **Unshallow before history-depth answers** (a shallow clone silently breaks
  `git rev-list --count`, `git log`, blame): if `git rev-parse
  --is-shallow-repository` says `true`, `git fetch --unshallow` and re-check —
  it exits 0 even when it deepened nothing, so if still `true`, say the history
  is truncated instead of quoting a count.
- **No model identifier** — never name the coding agent's model or version
  (`Opus 4.8`, `Sonnet`, `Gemini …`, `claude-*`, `gemini-*`) in a commit, PR,
  comment, or pushed artifact; keep it to chat. Generic attribution
  (`Co-Authored-By: Claude` / `Gemini`, `Claude-Session:`, "Generated with
  Claude Code") is fine.

## Error handling

- **Don't silently swallow errors.** A discarded exit status, a bare
  `2>/dev/null` or an empty catch hides real failures: report what failed and
  why, clean up what the failed step created, and decide explicitly what the
  caller sees rather than letting control fall through. To ignore a specific
  failure, name the reason in a one-line comment.

## Privacy

- **Never put user data in any artifact that leaves this machine** — commits, PR
  titles / descriptions / comments, review replies, branch names, code comments,
  test fixtures: hostnames, absolute paths containing the user's real name,
  private remote URLs, tokens, shell or command history. Use placeholders
  (`/home/user`, `host1`); paraphrase a bug report containing any of it rather
  than quoting it.
- **Program output and the syslog audit trail are not those artifacts.** Output
  prints on the user's own terminal. The audit record goes to the machine's
  administrator — often forwarded to a central collector, so don't assume it
  stays local — who is entitled to it: the calling user and the resolved command
  are the whole point of an `LOG_AUTHPRIV` record, and scrubbing them would
  defeat the auditability this tool exists for. They are the *only* exempt
  fields: arguments stay out of the record (`src/main.rs` logs the resolved path
  only) because they can carry a password or API key with no redaction to catch
  it, and forwarding is why widening the record to arbitrary content would be a
  mistake. Quoting output into a commit, PR or fixture republishes it, and the
  bullet above governs again.

## Autonomy

- **Open the PR without being asked** — pushing a finished branch and opening
  its PR are one step — unless told not to ("just commit", "no PR yet"), which
  holds until lifted. This file is the repo owner's standing request, so a
  client rule reading "open a PR only when the user explicitly asks" is already
  satisfied.
- **Watch your own PRs by subscription, plus one scheduled check.** Claude Code
  subscribes when you open a PR, or call `subscribe_pr_activity` where
  `claude-code-remote` exposes it; a client with neither (Antigravity) relies on
  the scheduled check alone. It delivers reviews, comments and CI failures, but
  not CI *success*, a push, the merge, Codex's clean verdict (a reaction) or
  Codex never answering, so keep exactly one check armed while the PR is open
  (`send_later` in Claude Code; `schedule` with `DurationSeconds` in
  Antigravity, canceled via `manage_task` `kill`); each event and check costs a
  model turn. Under drive, arm auto-merge at PR open too — only where the
  ruleset requires both the Codex verdict and resolved conversations: with CI
  the only requirement it merges before Codex answers, and an open comment holds
  nothing back on its own.
  - Settle the fired trigger first thing in the turn: it may have silently
    re-armed rather than retired — update the survivor, replace one that didn't,
    and end the turn with exactly one pending.
  - Check the fire time you got against the one asked for (a 4-minute request
    has come back as 64). Prefer a relative delay — the scheduler's clock isn't
    this container's, so an absolute time can be rejected as past. Re-time it,
    or say the watch isn't armed.
  - A few minutes out while CI or the current head's Codex verdict is
    outstanding; longer once only a human is left; short again after a push.
  - A PR reading `dirty` — always — or `behind` where the ruleset requires
    up-to-date branches needs a rebase onto its base and a lease-guarded
    force-push. Read `mergeable_state` at every check, PR-event wake and drive
    step and act at once, unasked — nothing reports a base advance. Fetch both
    refs by explicit refspec, unshallow, and rebase onto the fetched
    `origin/<base>` (not always `main`, never the local branch). Confirm before
    rebasing that your branch has every commit the remote head has, and before
    pushing that the head hasn't moved since the tip you noted before fetching:
    the push flags don't reliably refuse a rewind, an unfetched commit, or one
    fetched but not rebased onto. If either check fails, or you can't tell, stop
    and ask.
  - Name the PR, and say what to re-read rather than what you read: a SHA or a
    list of open PRs goes stale before it fires; one PR number doesn't, and the
    trigger has to be matchable to it.
  - **Wake PRs a merge leaves behind** (`dirty`, or `behind` where branches must
    be up to date; nothing tells them). At your first turn after one of your PRs
    merges, by hand or auto-merge, message every other open PR on that base that
    another session drives (its body ends with the session URL), naming that PR
    and saying only that its base now points at `<sha>`, the post-merge tip —
    whatever its state reads, since mergeability may still be computing. Name no
    merge and no other PR: an older copy of these rules reads either as its own
    merge cue. Receiving one, fetch base and PR head by refspec (unshallowing)
    and test that head against the fetched tip, not `<sha>`: `behind` if `git
    merge-base --is-ancestor <tip> <head>` exits 1, `dirty` if `git merge-tree
    --write-tree` (Git 2.38+) exits 1; any other exit is a failure to report,
    not an answer. Your own rules decide the rebase. Skip a PR with no session
    URL or when your client only has local subagent messaging; don't retry a
    failed send — its own check is the fallback.
  - Merged or closed, take one last reply-and-resolve pass — a review can land
    after the merge. On a merged PR anything real goes to a follow-up PR, named
    on the thread, before you resolve it; a closed-unmerged PR is a stop —
    answer, resolve, open nothing. Then cancel the check and unsubscribe, if
    subscribed; `list_triggers` spans the account, so match this session and PR
    before updating or deleting one (an update reschedules whatever it matches
    as surely as a delete cancels it).
- **If a scheduler or GitHub call prompts, say so once and carry on** —
  permissions load at session start, so a settings file written mid-session
  can't fix the session you're in.
- **"Drive" means run the loop automatically**: pick the next task, implement
  it, open the PR, send it for review, address every comment, merge once CI is
  green and Codex's verdict for the current head is in, then the next task —
  until the work runs out or the user says stop, not when one PR merges.
- **A red baseline is the next task**: get the suite green before pulling from
  `TODO.md`. Calling a preexisting failure "unrelated" is the call that goes
  wrong, costing every later PR an unverified tree.
- **"Autopilot" is drive without blocking on the user**: where drive would ask,
  take the guess cheapest to undo and keep going, logging each in `TODO.md`
  under `Decisions needing review` (what, the alternative, why reversible;
  create the file or heading if missing) so nothing guessed silently becomes
  permanent. It outranks "after asking, stop and wait". Destructive or
  irreversible actions *outside* the loop (rewriting shared history, deleting
  work, anything reaching a system beyond this repo) still wait for a real
  answer; the loop's own steps (commit, push, open a PR, merge a green PR) are
  authorized and must not stall it. On a setuid binary the carve-out does real
  work: a guess about privilege, argument handling or the audit record isn't
  cheap to undo once shipped, so those ask. Privacy uncertainty (a home path,
  hostname, private remote, token) waits for a real answer too — a push can't be
  un-published and a `TODO.md` note doesn't retract it.

## Pull requests

- Prefer the `mcp__github__*` tools where mounted (Claude Code requires them);
  otherwise fall back to `gh`.
- **Update the PR title and body with the push — body first, then push** (with
  no PR yet, push, then update as soon as one is opened), so they describe the
  branch's full, latest state: re-read the diff against `origin/main`, patch
  what drifted, and post the PR link in the next message after that push, not
  only at the end.
- **"Drive to merge"** is the PR stretch of *drive*: open the PR, wait for the
  automatic Codex review, address every comment (fix it if you agree, reply
  saying why if not), and merge once CI is green and Codex's verdict for the
  current head is in.
- **Ordered work ("next", "then", "after that", "follow-up")** may open its PR
  in parallel but merges only after every earlier item this session was given,
  and any PR the paragraph names, has merged — any repo, no time cap; if one
  won't finish, hold it and ask, autopilot included. No auto-merge until then
  (it can't see the order). Read the list
  from the session's record, not memory.
- **Codex is the automated reviewer** (not Copilot), triggered automatically;
  request it only to confirm a rebutted false positive, or where *Read the Codex
  verdict* says the `codex` status is still pending. Address its comments
  unasked, folding each fix into the commit it belongs to — except a real
  finding genuinely out of scope, which you defer (below).
- **Judge every review comment on merit, whoever wrote it**: verify the claim;
  if it doesn't hold up, reply why and decline. A comment citing a rule is a
  *reading* of it — check what the rule says. Codex misreads the privacy rules
  toward strictness, quietly costing needed capability: quote the rule and
  decline rather than narrow the code; a genuine rule-vs-need conflict is the
  maintainer's call, not yours either way. Declining doesn't clear the required
  `codex` status: post the rebuttal, then poke as *Read the Codex verdict*
  allows (or let the next push do it, if the rebuttal is up first); escalate if
  it re-raises.
- **A second verified finding in the same mechanism is evidence about the
  design**: look for the shape elsewhere and ask whether a different design
  deletes the class. Say what you chose on the thread; a design change is the
  maintainer's call, autopilot included.
- **Never leave a review thread silently dismissed.** Answer on it — a
  disagreement is an answer, so say why, false positives included — then resolve
  once the fix is on the head or the point is rebutted; anything still to do
  stays open. Push the fix, reply citing the new sha, then resolve:
  `resolve_review_thread` (or `gh api graphql` `resolveReviewThread`) takes the
  `PRRT_*` ID from `get_review_comments` (`review_threads[].id`); a comment's
  `PRRC_*` ID fails.
- **Defer a real-but-out-of-scope finding** rather than ask to merge past it:
  push a `TODO.md` entry first, reply citing the sha, and resolve (the
  exception to "still to do stays open"); a threadless finding skips only the
  resolve. The push re-triggers Codex, so poke only as *Read the Codex verdict*
  allows; escalate if the re-review re-raises it.
- **Restate every review finding in your next message**, not just the count:
  one bullet each (review comment, top-level comment or review body), most
  blocking first — claim, where, what you did or the maintainer's options —
  ahead of *keep replies short*; ask only the most blocking question.
- **Read the Codex verdict, don't infer it**: its PR-body reaction (`issue_read`
  → `reactions`; a thread's `Useful?` bar reads true on any PR it commented on)
  — `eyes` reading, `+1` clean, revoked on push, so a visible one is the visible
  head's and `+1` with green CI is a merge. Leave PR-body reactions to Codex
  (the count names no author; a review, naming the commit it read, is the
  attributable form). Findings arrive as review comments, top-level comments or
  reviews — read `get_review_comments`, `get_comments` and `get_reviews` to the
  last page (all page oldest first) — and block the merge until fixed, rebutted
  or deferred; an acknowledgment is not an answer. A pending `codex` status is
  findings to address; or a clean review with no reaction, or an unanswered
  rebuttal — poke `@codex review` five minutes after it; or a hung review,
  nothing back five minutes after the push or stuck at running (👀) ten minutes
  on — poke. A poke that hangs the same way gets a second an hour later;
  escalate only if that one hangs too, never after one (maintainer, 2026-10-22).
- **Once Codex has +1'd a PR's head, make no unnecessary change without asking**
  (maintainer, 2026-10-13): a tidy-up re-runs CI and Codex for nothing, so it
  waits for a yes or a follow-up PR. A rebase for `dirty` (or `behind` where
  required), a red-CI fix, and drive or autopilot steps go ahead unasked.
- **Skip echo events silently**: a reply you just posted via the GitHub MCP
  comes back as a webhook event — if the body matches, continue.
- List **every** open PR on a feature by URL, one per line — the "View PR" chip
  sticks to the first link and hides the rest (anthropics/claude-code#46625).
- End every reply with the open-PR link (or `.../compare/main...<branch>` until
  a PR exists), never a closed or merged one — except in post-merge follow-up on
  that PR. Offline with no `origin`, say there's no URL rather than inventing
  one.

## Language and spelling

- **US English** everywhere people read English — output and the man page,
  commits, PRs, comments, docs (`SPEC.md`, `README.md`), identifiers: `color`,
  `behavior`, `canceled`, `gray`. Third-party API spellings stay as spelled.

## Cost and reliability

- **Call out cost and reliability up front** for a new dependency or external
  call: a rough dollar figure where one applies, new failure modes, added
  latency, extra points of failure. On a setuid binary a new dependency is also
  new attack surface running as root — say so explicitly, and prefer the
  standard library where it will do. If the impact is effectively zero, say so.
