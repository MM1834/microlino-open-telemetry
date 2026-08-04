#!/usr/bin/env python3
from pathlib import Path
import hashlib
import re
import sys

root = Path.cwd()
docs = root / "docs"
errors = []

required_release_evidence = [
    docs / "project/sprints/V1.0.0-RC.1.md",
    docs / "release-notes/v1.0.0-rc.1.md",
    docs / "testing/REL-001-rc-validation.md",
    docs / "security/cloud-risk-register.md",
]
for required in required_release_evidence:
    if not required.is_file():
        errors.append(f"missing v1.0.0-rc.1 evidence: {required.relative_to(root)}")

retired_directories = [
    docs / "archive",
    docs / "aws",
    docs / "legacy",
    docs / "images",
    docs / "release",
    docs / "releases",
    docs / "developer/lte",
    docs / "developer/mqtt",
    docs / "developer/can",
    docs / "developer/operations",
    docs / "developer/release-notes",
]
for retired in retired_directories:
    if retired.exists():
        errors.append(f"retired documentation path returned: {retired.relative_to(root)}")

allowed_root_docs = {
    "AGENTS.md",
    "CHANGELOG.md",
    "CODE_OF_CONDUCT.md",
    "CONTRIBUTING.md",
    "README.md",
    "SECURITY.md",
}
for candidate in root.iterdir():
    if candidate.is_file() and candidate.suffix.lower() in {".md", ".txt", ".rst", ".adoc", ".docx", ".pdf", ".patch", ".diff"}:
        if candidate.name not in allowed_root_docs:
            errors.append(f"unexpected root document: {candidate.name}")

pattern = re.compile(r'!?\[[^\]]*\]\(([^)#]+)')
for md in docs.rglob("*.md"):
    text = md.read_text(encoding="utf-8")
    for target in pattern.findall(text):
        if "://" in target or target.startswith("mailto:"):
            continue
        target = target.split("?", 1)[0]
        resolved = (md.parent / target).resolve()
        if not resolved.exists():
            errors.append(f"broken link: {md.relative_to(root)} -> {target}")

content_owners = {}
document_files = [
    candidate
    for base in (root, docs)
    for candidate in (base.iterdir() if base == root else base.rglob("*"))
    if candidate.is_file() and candidate.suffix.lower() in {".md", ".txt"}
]
for candidate in document_files:
    digest = hashlib.sha256(candidate.read_bytes()).hexdigest()
    content_owners.setdefault(digest, []).append(candidate.relative_to(root))
for duplicates in content_owners.values():
    if len(duplicates) > 1:
        errors.append("exact duplicate documents: " + ", ".join(map(str, duplicates)))

if errors:
    print("\n".join(errors))
    sys.exit(1)

print("Documentation audit passed.")
