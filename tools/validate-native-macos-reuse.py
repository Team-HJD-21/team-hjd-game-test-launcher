#!/usr/bin/env python3
"""Authorize same-repository native CI artifacts only when native sources match."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import urllib.error
import urllib.request

BUILD_NAMES = ("Build and test macOS arm64", "Build and test macOS x64")
ARTIFACT_NAMES = ("macOS-native-arm64", "macOS-native-x64")
NATIVE_TREES = ("CMakeLists.txt", "src", "qml", "assets", "vendor")
MAX_RESPONSE = 4 * 1024 * 1024


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def valid_run_id(value: str) -> bool:
    return bool(re.fullmatch(r"[1-9][0-9]{0,19}", value))


def allowed_change(name: str) -> bool:
    path = PurePosixPath(name)
    if not name or name != path.as_posix() or path.is_absolute() or "\\" in name or ".." in path.parts:
        return False
    if name == "CMakeLists.txt" or path.parts[0] in {"src", "qml", "assets", "vendor", "miniz"}:
        return False
    if path.parts[0] == "tools":
        return path.suffix.lower() in {".py", ".ps1", ".sh", ".md", ".txt"}
    if path.parts[0] == "docs":
        return True
    if name.startswith(".github/workflows/"):
        return path.suffix.lower() in {".yml", ".yaml"}
    if path.parts[0] in {"server", "tests"}:
        return path.suffix.lower() == ".py"
    return name in {"README.md", "THIRD_PARTY_NOTICES.md"}


def validate_metadata(run: dict, jobs: list[dict], artifacts: list[dict], repository: str, run_id: str) -> dict:
    require(str(run.get("id")) == run_id, "Upstream run ID differs from the requested run")
    repo = run.get("repository", {})
    head_repo = run.get("head_repository", {})
    require(repo.get("full_name") == repository and head_repo.get("full_name") == repository
            and repo.get("id") == head_repo.get("id") and repo.get("id") is not None,
            "Native outputs must come from this repository, including its head source")
    require(run.get("path", "").split("@", 1)[0] == ".github/workflows/macos-release.yml",
            "Native outputs came from a different workflow")
    sha = run.get("head_sha", "")
    require(bool(re.fullmatch(r"[a-f0-9]{40}", sha)), "Upstream native source SHA is invalid")
    builds = []
    for name in BUILD_NAMES:
        matching = [job for job in jobs if job.get("name") == name]
        require(len(matching) == 1 and matching[0].get("status") == "completed"
                and matching[0].get("conclusion") == "success", "Both native architecture build jobs must have succeeded")
        builds.append({"name": name, "id": matching[0]["id"], "conclusion": "success"})
    selected = []
    for name in ARTIFACT_NAMES:
        matching = [artifact for artifact in artifacts if artifact.get("name") == name and artifact.get("expired") is False]
        require(len(matching) == 1 and matching[0].get("size_in_bytes", 0) > 0,
                "Both nonexpired native architecture artifacts are required")
        artifact = matching[0]
        source = artifact.get("workflow_run", {})
        require(str(source.get("id")) == run_id and source.get("head_sha") == sha
                and source.get("repository_id") == repo["id"] and source.get("head_repository_id") == repo["id"],
                "Native artifact source identity differs from its run")
        selected.append({"name": name, "id": artifact["id"], "headSha": sha})
    # Packaging or audit may have failed after the verified raw binaries were
    # retained. The successful native build jobs, not the overall result, matter.
    return {"sourceHeadSha": sha, "sourceRunConclusion": run.get("conclusion"),
            "buildJobs": builds, "artifacts": selected}


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, new_url):
        raise ValueError("GitHub API redirect refused")


def github_json(repository: str, route: str, token: str):
    request = urllib.request.Request("https://api.github.com/repos/" + repository + route,
        headers={"Authorization": "Bearer " + token, "Accept": "application/vnd.github+json",
                 "X-GitHub-Api-Version": "2022-11-28", "User-Agent": "TeamHJD-Native-Artifact-Validation"})
    with urllib.request.build_opener(NoRedirect()).open(request, timeout=30) as response:
        raw = response.read(MAX_RESPONSE + 1)
    require(len(raw) <= MAX_RESPONSE, "Oversized GitHub source metadata")
    return json.loads(raw)


def github_list(repository: str, route: str, key: str, token: str) -> list[dict]:
    result = []
    for page in range(1, 21):
        separator = "&" if "?" in route else "?"
        response = github_json(repository, route + f"{separator}per_page=100&page={page}", token)
        items = response.get(key)
        require(isinstance(items, list), "GitHub source metadata has the wrong shape")
        result.extend(items)
        if len(items) < 100:
            return result
    raise ValueError("Too many upstream jobs or artifacts")


def git(*arguments: str, required: bool = True) -> bytes:
    result = subprocess.run(["git", *arguments], check=False, capture_output=True, timeout=60)
    if required:
        require(result.returncode == 0, "Git could not verify the exact native source commits")
    return result.stdout if result.returncode == 0 else b""


def validate_source(upstream: str, current: str) -> dict:
    require(git("rev-parse", "HEAD").decode().strip() == current, "Checkout differs from the current workflow source SHA")
    require(not git("diff", "--name-only", "HEAD", "--"), "Tracked files changed after checkout")
    if not git("rev-parse", "--verify", upstream + "^{commit}", required=False):
        # A depth-one checkout may not contain the older artifact source yet.
        git("fetch", "--no-tags", "--depth=1", "origin", upstream)
    changed = [os.fsdecode(name) for name in git("diff", "--no-renames", "--name-only", "-z", upstream, current, "--").split(b"\0") if name]
    require(len(changed) <= 2_000 and all(allowed_change(name) for name in changed),
            "Native outputs cannot be reused after native source or unapproved file changes")
    tree_ids = {}
    for path in NATIVE_TREES:
        original = git("rev-parse", upstream + ":" + path).decode().strip()
        candidate = git("rev-parse", current + ":" + path).decode().strip()
        require(original == candidate, "Native source tree differs from the compiled artifact source")
        tree_ids[path] = original
    return {"changedPaths": changed, "nativeSourceTrees": tree_ids, "currentHeadSha": current}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--current-sha", required=True)
    parser.add_argument("--proof", type=Path, required=True)
    args = parser.parse_args()
    require(valid_run_id(args.run_id), "Native source run ID must contain positive decimal digits only")
    require(bool(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*/[A-Za-z0-9][A-Za-z0-9_.-]*", args.repository)),
            "Invalid GitHub repository")
    require(bool(re.fullmatch(r"[a-f0-9]{40}", args.current_sha)), "Invalid current source SHA")
    require(args.repository == os.environ.get("GITHUB_REPOSITORY"), "Only the current GitHub repository is permitted")
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    require(bool(token), "A read-only GitHub Actions token is required")
    route = "/actions/runs/" + args.run_id
    run = github_json(args.repository, route, token)
    jobs = github_list(args.repository, route + "/jobs?filter=latest", "jobs", token)
    artifacts = github_list(args.repository, route + "/artifacts", "artifacts", token)
    result = validate_metadata(run, jobs, artifacts, args.repository, args.run_id)
    result.update(validate_source(result["sourceHeadSha"], args.current_sha))
    result.update(result="PASS", repository=args.repository, sourceRunId=args.run_id,
                  nativeSourcesIdentical=True, nativeCompilationSkipped=True)
    args.proof.parent.mkdir(parents=True, exist_ok=True)
    with args.proof.open("x", encoding="utf-8") as output:
        json.dump(result, output, indent=2)
        output.write("\n")
    if os.environ.get("GITHUB_OUTPUT"):
        with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
            output.write("run_id=" + args.run_id + "\n")
            output.write("source_sha=" + result["sourceHeadSha"] + "\n")
    print(json.dumps({"result": "PASS", "sourceRunId": args.run_id,
                      "sourceHeadSha": result["sourceHeadSha"], "currentHeadSha": args.current_sha,
                      "changedFiles": len(result["changedPaths"]), "nativeSourcesIdentical": True}), flush=True)


if __name__ == "__main__":
    try:
        main()
    except urllib.error.HTTPError as error:
        print(f"FAIL native source reuse: GitHub metadata HTTP {error.code}", file=sys.stderr)
        raise SystemExit(1)
    except (ValueError, KeyError, OSError, subprocess.TimeoutExpired, urllib.error.URLError) as error:
        print(f"FAIL native source reuse: {error}", file=sys.stderr)
        raise SystemExit(1)
