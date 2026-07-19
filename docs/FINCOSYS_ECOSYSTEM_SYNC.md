# Fincosys Ecosystem Sync — Status

This documents how gnucashcog-v3 connects to the wider fincosys financial
ecosystem: `RegimA-Zone/fincosys-atomspace-builder`, `cogpy/fincosys`,
`fincosys/helix`, `cogpy/revstream1`, and `cogpy/ad-res-j7`.

## What already exists

`libgnucash/engine/gnc-fincosys-bridge.h/.cpp` (see
[COGNITIVE_ACCOUNTING.md](../COGNITIVE_ACCOUNTING.md)) implements the C++
side of the shared **Fincosys Ecosystem Sync Schema v1**
(`"schema": "fincosys-ecosystem-sync/v1"`):

- `gnc_cognitive_export_fincosys_json()` — export every ConceptNode /
  PredicateNode / InheritanceLink / EvaluationLink atom in the cognitive
  AtomSpace, including generic attributes recorded via
  `gnc_atomspace_set_atom_attribute()`.
- `gnc_cognitive_import_fincosys_json()` — materialize ConceptNode /
  PredicateNode / InheritanceLink / EvaluationLink / HierarchyLink atoms
  from a document in this schema into the cognitive AtomSpace.

These are exercised by `libgnucash/engine/test/test-fincosys-bridge.cpp`,
but until now nothing outside that test suite ever produced or consumed a
real sync document. (`gnc-ontogenesis-bridge.h/.cpp` is a second, similarly
structured external-sync bridge in this repo, unrelated to the fincosys
schema.)

## What this change adds

`scripts/sync_fincosys_ecosystem.py` drives
`fincosys-atomspace-builder`'s `gnucash_ecosystem` preset (fincosys master
data + this repo's own prior export, if any + gnucashm's organizations +
helix's manifest + revstream1's case-evidence records) and writes the
result to `data/fincosys_sync/gnucashcog_ecosystem_sync.json` — a real,
schema-conformant document ready for `gnc_cognitive_import_fincosys_json()`
to import into the cognitive AtomSpace.

`.github/workflows/sync-fincosys-ecosystem.yml` runs that script on manual
dispatch: dry-run by default (build + validate + print counts only), or
`write: true` to persist a snapshot and open a draft PR for review. It
never runs on a schedule and never commits without an explicit run.

```bash
# Local usage, with sibling checkouts of the repos above:
python scripts/sync_fincosys_ecosystem.py \
    --atomspace-builder-dir ../fincosys-atomspace-builder \
    --fincosys-data-dir ../fincosys/data \
    --helix-manifest ../helix/ecosystem/related_artifacts.json \
    --revstream1-data-dir ../revstream1/data_models \
    --write
```

## What is still missing (follow-up)

`gnc_cognitive_import_fincosys_json()` itself is not yet reachable from any
CLI, Scheme report, or menu action in a built gnucashcog-v3 — it is only
called from the test suite. Wiring it up requires building and testing the
full cognitive-accounting engine (already flagged in
`COGNITIVE_ACCOUNTING.md` as having known gcc13/glib2.80 build breakage in
unrelated pre-existing files), which this change does not attempt. Until
that lands, the file this script produces is a staged artifact for
manual/future import, not an automatically-applied one — no AtomSpace is
modified by running it.

## Why helix's contribution is not treated as financial data

`fincosys/helix` is a fork of an unrelated generic AI-agent research-loop
tool. It holds no ledger data; the `ecosystem/related_artifacts.json`
manifest it publishes is hand-authored/self-reported, with no code in that
repo that computes or verifies the row/object counts it claims.
`fincosys-atomspace-builder`'s `loaders/helix.py` already accounts for this:
every node it produces from that manifest is tagged
`verification_status: "unverified_self_reported"` with a low-confidence
truth value, and the manifest's raw numbers are namespaced under
`helix_self_reported` rather than merged into verified attributes. This
script inherits that behavior unchanged — do not strip the tagging when
consuming its output, and do not cite `helix_self_reported` figures as
verified case evidence.
