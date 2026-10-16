#!/usr/bin/env python3
"""Regenerate platform increments after the recorded prerequisite baseline commits."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

PATCH_ROOT = Path(__file__).resolve().parent


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--top", type=Path, default=next((p for p in PATCH_ROOT.parents if (p / "build/envsetup.sh").is_file()), Path.cwd()))
    args = parser.parse_args()
    series = json.loads((PATCH_ROOT / "series.json").read_text())
    exports = []
    for project in series["projects"]:
        repo = args.top.resolve() / project["path"]
        base = project["base_commit"]
        if git(repo, "status", "--porcelain", "--untracked-files=no").strip():
            raise RuntimeError("Tracked changes present: " + project["path"])
        candidates = (base, project.get("equivalent_source_base"))
        base = next((candidate for candidate in candidates if candidate and
                     subprocess.run(["git", "-C", str(repo), "merge-base", "--is-ancestor",
                                     candidate, "HEAD"], stdout=subprocess.DEVNULL,
                                    stderr=subprocess.DEVNULL).returncode == 0), None)
        if base is None:
            raise RuntimeError("Recorded prerequisite missing: " + project["path"])
        project["base_commit"] = base
        baseline_tree = git(repo, "rev-parse", base + "^{tree}").decode().strip()
        if project.get("baseline_tree", baseline_tree) != baseline_tree:
            raise RuntimeError("Recorded prerequisite tree changed: " + project["path"])
        project["baseline_tree"] = baseline_tree
        if git(repo, "rev-list", "--merges", base + "..HEAD").strip():
            raise RuntimeError("Nonlinear patch history: " + project["path"])
        previous = {p["source_commit"]: p["file"] for p in project["patches"]}
        old_files = set(previous.values())
        patches = []
        for number, commit in enumerate(git(repo, "rev-list", "--reverse", base + "..HEAD").decode().splitlines(), 1):
            data = git(repo, "format-patch", "-1", "--stdout", "--no-signature", "--no-numbered", commit)
            subject = git(repo, "show", "-s", "--format=%s", commit).decode().strip()
            filename = previous.get(commit)
            if filename is None:
                name = re.sub(r"[^A-Za-z0-9._-]+", "-", subject).strip("-")[:64]
                filename = project["path"].replace("/", "-") + "/%04d-%s.patch" % (number, name)
            patch_id = subprocess.check_output(["git", "patch-id", "--stable"], input=data).split()[0].decode()
            patches.append({"file": filename, "source_commit": commit,
                            "source_author": git(repo, "show", "-s", "--format=%an <%ae>", commit).decode().strip(),
                            "patch_id": patch_id, "sha256": hashlib.sha256(data).hexdigest()})
            exports.append((PATCH_ROOT / filename, data))
        project["source_head"] = git(repo, "rev-parse", "HEAD").decode().strip()
        project["patches"] = patches
        exports.extend((PATCH_ROOT / filename, None) for filename in old_files - {p["file"] for p in patches})
    # Prepare every project before replacing any recorded files.
    for path, data in exports:
        if data is None:
            path.unlink()
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    series["mione_platform_increment_count"] = sum(len(p["patches"]) for p in series["projects"])
    (PATCH_ROOT / "series.json").write_text(json.dumps(series, indent=2) + "\n")
    print("Exported %d platform patches" % sum(len(p["patches"]) for p in series["projects"]))


if __name__ == "__main__":
    main()
