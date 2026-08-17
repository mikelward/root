#![deny(unsafe_code)]

//! Tests for the codex-review workflows.
//!
//! The sweep's own logic and tests live in mikelward/codex-review; what is
//! pinned here is the part that stays in a consumer repository: which events
//! may run a status-writing job, and what token it holds. Every one of these
//! guards a value whose wrong setting produces no error at all -- just a merge
//! gate that quietly stops working, or one that can never clear.
//!
//! Read as patterns over YAML, which is an approximation of YAML and known to
//! be. It is worth it because the alternative is a parser this crate has no
//! reason to depend on -- and on a setuid binary a dependency is attack
//! surface -- while the risk is bounded by these being pins on exact strings
//! that a human wrote and a human will edit.
//!
//! Every match runs against a COMMENT-STRIPPED copy, and the settings that
//! live in a block are matched inside that block. Those headers explain each
//! setting at length, in the setting's own words -- `statuses: write` appears
//! in six comments of codex-review.yml, `pull_request_review` in the
//! listener's -- so a whole-file match would keep passing on the prose after
//! the setting it describes was deleted.

use std::fs;
use std::path::PathBuf;

fn workflows_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join(".github/workflows")
}

fn read(name: &str) -> String {
    let path = workflows_dir().join(name);
    fs::read_to_string(&path).unwrap_or_else(|e| panic!("reading {}: {e}", path.display()))
}

/// Drops whole-line and trailing comments. A value keeps whatever precedes the
/// `#`, so this can only ever remove prose, never a setting.
fn uncomment(text: &str) -> String {
    text.lines()
        .map(|line| match line.find('#') {
            Some(i) => line[..i].trim_end(),
            None => line,
        })
        .collect::<Vec<_>>()
        .join("\n")
}

/// The raw sweep, comments and all. Only the assertion that pins a warning in
/// the header wants this.
fn sweep_raw() -> String {
    read("codex-review.yml")
}

fn sweep() -> String {
    uncomment(&sweep_raw())
}

fn listener() -> String {
    uncomment(&read("codex-review-listener.yml"))
}

/// The body of a top-level block: everything after `<key>:` at column 0, up to
/// the next line that starts at column 0. Stripped comments leave blank lines,
/// which stay inside the block and match nothing.
fn top_block(text: &str, key: &str) -> String {
    let header = format!("{key}:");
    let mut body = Vec::new();
    let mut inside = false;
    for line in text.lines() {
        if line == header {
            inside = true;
            continue;
        }
        if inside && line.starts_with(|c: char| !c.is_whitespace()) {
            break;
        }
        if inside {
            body.push(line);
        }
    }
    body.join("\n")
}

/// The trigger assertions read the `on:` block alone. The header above it
/// explains at length why `workflow_dispatch` and a bare `pull_request` are
/// absent, so a whole-file scan for those names matches the prose and passes
/// while saying nothing about the triggers.
fn triggers() -> String {
    top_block(&sweep(), "on")
}

fn permissions() -> String {
    top_block(&sweep(), "permissions")
}

/// Both extractions would pass vacuously on an empty string, taking every
/// assertion that reads them with it.
#[test]
fn the_extracted_blocks_are_found_so_the_tests_over_them_mean_something() {
    assert!(triggers().contains("pull_request_target:"));
    assert!(permissions().contains("contents: read"));
}

/// `@main` is deliberate: the action has no build step and no dependencies, so
/// the file that runs is the file you can read. No checkout, also deliberate --
/// it would put a token-bearing .git/config within reach of a job that can
/// write commit statuses.
#[test]
fn the_sweep_runs_the_shared_action_unpinned_and_checks_nothing_out() {
    let sweep = sweep();
    assert!(sweep.contains("uses: mikelward/codex-review@main"));
    assert!(!sweep.contains("actions/checkout"));
}

/// `edited` is load-bearing: retargeting a pull request changes the reviewed
/// diff without moving the head SHA, and GitHub emits `edited` rather than
/// `synchronize` for it, so an existing `codex: success` would stand over a
/// diff nothing had read.
#[test]
fn the_sweep_starts_on_every_event_that_can_change_the_verdict() {
    let on = triggers();
    assert!(on.contains("types: [opened, reopened, ready_for_review, synchronize, edited, closed]"));
    assert!(on.contains("workflows: [codex-review-listener]"));
}

