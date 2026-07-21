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

## Cross-repo contract verification (this change)

The Python-bindings sync tool at
`bindings/python/example_scripts/fincosys_sync/` (`sync_fincosys.py` +
`cognitive_bridge.py`) has its own, separate `--feed` input path for
`fincosys-atomspace-builder`'s `GnuCashSyncExporter`
(`atomspace_builder/exporters/gnucash_exporter.py`) — distinct from the
`gnc_cognitive_*_fincosys_json()` C++ bridge described above, and already
reachable without building the full engine (`--plan-only` is pure Python;
`--apply`/`cognitive_bridge.py` need only the SWIG Python bindings, not a
full menu/report integration). Until now, that `--feed` path and the
exporter it targets had never actually been run against each other — each
side was built and tested in its own repository against a shared *written*
schema description, with no fixture proving the two agree on the wire
format.

`bindings/python/example_scripts/fincosys_sync/tests/fixtures/atomspace_builder_sync_feed.json`
closes that gap: it's a real document captured from
`generate_feed()`/`GnuCashSyncExporter` run against that repo's own
`tests/test_gnucash_exporter.py` fixture (provenance and regeneration
instructions in `tests/fixtures/README.md` alongside it), and
`tests/test_atomspace_builder_feed.py` feeds it through `load_feed()` +
`build_plan()` + `cognitive_bridge.build_atoms()` and asserts a clean
result. This does not change the C++-bridge status below — it verifies the
independent Python `--feed` path, which is the one with the shortest path
to actually consuming real exporter output today.

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
