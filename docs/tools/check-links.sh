#!/usr/bin/env bash
set -e

echo "Checking markdown links..."
find docs -name "*.md" | while read -r f; do
  grep -oE '\]\(([^)#]+\.md)\)' "$f" | sed -E 's/.*\((.*)\)/\1/' | while read -r l; do
    p="$(dirname "$f")/$l"
    if [ ! -f "$p" ]; then
      echo "Missing: $f -> $l"
    fi
  done
done
echo "Done."
