# AGENTS.md

## Project operating model

Continue working autonomously from the authoritative governance and
status documents. Prefer completing coherent work packages over asking
for approval after every individual edit.

## Authoritative context

Use the current documents under `docs/governance/` and the repository
root status documents as the authoritative project state.

Treat old sprint manifests, historical release notes, patch files,
migration notes, and version-specific README files as historical
evidence only. Do not treat them as current requirements unless an
authoritative document explicitly references them.

## Scope and efficiency

Ignore generated and local-only content, including:

- `.git/`
- `**/.pio/`
- `build/`
- `.DS_Store`
- binaries, object files, caches, and generated dependencies

Do not inspect large images unless the task specifically concerns
hardware identification, documentation imagery, branding, or layout.

Treat `docs/archive/` as audit-only. Do not search or read it during normal
implementation, review or documentation work unless an authoritative document
references a specific archived record or the task explicitly concerns history.

For documentation work, inspect firmware and dashboard sources only
when needed to verify a technical statement or cross-reference.

## Working method

Before broad repository analysis:

1. Read the authoritative governance and current-status documents.
2. Identify the current work package.
3. Inspect only the project areas relevant to that work package.
4. Preserve consistency across authoritative documents.
5. Run applicable validation and report the resulting repository state.

## Task routing

Use `docs/DOCUMENT_MAP.md` to select the smallest relevant context set. After the
governance documents, read one relevant area index before opening detail pages.
Do not recursively read an entire documentation or source tree when the map and
local references identify narrower evidence.
