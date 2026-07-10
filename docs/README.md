# Documentation contributor guide

The public documentation entry point is [`index.md`](index.md). This file explains how documentation is organized and maintained.

## Canonical structure

```text
docs/
├── index.md
├── getting-started/
├── hardware/
├── webui/
├── dashboard/
├── firmware/
├── api/
├── architecture/
├── troubleshooting/
├── developer/
├── roadmap/
├── releases/
└── assets/images/
```

## Audience separation

- `getting-started`, `hardware`, `webui`, `dashboard` and `firmware` contain current user/integrator documentation.
- `developer` preserves implementation history and experimental details.
- `releases` contains version-specific release notes.
- Do not add sprint/debug notes back into `docs/firmware/`; place them under the appropriate `docs/developer/` section.

## Images

Store images under:

```text
docs/assets/images/branding/
docs/assets/images/diagrams/
docs/assets/images/dashboard/
docs/assets/images/hardware/
docs/assets/images/webui/
```

Use lowercase filenames with hyphens.

Preferred formats:

- technical screenshots: PNG,
- hardware photos: PNG or JPG,
- diagrams and logos: SVG.

Do not commit macOS metadata files such as `.DS_Store`, `._*` or `.-*`.

## Links

Use relative links. From a document one directory below `docs/`, image links normally look like:

```markdown
![ESP32-WROOM](assets/images/hardware/esp32-wroom.png)
```

## Writing style

- Start with the user goal.
- Mark experimental behavior explicitly.
- Avoid presenting historical implementation notes as current behavior.
- Never include real passwords, tokens, IMEIs or private backup JSON.
- Add cross-links to related guides instead of duplicating long explanations.

## Review checklist

Before committing:

```bash
git status
grep -R "docs/images\|../images/" docs --include="*.md"
find docs -name ".DS_Store" -o -name "._*" -o -name ".-*"
```

Review rendered Markdown and verify that referenced images exist.
