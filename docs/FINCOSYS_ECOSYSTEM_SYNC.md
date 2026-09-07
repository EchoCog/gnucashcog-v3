# Fincosys Ecosystem Sync — Status

This documents how gnucashcog-v3 connects to the wider fincosys financial
ecosystem: `fincosys/accospace`, `cogpy/fincosys`,
`fincosys/helix`, `cogpy/revstream1`, and `cogpy/ad-res-j7`.


> **Repository move (2026-09-06).** The AtomSpace builder is now
> `fincosys/accospace`. It was `RegimA-Zone/fincosys-atomspace-builder`, and
> sections below still name it that where they describe events from that
> time. The pip package and import name are unchanged --
> `fincosys-atomspace-builder` / `atomspace_builder` -- so only checkout
> paths and repository references move, and
> `scripts/sync_fincosys_ecosystem.py` accepts a sibling checkout under
> either directory name.
>
> This does not on its own unblock the sync workflow. gnucashcog-v3 lives in
> `EchoCog`, so `fincosys/accospace` is still a private cross-org checkout
> that `github.token` cannot read; the workflow still needs an
> `ECOSYSTEM_SYNC_TOKEN` PAT, now scoped to the `fincosys` org rather than
> `RegimA-Zone`. Since `fincosys/helix` was already on that list, one PAT
> scoped to `fincosys` now covers two of the four cross-org checkouts
> instead of one.

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
    --atomspace-builder-dir ../accospace \
    --fincosys-data-dir ../fincosys/data \
    --helix-manifest ../helix/ecosystem/related_artifacts.json \
    --revstream1-data-dir ../revstream1/data_models \
    --write
```

**Update (2026-08-03)**: the `gnucash_ecosystem` preset now also enables
`include_transaction_index`, which loads fincosys's canonical, balance-
hash-verified `data/transaction_index.json` ledger (22K+ reconciled
transactions across all entities) via `TransactionIndexLoader` — additive
alongside the per-statement `include_transactions` extract loader already
in use, with a disjoint node-ID namespace (`TXI_<txid>` vs.
`TX_<account>_<stmt>_<index>`), so both run together. No flag change is
needed on this side to pick it up; it flows through automatically via
`gnucash_ecosystem_config()`, giving `gnc_cognitive_import_fincosys_json()`
access to the reconciled ledger's TRANSACTION_NODE atoms once merged.

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

## CLI entry point

`gnucash-cli --import-fincosys-sync <path> [--export-fincosys-sync <out>]`
(see `Gnucash::import_fincosys_sync()` in `gnucash/gnucash-commands.cpp`,
wired into `gnucash-cli.cpp`'s option parsing) initializes the cognitive
AtomSpace, reads the sync document at `<path>`, and calls
`gnc_cognitive_import_fincosys_json()` against it. If `--export-fincosys-sync`
is also given, the (now-merged) AtomSpace is immediately re-exported via
`gnc_cognitive_export_fincosys_json()` to that path — the cognitive
AtomSpace has no on-disk persistence of its own (see
`gnc-cognitive-accounting.h`), so a CLI invocation's imported atoms would
otherwise vanish at process exit. This closes the previously-missing
wiring the note below used to describe; the file
`scripts/sync_fincosys_ecosystem.py` produces can now be applied directly:

```bash
gnucash-cli --import-fincosys-sync data/fincosys_sync/gnucashcog_ecosystem_sync.json \
    --export-fincosys-sync data/fincosys_sync/gnucashcog_ecosystem_sync.merged.json
```

**Verified end-to-end (2026-07-23)**: a full CMake build of the `gnc-engine`,
`test-fincosys-bridge`, and `gnucash-cli` targets succeeded with zero source
changes needed (`gnc-fincosys-bridge.cpp/.h`, `gnc-cognitive-accounting.cpp/.h`,
`gnucash-commands.cpp`, and `gnucash-cli.cpp` all compiled and linked cleanly
against current headers). `test-fincosys-bridge` passed all 15 cases (export
schema header, ConceptNode/InheritanceLink/HierarchyLink/EvaluationLink
export, truth-value round-trip, invalid/unsupported-atom handling, full
round-trip preservation). The real CLI round-trip was also exercised:

```bash
./bin/gnucash-cli --import-fincosys-sync data/fincosys_sync/gnucashcog_ecosystem_sync.json \
    --export-fincosys-sync /tmp/roundtrip.json
```

imported 99 atoms from this repo's own generated sync snapshot into the
cognitive AtomSpace and re-exported an identical 99 atoms / 0 links with no
data loss. Building required `-DWITH_AQBANKING=OFF` plus the `libofx-dev`/
`libdbi-dev` system packages (unrelated optional GnuCash backends, not part
of this bridge) — with those, the gcc13/glib2.80 breakage
`COGNITIVE_ACCOUNTING.md` flags in *other* subsystems was never triggered by
this build path (`gnc-engine`/`gnucash-cli`); it may still affect subsystems
those targets don't pull in.

## Snapshot freshness (2026-08-17)

The committed `data/fincosys_sync/gnucashcog_ecosystem_sync.json` /
`gnucashcog_ecosystem_atomspace.json` had not been regenerated since
2026-07-23 — three weeks stale, and from *before*
`fincosys-atomspace-builder`'s 2026-08-03 `TransactionIndexLoader` change
landed (see "What this change adds" above). Despite that section's text
already describing the transaction-index merge as automatic, the checked-in
snapshot never actually reflected it: it held 379 nodes / 21 organizations,
none of them transaction-index-derived.

Since the required sibling checkouts (`fincosys-atomspace-builder`,
`fincosys`, `helix`, `revstream1`) are all available locally in this
environment, this refresh ran the documented local-usage command directly
(no `ECOSYSTEM_SYNC_TOKEN` needed — that secret only gates the GitHub
Actions workflow's cross-org checkout step, not a local run against
existing sibling working copies) and re-committed the output. The new
snapshot: **24,035 nodes / 5,348 edges / 5 inferred rules**, 777
organizations (21 fincosys entities + counterparty pseudo-organizations
newly surfaced by the transaction-index merge, prefixed `CP_`), picking up
`cogpy/fincosys`'s 2026-08-12+ `MASTER_ACCOUNTS.json` refinement-tooling
changes and `cogpy/revstream1`'s subsequent case-evidence-model sync. This
is a data refresh only — no loader/exporter/bridge code changed.

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
