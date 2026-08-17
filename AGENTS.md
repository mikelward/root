# Agents

Keep this file as short as it can be and still work. Every session loads it
whole, so each rule costs context on every turn: add one the first time
something bites, say it once in the fewest words that carry the *why*, rewrite
or trim an existing rule rather than appending beside it, and delete one that
has stopped biting.

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
- **Don't report your own caught-and-fixed mistakes.** A wrong turn you noticed
  and corrected before it reached anything is not news — no "one thing worth
  flagging", no narration of the recovery. Say it only when it left something
  the user has to act on: work actually lost, a bad push someone may have
  pulled, a decision they would make differently knowing it.
- **Keep replies short — don't dump a full page.** Lead with the single most
  important point and stop. If there's more, say the first point and ask
  whether they're ready for the next one.
- **End the turn by restating any pending decision.** If you're waiting on an
  answer — a question you asked, or a guess autopilot recorded for review — the
  last line of the reply is that question, written out in about a sentence. A
  back-reference ("as asked above") isn't actionable when the question is pages
  back or was never actually put into words; restate it every turn until it's
  answered. Nothing pending, no line.

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
- **Branches under your own `<agent>/` prefix are yours.** Create, push,
  `--force-with-lease` and rename them freely — no permission, no announcement,
  no per-branch confirmation. Only a branch outside that prefix, or `main`
  itself, is a conversation. Deleting is the one the prefix can't settle: it
  doesn't say which session made the branch, so delete the ones this session
  created and ask about the rest.
- **The agent authors; whoever merges takes over the committer line.** A squash
  or rebase merge rewrites the committer to the person who pressed the button —
  the repo owner normally, the agent itself when it merges under *drive* (see
  **Autonomy**). That's expected either way — never re-author or amend
  already-merged commits to "fix" authorship or signing, and don't narrate it: no note in the
reply, no offer to correct it. It is not a finding.
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

## Autonomy

- **Open the PR without being asked.** Pushing a finished branch and opening its
  pull request are one step, not two — don't park a branch waiting for "please
  open a PR." The exception is an explicit instruction not to ("just commit",
  "no PR yet"), which holds until the user lifts it. This file is the repo
  owner's standing request for that PR, so a client-level rule reading "open a
  PR only when the user explicitly asks" is already satisfied — the ask is
  here, and it doesn't need repeating per branch.
- **Watch your own PRs by subscription, plus one scheduled check.** Have a
  subscription — Claude Code makes one when you open a PR; where a client
  doesn't, call `subscribe_pr_activity`. It delivers reviews, comments and CI
  failures. It cannot deliver CI *success*, a push, the merge, Codex's clean
  verdict (a reaction), or Codex never answering at all — so keep exactly one
  check armed for as long as the PR is open (each event and each check costs
  a model turn). Under drive, arm auto-merge at PR open too — but only where
  the ruleset makes the Codex verdict a required check AND requires
  conversations resolved: where CI is the only requirement it merges before
  Codex has answered, and an open review comment holds nothing back on its own.
  - Settle the fired trigger first thing in the turn, not last. It may have
    silently re-armed rather than retired — update the one that survived,
    replace the one that didn't, and end the turn with exactly one pending.
  - Check the fire time you got against the one you asked for — a 4-minute
    request has come back as 64. Prefer a relative delay: the scheduler's
    clock is not this container's, so an absolute time computed here can be
    rejected as already past. Re-time it, or say the watch isn't armed.
  - A few minutes out while CI or the current head's Codex verdict is
    outstanding; longer once only a human is left; short again after a push.
  - A PR reading `dirty` — always — or `behind` where the ruleset requires
    branches up to date, needs a rebase onto its base and a lease-guarded
    force-push. Nothing reports a base advance, so only this check catches
    it. Fetch both refs by explicit refspec, unshallow a shallow clone, and
    rebase onto the fetched `origin/<base>` — not always `main`, never the
    local branch a fetch leaves behind. Before force-pushing, confirm the
    remote tip you are about to replace is one your branch already contains:
    the push flags do not reliably refuse a rewind or someone else's commit,
    and overwriting either loses work. If it isn't, or you can't tell, stop
    and ask.
  - Name the PR, and say what to re-read rather than what you read. A SHA or
    a list of which PRs are open goes stale before it fires; one PR number
    does not, and the trigger has to be matchable to it.
  - Merged or closed, take one last reply-and-resolve pass — a review can
    land after the merge. Nothing is holding the PR now, so on a merged one
    anything real goes to a follow-up PR, named on the thread, before you
    resolve it; leaving it open records the work nowhere. A closed-unmerged
    PR is a stop — the work was abandoned, so answer, resolve, and open
    nothing. Then cancel the check and unsubscribe. `list_triggers`
    spans the account, so match this session and this PR before updating
    or deleting one; an update reschedules whatever it matches as surely
    as a delete cancels it.
