# TODO

## Decisions needing review

Guesses made under autopilot, recorded so nothing decided without the
repository owner silently becomes permanent. Each says what was decided, what
the alternative was, and why it is reversible.

- [ ] **The fork-pull-request gap is documented upstream, not fixed here.**
      The shared codex-review setup is taken as-is, with its fork limitation
      recorded in `mikelward/codex-review`'s `docs/CONSUMER.md` rather than
      fixed. The alternative was holding this conversion until the shared
      action publishes its check result against `pull_request.head.sha`, so a
      fork pull request could satisfy a required `codex-review-check`.
      External fork pull requests are not a case these repositories take
      today, and the head-associated check comes from the `push` trigger,
      which same-repo pull requests always get. The three workflow files here
      are byte-identical template copies, so a local edit would fail the pin;
      the fix belongs upstream once.
      *Reversible:* entirely. When the remedy lands upstream this repository
      re-copies `templates/` and gets it for free, and the remedy is written
      out there in full — including that it is only half the gate, since a
      fork head also fails the `codex` status for a separate, deliberate
      reason.

## Add the ruleset settings the Codex gate expects

Three settings this repository's ruleset does not have yet, all explained in
the shared `docs/CONSUMER.md`: require `codex` (not `sweep`), require
`codex-review-check / codex-review-check`, and require branches to be up to
date before merging. Deliberately a follow-up — requiring a check in the same
change that installs it would block the change that installs it.

A fourth, raised by Codex on the pull request that installed these files and
written up upstream since: **require the workflow**
`mikelward/codex-review/.github/workflows/check-consumer.yml@main` under
*Require workflows to pass before merging*, not only the status context. The
check run that lands on a pull request's head comes from `codex-review-check`'s
`push` trigger, and a `push` workflow's definition is the pushed branch's own —
confirmed on run 32059210400 here, `event: push`. So a same-repository pull
request could declare a job of that name calling something else and the
required context would report green having checked nothing. Requiring the
workflow evaluates code the pull request cannot supply. The `codex` status is
not exposed this way: the sweep runs from `pull_request_target` and `schedule`,
both of which take their definition from the default branch.

Worth knowing when the next repository is converted, since it looks like a
broken gate and is not: until `codex-review.yml` is on the default branch, the
two triggers that sweep unprompted — `schedule` and `pull_request_target` —
resolve their definition *there*, so neither fires for the pull request
installing it. What does fire is `pull_request_review_comment`, which resolves
against the merge ref, so a reply on a review thread runs the sweep and
publishes the verdict for the current head.

## Review and merge gates

- [ ] **Add `zizmor` to the ruleset's required set** once it has reported
      on a pull request: the new zizmor workflow runs unfiltered on every
      PR precisely so it can be required (a paths-filtered workflow
      creates no check run at all on a non-matching PR, which a ruleset
      waits on forever) — the posture piloted in mikelward/lanes and
      mikelward/ci-commit-artifact. `repo-rules mikelward/root` with no
      arguments applies the standard `lanes codex zizmor` set.

- [ ] Verify the settings half of the fleet's bar — every repository
      works the same: comprehensive automated review, required merge
      gates, and auto-merge. A ruleset on the default branch requiring
      the gates, the `codex` status, conversation resolution and
      up-to-date branches, with the auto-merge setting enabled — together
      with the required-workflow gate for the trusted `check-consumer.yml`
      documented earlier in this file, which completes the list.