/// `workflow_dispatch` takes a ref and runs the file FROM that ref; a bare
/// `pull_request` has the same hole via the merge ref; and
/// `pull_request_review` is merge-ref too, which is why it lives on the
/// unprivileged listener instead.
#[test]
fn the_sweep_starts_on_no_event_that_lets_a_branch_supply_its_own() {
    let on = triggers();
    assert!(!on.contains("workflow_dispatch"));
    assert!(!on.contains("  pull_request:"));
    assert!(!on.contains("  pull_request_review:"));
}

/// A canceled loop is a gate that stopped sweeping mid-review; 65 minutes caps
/// a hung API call ten past the action's own 55-minute loop.
#[test]
fn the_sweep_keeps_its_loop_envelope_and_hourly_backstop() {
    assert!(triggers().contains("cron: '23 * * * *'"));
    let sweep = sweep();
    assert!(sweep.contains("cancel-in-progress: false"));
    assert!(sweep.contains("timeout-minutes: 65"));
}

/// Pinned so the header's reasoning keeps naming a job that exists -- NOT
/// because a ruleset requires it. Requiring `sweep` is unsafe: a concurrency
/// group holds one pending run, so a head-associated run queued behind a long
/// sweep is canceled and its replacement reports against the default branch,
/// leaving the head with a required check that can never clear.
#[test]
fn the_sweep_job_is_named_and_is_not_a_required_check() {
    assert!(sweep().contains("\n  sweep:\n"));
    // The one assertion that wants the raw file: it pins the warning itself.
    assert!(sweep_raw().contains("DO NOT REQUIRE `sweep`"));
}

/// Renaming one end without the other severs the relay silently, and a verdict
/// submitted as a review with no inline comments then goes unheard.
#[test]
fn the_listener_holds_nothing_and_is_named_at_both_ends() {
    let listener = listener();
    assert!(listener.contains("name: codex-review-listener"));
    assert!(listener.contains("pull_request_review:"));
    assert!(listener.contains("permissions: {}"));
}

/// A commit status belongs to the SHA, so a second writer is an unordered
/// write: one delayed past this run's exit overwrites a fresh verdict with a
/// stale one, and nothing reports that it happened.
///
/// Refusing the literal `statuses: write` in the other files is not enough,
/// and both holes are silent. `permissions: write-all` is valid YAML that
/// grants the scope without ever spelling it; and a workflow with NO
/// permissions block inherits the repository's default GITHUB_TOKEN
/// permission -- a repository SETTING, which no file here can see, and which
/// may be read/write. So each other workflow has to declare its own grant,
/// name no status scope at all (none of them needs even `statuses: read`, and
/// asking for the key rather than one spelling of its value settles the
/// quoting question too), and take no blanket grant.
///
/// The sweep is asked about its permissions BLOCK; the others about their
/// whole (uncommented) file, since a job-level grant is indented and a
/// top-level scan would miss it. Only the top-level `permissions:` counts as
/// declaring one, though: a job-level block leaves every other job on the
/// repository default.
#[test]
fn the_sweep_is_the_only_workflow_here_that_can_write_statuses() {
    let perms = permissions();
    assert!(perms.contains("statuses: write"));
    assert!(!sweep().contains("write-all"), "the sweep takes no blanket grant");
    // ...and the status is the only thing it may write.
    let writes = perms.lines().filter(|l| l.contains(": write")).count();
    assert_eq!(writes, 1, "the sweep must grant exactly one write scope");
    let dir = workflows_dir();
    let mut scanned = 0;
    for entry in fs::read_dir(&dir).expect("reading .github/workflows") {
        let path = entry.expect("reading a workflow entry").path();
        match path.extension().and_then(|e| e.to_str()) {
            Some("yml") | Some("yaml") => {}
            _ => continue,
        }
        scanned += 1;
        if path.file_name().and_then(|n| n.to_str()) == Some("codex-review.yml") {
            continue;
        }
        let text = uncomment(&fs::read_to_string(&path).expect("reading a workflow"));
        assert!(
            text.lines().any(|l| l.starts_with("permissions:")),
            "{} must declare a top-level permissions block",
            path.display()
        );
        assert!(
            !text.contains("statuses:"),
            "{} must ask for no status scope",
            path.display()
        );
        assert!(
            !text.contains("write-all"),
            "{} must take no blanket grant",
            path.display()
        );
    }
    // The scan passes vacuously over an empty directory, so prove it had the
    // sweep and at least one other file to distinguish.
    assert!(scanned >= 2, "the scan must have something to scan (found {scanned})");
}