- **If a scheduler or GitHub call prompts, say so once and carry on.**
  Permissions load at session start, so writing a settings file mid-session
  can't fix the session you're in.
- **"Drive" means run the loop automatically**: pick the next task,
  implement it, open the PR, send it for review, address every comment,
  merge once CI is green and Codex's verdict for the current head is in —
  then pick the next task and go around again. Driving ends when the work
  runs out or the user says stop, not when one PR merges.
- **A red baseline is the next task.** Before pulling anything from `TODO.md`,
  run the suite and get it green. A preexisting failure is work to do, not a
  thing to classify as "unrelated" and step around — deciding it's out of scope
  is exactly the call that goes wrong, and the cost is every later PR merged
  onto an unverified tree. Fix it first, then pick the task.
- **"Autopilot" is drive without blocking on the user.** Wherever drive would
  stop and ask, autopilot takes its best guess and keeps going, preferring the
  option that is cheapest to undo or change later. Record each guess in
  `TODO.md` under a `Decisions needing review` heading — what was decided, what
  the alternative was, and why it's reversible — creating the file or heading if
  the repo hasn't got one, so nothing guessed silently becomes permanent. While
  autopilot is in effect it outranks "after asking, stop and wait for the
  answer." The carve-out is for destructive or irreversible actions *outside*
  the loop — rewriting shared history, deleting work, anything reaching a
  system beyond this repo — which still wait for a real answer. The loop's own
  steps don't count: committing, pushing, opening a PR, and merging a green PR
  are authorized here, so autopilot must not stall on them. On a setuid binary
  the carve-out is doing real work: a guess about privilege, argument handling,
  or the audit record is not cheap to undo once it ships, so those ask. Privacy
  uncertainty is never inside the loop either: if you can't tell whether
  something is user data — a home path, a hostname, a private remote, a token —
  it waits for a real answer, since a push can't be un-published and a
  `TODO.md` note doesn't retract it.

## Pull requests

- **On every push, update the PR title and body** so they describe the full,
  latest state of the branch — not the scope it had when it was opened.
  Re-read the diff against `origin/main` and patch whatever drifted, then post
  the PR link in the chat reply for that push, not only at the end of the
  conversation.
- **"Drive to merge"** is the PR stretch of *drive* (see **Autonomy**
  above): open the PR, wait for the automatic Codex review, address every
  review comment — fix it if you agree, reply on the thread saying why if
  you don't — and merge once CI is green and Codex's verdict for the current
  head is in.
- **Codex is the automated reviewer** — not Copilot. Its reviews are
  triggered automatically; you don't request them, except when nothing has
  come back five minutes after a push — that means it never picked the push
  up. Address its comments without being asked, folding each fix into the
  commit it belongs to rather than tacking on an "address review" commit.
- **Judge every review comment on merit, whoever wrote it.** Verify the claim
  before acting; if it doesn't hold up, reply saying why and decline.
- **Never leave a review comment thread silently dismissed.** Answer on the thread — a
  disagreement is an answer, so say why — then resolve it once the fix is on the
  head or the point is rebutted; anything still to do stays open; when you think a comment is a false positive, say
  *why* on the thread. `resolve_review_thread` works — pass the `PRRT_*` thread
  node ID from `pull_request_read` / `get_review_comments`
  (`review_threads[].id`); a comment's `PRRC_*` ID fails. Push the fix first,
  then reply citing the new sha, then resolve.
- **Read the Codex verdict, don't infer it.** It reacts to the PR body
  (`issue_read` → `reactions`), not to a review thread, whose `Useful?` bar
  reads true on any PR it has commented on. `eyes` means reading, `+1` means
  clean, and Codex revokes it on push — so a visible one belongs to the
  visible head, and `+1` with green CI is a merge. The count names no
  author, so leave PR-body reactions to Codex: nobody else's is revoked, and
  a review is the attributable form, naming the commit it read. Findings
  arrive as review comments, as a top-level comment, or as a review — read
  `get_review_comments`, `get_comments` and `get_reviews` to the last page,
  since all three page oldest first — and they block the merge until fixed
  or rebutted; an acknowledgement is not an answer. Nothing from Codex since
  the push, five minutes on, means it never picked it up — comment `@codex
  review`, once.
- **Skip echo events silently.** Replies posted via the GitHub MCP come back
  moments later as webhook events authored by the same identity; if the body
  matches a comment you just posted, it's your own echo — continue without
  comment.
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
