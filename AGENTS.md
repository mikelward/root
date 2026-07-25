# Agents

## Working on this project

- Update `SPEC.md` when changing behavior.
- Update `README.md` when changing user-facing features.
- Add tests for new functionality and run tests before committing.

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

## Pull requests

- **"Drive to merge"** is shorthand for the whole loop: open the PR, send it
  for Codex review, address every review comment — fix it if you agree, reply
  on the thread saying why if you don't — and merge once CI is green and Codex
  has left its thumbs up.
