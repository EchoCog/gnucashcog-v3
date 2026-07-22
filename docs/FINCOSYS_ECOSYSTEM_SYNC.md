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

**Not yet attempted**: a full build/link of gnucashcog-v3's cognitive
engine. `COGNITIVE_ACCOUNTING.md` already flags known gcc13/glib2.80 build
breakage in unrelated pre-existing files on modern toolchains, and this
change does not attempt to fix that. The new code reuses the exact same
helpers, includes, and command-implementation pattern already used by the
neighbouring `Gnucash::add_quotes()`/report commands in the same file, but
has not been compiled end-to-end. Build and exercise it in an environment
with a working cognitive-engine toolchain before relying on it.

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
