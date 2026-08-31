# Dashboard workspace

The repository dashboard material is consolidated here.

- `current/` is the maintained portal source and the input for validation and
  deployment packaging.
- `legacy/` preserves superseded tracked implementation snapshots for audit.
- `packages/` contains ignored local upload packages, including environment
  configuration, and must not be committed.

The hosted path remains `/dashboard/`; this repository layout does not change the
public URL.

## Languages

German is the project language and the maintained dashboard default/fallback.
`current/js/i18n.js` adds the persisted English and French portal variants. New
user-visible strings must be added to both translation catalogs and covered by
`tools/tests/test_dashboard_i18n_contract.py`.
