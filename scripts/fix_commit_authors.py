#!/usr/bin/env python3
"""
This script should be run after cherry-picking or squashing commits from a
source branch (eg: preview) to a target branch (eg: trunk) to preserve proper
authorship attribution.

It analyzes commits in a given range to determine the correct authorship based on
the lines modified or added. It compares changes against the upstream branch using
`git blame` to identify the original authors of the code.

The script can automatically rewrite git history (using `git rebase`) to:
1. Set the primary author of a commit to the person who wrote the majority of the lines.
2. Add `Co-authored-by` trailers to the commit message for other significant contributors.

It handles file renames and supports a dry-run mode for previewing changes without modifying
the repository. Useful for preserving attribution when cherry-picking or squashing commits.
"""
import argparse
import os
import re
import shlex
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from difflib import SequenceMatcher
from typing import Dict, List, Optional, Tuple


@dataclass
class AddedLine:
    file_path: str
    head_line_no: int  # 1-based in HEAD version of the file
    content: str
    old_path: Optional[str] = None  # path before rename (if any)


@dataclass
class CommitDiff:
    commit_sha: str
    parent_sha: Optional[str]
    added_lines: List[AddedLine]
    path_renames: Dict[str, str]  # old_path -> new_path


def eprint(msg: str) -> None:
    sys.stderr.write(msg + "\n")


