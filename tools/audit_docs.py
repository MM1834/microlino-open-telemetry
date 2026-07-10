#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path.cwd()
docs = root / "docs"
errors = []

for p in docs.rglob("*"):
    if p.name == ".DS_Store" or p.name.startswith("._") or p.name.startswith(".-"):
        errors.append(f"macOS metadata: {p}")

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

if errors:
    print("\n".join(errors))
    sys.exit(1)

print("Documentation audit passed.")
