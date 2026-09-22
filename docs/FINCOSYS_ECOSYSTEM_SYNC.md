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


## Commerce records reach this repository through accospace (2026-09-22)

Until now `commerce_import.py` took commerce documents by path only, so the
ecosystem-sync path -- the one this document is about -- carried no sales at
all. accospace's `EcosystemSyncExporter` serialized `organizations` and
`atoms` and simply dropped every commerce record on the floor.

Worse, it did not drop all of them. The commerce loader creates a
counterparty as an `ENTITY` node and `organizations` was defined as "every
`ENTITY` node", so Shopify **retail customers were exported as fincosys group
organizations** and imported here by
`gnc_organizations_from_fincosys_json()` as `GncOrganization` records. One
month of RZL orders produces 78 of them.

accospace now excludes them and carries the records in a `commerce` section
instead (`fincosys/accospace`, `docs/COMMERCE_SYNC_SCHEMA.md`). This side
consumes it:

```bash
# accospace's hypergraph export, commerce section and all
python3 commerce_import.py ../accospace/out/ecosystem_sync.json --out feed.json

# or scan a directory of entity-repository checkouts
python3 commerce_import.py --repos-root ~/fincosys-repos --out feed.json

python3 sync_fincosys.py --feed feed.json --plan-only
```

### What `--repos-root` had to learn from the real corpus

Run against the 43 checked-out repositories it finds 15 record documents and
books 23,135 transactions for DRH, DRHW and RZL into a clean plan. Three
things were needed to get there, each from something the corpus actually
contains:

- **Report captures carry this schema too.** Four QuickBooks documents in
  `entity-regima-dr-h-uk` declare `fincosys-commerce-sync/v1` but hold a
  `report` or `items` block rather than `records` -- a balance sheet, a
  product/service list, two sales summaries. They are not this script's
  input and not errors; they are skipped and counted. A fifth,
  `2026-09-08_ap_aging_detail.json`, *does* carry records but spells its
  entity `entity.entity_code` instead of the schema's `entity.code`, so it
  is reported as unreadable and the run exits non-zero. One malformed file
  no longer stops the other fourteen.

- **The corpus keeps superseded captures on purpose.** RZL holds both the
  109-order Shopify window of 2026-09-06 and the 9,449-order history that
  replaced it, and both the 1,000-row and 13,051-row QuickBooks invoice
  exports. Feeding all of them produced 1,109 duplicate txids and an unclean
  plan. Copies that agree are collapsed -- complete capture over partial,
  then later `generated_at`, with more detail breaking the tie -- and the
  supersession is reported, not silent.

- **Differing detail is not differing figures.** The two QuickBooks exports
  decompose the same invoices differently: the older states total, tax and
  balance, the newer adds subtotal, shipping and discounts. All 1,000
  overlapping invoices agree on every stated figure. So the test for a
  conflict is the amount charged, not the whole decomposition; otherwise a
  thousand real invoices go unbooked over a disagreement that does not
  exist.

Where two captures *do* disagree on the amount charged, neither is booked
and both are named. Across the whole corpus exactly one record does:
`QBO_RZL_INVOICE_52168` (#9592) reads 276.76 with 0.76 outstanding in the
2026-09-06 export and 276.77 fully paid in the 2026-09-07 one. A penny of
rounding and a 76p payment in between -- immaterial, and still not resolved
here, because picking one would answer a question about the evidence
silently.