def run_git(args: List[str], cwd: Optional[str] = None, check: bool = True) -> subprocess.CompletedProcess:
    cmd = ["git"] + args
    # Decode with UTF-8 and replace invalid sequences to be robust against binary outputs
    return subprocess.run(
        cmd,
        cwd=cwd,
        check=check,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def ensure_clean_worktree() -> None:
    out = run_git(["status", "--porcelain", "--untracked-files=no"]).stdout.strip()
    if out:
        eprint("Working tree not clean (ignoring untracked files). Please commit/stash changes before proceeding.")
        sys.exit(1)


def is_merge_commit(commit: str) -> bool:
    out = run_git(["rev-list", "--parents", "-n", "1", commit]).stdout.strip()
    parts = out.split()
    return len(parts) > 2


def get_commit_parent(commit: str) -> Optional[str]:
    out = run_git(["rev-list", "--parents", "-n", "1", commit]).stdout.strip()
    parts = out.split()
    if len(parts) >= 2:
        return parts[1]
    return None


def get_commit_message(commit: str) -> str:
    return run_git(["log", "-1", "--pretty=%B", commit]).stdout


def set_commit_author_and_message(name: str, email: str, new_message: Optional[str]) -> None:
    env = os.environ.copy()
    author_flag = ["--author", f"{name} <{email}>"]
    if new_message is None:
        run_git(["commit", "--amend", "--no-edit", *author_flag])
    else:
        with tempfile.NamedTemporaryFile("w", delete=False) as tf:
            tf.write(new_message)
            tf.flush()
            temp_path = tf.name
        try:
            run_git(["commit", "--amend", *author_flag, "-F", temp_path])
        finally:
            try:
                os.unlink(temp_path)
            except OSError:
                pass


def list_commits_in_range(commit_range: str) -> List[str]:
    # Oldest first
    out = run_git(["rev-list", "--reverse", commit_range]).stdout
    commits = [l.strip() for l in out.splitlines() if l.strip()]
    return commits


def parse_renames_for_commit(parent: str, commit: str) -> Dict[str, str]:
    # Map old_path -> new_path
    out = run_git(["diff", "--name-status", "-M", f"{parent}", f"{commit}"]).stdout
    renames: Dict[str, str] = {}
    for line in out.splitlines():
        # Example: R100\told\tnew
        if not line:
            continue
        status_and_paths = line.split("\t")
        if not status_and_paths:
            continue
        status = status_and_paths[0]
        if status.startswith("R") and len(status_and_paths) == 3:
            old_path = status_and_paths[1]
            new_path = status_and_paths[2]
            renames[old_path] = new_path
    return renames


HUNK_HEADER_RE = re.compile(r"^@@\s+-\d+(?:,\d+)?\s+\+(\d+)(?:,(\d+))?\s+@@")


def collect_added_lines_for_commit(commit: str) -> CommitDiff:
    parent = get_commit_parent(commit)
    added_lines: List[AddedLine] = []
    renames = parse_renames_for_commit(parent, commit) if parent else {}

    if parent is None:
        return CommitDiff(commit, parent, added_lines, renames)

    # Diff for the whole commit with zero context to isolate added lines
    diff_out = run_git(["diff", "--unified=0", "--no-color", "-M", f"{parent}", f"{commit}"]).stdout
    current_file: Optional[str] = None
    old_file: Optional[str] = None
    head_line_cursor = 0
    in_hunk = False

    for raw_line in diff_out.splitlines():
        line = raw_line.rstrip("\n")
        if line.startswith("diff --git "):
            current_file = None
            old_file = None
            in_hunk = False
            continue
        if line.startswith("+++ "):
            # format: +++ b/path or +++ /dev/null
            path = line[4:].strip()
            if path.startswith("b/"):
                current_file = path[2:]
            else:
                current_file = None
            continue
        if line.startswith("--- "):
            path = line[4:].strip()
            if path.startswith("a/"):
                old_file = path[2:]
            else:
                old_file = None
            continue
        if line.startswith("Binary files "):
            # Skip binary diffs
            current_file = None
            old_file = None
            in_hunk = False
            continue
        m = HUNK_HEADER_RE.match(line)
        if m:
            start = int(m.group(1))
            count = int(m.group(2) or "1")
            head_line_cursor = start
            in_hunk = True
            continue
        if in_hunk and current_file:
            if line.startswith("+") and not line.startswith("+++"):
                added_lines.append(AddedLine(file_path=current_file, head_line_no=head_line_cursor, content=line[1:], old_path=old_file))
                head_line_cursor += 1
            elif line.startswith("-") and not line.startswith("---"):
                # Deletions advance only original side, not head_line_cursor
                continue
            else:
                # Context or end of hunk lines advance cursor appropriately
                if not (line.startswith("+") or line.startswith("-")):
                    head_line_cursor += 1

    return CommitDiff(commit, parent, added_lines, renames)


def git_show_blob(rev: str, path: str) -> Optional[str]:
    try:
        return run_git(["show", f"{rev}:{path}"]).stdout
    except subprocess.CalledProcessError:
        return None


def blame_porcelain(upstream: str, path: str) -> Optional[List[Tuple[str, str]]]:
    try:
        out = run_git(["blame", "-e", "-w", "--line-porcelain", upstream, "--", path]).stdout
    except subprocess.CalledProcessError:
        return None
    authors: List[Tuple[str, str]] = []  # (name, email)
    current_author: Optional[str] = None
    current_email: Optional[str] = None
    for line in out.splitlines():
        if not line:
            continue
        if line.startswith("author "):
            current_author = line[len("author "):].strip()
        elif line.startswith("author-mail "):
            mail = line[len("author-mail "):].strip()
            # format: <email>
            if mail.startswith("<") and mail.endswith(">"):
                mail = mail[1:-1]
            current_email = mail
        elif line.startswith("\t"):
            # Source line follows; we should have author and email
            name = current_author or ""
            email = (current_email or "").lower()
            authors.append((name, email))
            current_author = None
            current_email = None
    return authors


def build_line_mapping(upstream_text: str, head_text: str) -> Dict[int, int]:
    """
    Return mapping from head line number (1-based) to upstream line number (1-based)
    for lines that are exactly equal according to SequenceMatcher. Fallback to
    unique exact-content search for unmapped lines.
    """
    upstream_lines = upstream_text.splitlines()
    head_lines = head_text.splitlines()

    sm = SequenceMatcher(None, upstream_lines, head_lines, autojunk=False)
    mapping: Dict[int, int] = {}
    for a0, b0, size in sm.get_matching_blocks():
        for k in range(size):
            upstream_idx = a0 + k  # 0-based
            head_idx = b0 + k
            mapping[head_idx + 1] = upstream_idx + 1

    # Fallback for unmapped lines: single-occurrence exact match search
    content_to_upstream_indices: Dict[str, List[int]] = {}
    for i, txt in enumerate(upstream_lines):
        content_to_upstream_indices.setdefault(txt, []).append(i + 1)

    for head_idx, txt in enumerate(head_lines, start=1):
        if head_idx in mapping:
            continue
        candidates = content_to_upstream_indices.get(txt)
        if candidates and len(candidates) == 1:
            mapping[head_idx] = candidates[0]

    return mapping


def select_authors(author_to_count: Dict[Tuple[str, str], int], threshold: float) -> Tuple[Optional[Tuple[str, str]], List[Tuple[str, str]]]:
    if not author_to_count:
        return None, []
    total = sum(author_to_count.values())
    # Primary: max count; tie-breaker by email lexicographically for determinism
    primary = max(author_to_count.items(), key=lambda kv: (kv[1], kv[0][1]))[0]
    coauthors: List[Tuple[str, str]] = []
    for author, count in author_to_count.items():
        if author == primary:
            continue
        if total > 0 and (count / total) >= threshold:
            coauthors.append(author)
    # Stable order for coauthors: by count desc, then email
    coauthors.sort(key=lambda a: (-author_to_count[a], a[1]))
    return primary, coauthors


def append_coauthors_to_message(message: str, coauthors: List[Tuple[str, str]]) -> str:
    existing = set()
    for line in message.splitlines():
        m = re.match(r"^Co-authored-by:\s*(.+)\s*<([^>]+)>\s*$", line.strip(), re.IGNORECASE)
        if m:
            name = m.group(1).strip()
            email = m.group(2).strip().lower()
            existing.add((name, email))
    to_add = [(n, e) for (n, e) in coauthors if (n, e) not in existing]
    if not to_add:
        return message
    trailer_lines = [f"Co-authored-by: {name} <{email}>" for (name, email) in to_add]
    if not message.endswith("\n"):
        message += "\n"
    if not message.endswith("\n\n"):
        message += "\n"
    message += "\n".join(trailer_lines) + "\n"
    return message


def analyze_head_commit(upstream: str, threshold: float, verbose: bool = False) -> Tuple[Dict[Tuple[str, str], int], int]:
    head = "HEAD"
    if is_merge_commit(head):
        if verbose:
            eprint("Skipping merge commit at HEAD")
        return {}, 0
    commit_diff = collect_added_lines_for_commit(head)
    author_to_count: Dict[Tuple[str, str], int] = {}
    total_counted = 0

    # Group added lines by file path for efficiency
    path_to_lines: Dict[str, List[AddedLine]] = {}
    for al in commit_diff.added_lines:
        path_to_lines.setdefault(al.file_path, []).append(al)

    for path, lines in path_to_lines.items():
        # Obtain HEAD version of file and upstream version
        head_blob = git_show_blob(head, path)
        if head_blob is None:
            continue
        # Prefer new path; if missing upstream, try old path if any
        upstream_blob = git_show_blob(upstream, path)
        if upstream_blob is None and lines and lines[0].old_path:
            upstream_blob = git_show_blob(upstream, lines[0].old_path)  # try old path
            upstream_path_for_blame = lines[0].old_path
        else:
            upstream_path_for_blame = path
        if upstream_blob is None:
            if verbose:
                eprint(f"Upstream file not found for {path}, skipping")
            continue

        # Build mapping head->upstream
        mapping = build_line_mapping(upstream_blob, head_blob)
        blame = blame_porcelain(upstream, upstream_path_for_blame)
        if blame is None:
            if verbose:
                eprint(f"git blame failed for {upstream}:{upstream_path_for_blame}, skipping")
            continue

        for al in lines:
            upstream_line = mapping.get(al.head_line_no)
            if upstream_line is None:
                # Try fallback: if the exact content appears uniquely in upstream
                # Note: build_line_mapping already attempted unique text fallback
                continue
            if 1 <= upstream_line <= len(blame):
                name, email = blame[upstream_line - 1]
                if not name or not email:
                    continue
                key = (name, email.lower())
                author_to_count[key] = author_to_count.get(key, 0) + 1
                total_counted += 1

    return author_to_count, total_counted


def orchestrate_range_or_rebase(args: argparse.Namespace) -> None:
    # If dry-run: analyze each commit and log only
    if args.dry_run:
        commits = list_commits_in_range(args.range)
        if not commits:
            eprint(f"No commits in range {args.range}")
            return
        for c in commits:
            if is_merge_commit(c):
                eprint(f"[skip] merge {c}")
                continue
            # Temporarily checkout the commit to analyze as HEAD? Avoid changing state.
            # Instead, diff and blob reading can be done by specifying <commit> explicitly.
            # For simplicity in dry-run, we cherry-pick the approach used for HEAD by temporarily using worktree-less git show/blame.
            # Reuse analyze_head_commit by checking out is complex; So implement a lightweight per-commit analysis inline:
            parent = get_commit_parent(c)
            if not parent:
                continue
            commit_diff = collect_added_lines_for_commit(c)
            author_to_count: Dict[Tuple[str, str], int] = {}
            path_to_lines: Dict[str, List[AddedLine]] = {}
            for al in commit_diff.added_lines:
                path_to_lines.setdefault(al.file_path, []).append(al)
            total_counted = 0
            for path, lines in path_to_lines.items():
                head_blob = git_show_blob(c, path)
                if head_blob is None:
                    continue
                upstream_blob = git_show_blob(args.upstream, path)
                if upstream_blob is None and lines and lines[0].old_path:
                    upstream_blob = git_show_blob(args.upstream, lines[0].old_path)
                    upstream_path_for_blame = lines[0].old_path
                else:
                    upstream_path_for_blame = path
                if upstream_blob is None:
                    continue
                mapping = build_line_mapping(upstream_blob, head_blob)
                blame = blame_porcelain(args.upstream, upstream_path_for_blame)
                if blame is None:
                    continue
                for al in lines:
                    upstream_line = mapping.get(al.head_line_no)
                    if upstream_line is None:
                        continue
                    if 1 <= upstream_line <= len(blame):
                        name, email = blame[upstream_line - 1]
                        if not name or not email:
                            continue
                        key = (name, email.lower())
                        author_to_count[key] = author_to_count.get(key, 0) + 1
                        total_counted += 1

            primary, coauthors = select_authors(author_to_count, args.threshold)
            if not primary:
                eprint(f"{c[:12]}: no mappable lines")
                continue
            total = sum(author_to_count.values())
            details = ", ".join([f"{n} <{e}>: {author_to_count[(n,e)]}" for (n,e) in sorted(author_to_count.keys(), key=lambda a: (-author_to_count[a], a[1]))])
            eprint(f"{c[:12]}: primary={primary[0]} <{primary[1]}> coauthors={len(coauthors)}/{total} [{details}]")
        return

    # Modify history via rebase with --exec
    ensure_clean_worktree()

    # Determine base for rebase
    base = None
    if ".." in args.range:
        base = args.range.split("..", 1)[0]
    else:
        base = "trunk"

    exec_cmd_parts = [
        sys.executable,
        os.path.relpath(__file__),
        "--amend-current",
        "-u",
        args.upstream,
        "-t",
        str(args.threshold),
    ]
    if args.verbose:
        exec_cmd_parts.append("--verbose")
    exec_cmd = " ".join(shlex.quote(p) for p in exec_cmd_parts)

    rebase_args = ["rebase", "--reapply-cherry-picks", base, "--exec", exec_cmd]
    if args.rebase_merges:
        # Place flag early
        rebase_args = ["rebase", "--rebase-merges", "--reapply-cherry-picks", base, "--exec", exec_cmd]

    eprint("Starting rebase to fix commit authors (this will rewrite history)...")
    try:
        # We do not capture stdout/stderr to allow user to interact if conflicts arise
        subprocess.run(["git"] + rebase_args, check=True)
    except subprocess.CalledProcessError as e:
        eprint("Rebase failed. Resolve conflicts, then run 'git rebase --continue' or abort with 'git rebase --abort'.")
        sys.exit(e.returncode)


def main() -> None:
    parser = argparse.ArgumentParser(description="Fix commit authors by blaming modified lines against an upstream branch.")
    parser.add_argument("--range", "-r", default="trunk..HEAD", help="Commitish or range to operate on (default: trunk..HEAD)")
    parser.add_argument("--upstream", "-u", default="gfx-rdp", help="Upstream branch to blame against (default: gfx-rdp)")
    parser.add_argument("--threshold", "-t", type=float, default=0.10, help="Co-author threshold as fraction of total (default: 0.10)")
    parser.add_argument("--dry-run", action="store_true", help="Analyze and log only; do not modify commits")
    parser.add_argument("--verbose", action="store_true", help="Verbose logging to stderr")
    parser.add_argument("--amend-current", action="store_true", help="Amend only the current commit (HEAD); used under rebase --exec")
    parser.add_argument("--rebase-merges", action="store_true", help="Use git rebase --rebase-merges to preserve merges")

    args = parser.parse_args()

    if args.amend_current:
        # Analyze HEAD and optionally amend
        author_to_count, total_counted = analyze_head_commit(args.upstream, args.threshold, verbose=args.verbose)
        if not author_to_count:
            eprint("HEAD: no mappable lines; leaving author unchanged")
            return
        primary, coauthors = select_authors(author_to_count, args.threshold)
        total = sum(author_to_count.values())
        details = ", ".join([f"{n} <{e}>: {author_to_count[(n,e)]}" for (n,e) in sorted(author_to_count.keys(), key=lambda a: (-author_to_count[a], a[1]))])
        eprint(f"HEAD: primary={primary[0]} <{primary[1]}> coauthors={len(coauthors)}/{total} [{details}]")
        if args.dry_run:
            return
        # Update commit message with co-authors
        msg = get_commit_message("HEAD")
        new_msg = append_coauthors_to_message(msg, coauthors)
        if primary is None:
            return
        set_commit_author_and_message(primary[0], primary[1], new_msg if new_msg != msg else None)
        return

    # Orchestrate over range via rebase or dry-run iteration
    orchestrate_range_or_rebase(args)


if __name__ == "__main__":
    main()


