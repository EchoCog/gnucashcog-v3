# fincosys Cognitive Bridge

`cognitive_bridge.py` maps a fincosys sync plan (the output of
`sync_fincosys.py --plan-only`, or a plan built inline during `--apply`)
onto the `GncAtomType` vocabulary declared in
[`libgnucash/engine/gnc-cognitive-accounting.h`](../../../../../libgnucash/engine/gnc-cognitive-accounting.h),
this repository's self-contained, in-house C++ simulation of
OpenCog/AtomSpace/PLN concepts.

## What this bridge does today

Given `plan.json`, `cognitive_bridge.py --plan plan.json --out
cognitive_atoms.json` writes a single JSON file:

```json
{"schema_version": "1.0",
 "atoms": [
   {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "entity:<code>", "label": "<entity legal_name>",
    "attributes": {"entity_code": "..."}},
   {"atom_type": "GNC_ATOM_CONCEPT_NODE", "id": "account:<code>", "label": "<account name>",
    "attributes": {"account_type": "...", "currency": "...", "parent_code": "..."}},
   {"atom_type": "GNC_ATOM_INHERITANCE_LINK", "id": "...", "source": "account:<code>", "target": "entity:<code>"}
 ],
 "evaluations": [
   {"atom_type": "GNC_ATOM_EVALUATION_LINK", "id": "tx:<txid>", "predicate": "balanced_transaction",
    "participants": ["account:<code1>", "account:<code2>"],
    "truth_value": {"strength": 1.0, "confidence": 0.9}}
 ]}
```

Concretely, for a given plan it emits:

* One `GNC_ATOM_CONCEPT_NODE` per distinct `entity_code` referenced by any
  account or transaction in the plan (`id: "entity:<code>"`). The label is
  the entity's name if the plan contains an `ENTITY-<code>` root account
  for it (the `--data-dir` fallback loader always creates one, using
  fincosys's `MASTER_ENTITIES.json` `legal_name`); otherwise the label
  falls back to the bare `entity_code`.
* One `GNC_ATOM_CONCEPT_NODE` per account in the plan (`id:
  "account:<code>"`).
* One `GNC_ATOM_INHERITANCE_LINK` per account that has an `entity_code`,
  linking `account:<code>` → `entity:<entity_code>` -- this is exactly the
  triple named in the task's target schema.
* An *additional* `GNC_ATOM_INHERITANCE_LINK` per account with a
  `parent_code` present in the plan, linking `account:<code>` →
  `account:<parent_code>`. This isn't in the minimal schema quoted above,
  but it's a direct, low-risk extension: `gnc-cognitive-accounting.h`
  documents `GNC_ATOM_INHERITANCE_LINK` as representing "account type
  hierarchy" generally, and the raw parent/child account tree is exactly
  that. It's additive -- nothing downstream needs it, and it doesn't
  replace the entity link.
* One `GNC_ATOM_EVALUATION_LINK` per transaction, predicate
  `balanced_transaction`, `participants` = every distinct
  `account:<code>` touched by that transaction's splits.
  * `strength` = `1.0` if the splits sum to (approximately) zero,
    otherwise `1.0 - |imbalance| / gross_split_amount` clamped to `[0,
    1]` -- a continuous "how balanced is this" measure rather than a bare
    pass/fail.
  * `confidence` reflects the *provenance* of the underlying data, not
    the balance computation itself: the `--data-dir` loader carries
    `transaction_index.json`'s own `balance_valid` flag (from fincosys's
    balance-chain verification of the source bank-statement row) through
    into `plan["transactions"][i]["metadata"]["balance_valid"]`.
    `cognitive_bridge.py` reads it back out: `0.9` if `True`, `0.4` if
    `False` (flagged by fincosys itself), `0.6` if absent (e.g. a
    `--feed`-sourced plan with no such provenance field).

Field names are exactly `strength` / `confidence` to match
`gnc_atomspace_set_truth_value(GncAtomHandle atom_handle, gdouble
strength, gdouble confidence)`'s parameter names.

## What this bridge does NOT do

This is a **data/documentation bridge only**. Specifically, as of this
writing:

* **No C++ was written or modified.** `gnc-cognitive-accounting.{h,cpp}`,
  `gnc-cognitive-comms.{h,cpp}`, `gnc-cognitive-scheme.{h,cpp}`, and
  `gnc-neural-symbolic-kernels.{h,cpp}` in `libgnucash/engine/` are
  untouched.
* **No real `GncAtomHandle` values are created.** The `id` fields in
  `cognitive_atoms.json` are plain strings (`"entity:RST"`,
  `"account:BANK-111"`, ...), not the `guint64` handles
  `gnc_atomspace_create_concept_node()` et al. would return from the
  actual (simulated) AtomSpace.
* **Nothing calls into the compiled engine.** There is no code path from
  this Python script to `gnc_atomspace_create_concept_node()`,
  `gnc_atomspace_set_truth_value()`, `gnc_atomspace_create_evaluation_link()`,
  or any other function declared in `gnc-cognitive-accounting.h`.
* **Nothing is wired into the inter-module comms bus.** There is no call
  to `gnc_cognitive_send_message()` / `gnc_cognitive_broadcast_message()`
  (`gnc-cognitive-comms.h`), and no `GncCognitiveMessageHandler` is
  registered anywhere for this data. A module wanting to consume
  `cognitive_atoms.json` today has to read the JSON file itself; there is
  no live/in-process delivery.
* **No PLN reasoning, ECAN attention allocation, or MOSES optimization is
  performed.** `truth_value.strength`/`.confidence` are computed directly
  from fincosys's own balance arithmetic and `balance_valid` flag in
  plain Python -- they are not the output of
  `gnc_pln_validate_double_entry()` or any PLN inference over the
  (simulated) AtomSpace.

In short: **the JSON file is the deliverable.** Nothing reads it back
into the C++ engine yet.

**Update (2026-07-25): the paragraph above is now only true of this
Python script itself.** A real C++ loader exists and is verified working
end-to-end -- see the checked-off item 1 below for exactly what was built,
what was verified, and what limitations remain (in particular: no
`gnc_atomspace_set_atom_attribute()` function actually exists anywhere in
this engine, despite being named in the original item 1 text and in
`docs/FINCOSYS_ECOSYSTEM_SYNC.md`).

## Next Steps (tracked here, not silently assumed done)

Consistent with how open items are tracked elsewhere in this ecosystem
(see e.g. `fincosys/CLAUDE.md`'s "Open Next Steps" / "Next Steps"
sections):

- [x] Write a small C loader (e.g. `gnc_cognitive_load_fincosys_atoms()`)
      that reads `cognitive_atoms.json` and calls
      `gnc_atomspace_create_concept_node()` /
      `gnc_atomspace_create_evaluation_link()` /
      `gnc_atomspace_set_truth_value()` to actually populate the
      in-process (simulated) AtomSpace defined in
      `gnc-cognitive-accounting.cpp`.

  **Done (2026-07-25).** `libgnucash/engine/gnc-cognitive-fincosys-loader.{h,cpp}`
  implements `gnc_cognitive_load_fincosys_atoms()`: it parses
  `cognitive_atoms.json` with a small purpose-built JSON parser (a second,
  independent copy of the same minimal-parser pattern
  `gnc-fincosys-bridge.cpp` already uses for the *other*
  `fincosys-ecosystem-sync/v1` schema -- not shared, since that one is
  private to its own translation unit and the two schemas don't overlap),
  creates one real `GncAtomHandle` per `GNC_ATOM_CONCEPT_NODE` entry via
  `gnc_atomspace_create_concept_node()`, one real `GNC_ATOM_INHERITANCE_LINK`
  atom per inheritance entry via `gnc_atomspace_create_inheritance_link()`
  (resolving `source`/`target` string ids against previously-created
  handles, skipping with a `g_warning()` -- not crashing -- if an id is
  unresolved), and one real `GNC_ATOM_EVALUATION_LINK` atom per resolved
  `participants` entry via `gnc_atomspace_create_evaluation_link()` +
  `gnc_atomspace_set_truth_value()` (one shared `PredicateNode` per distinct
  predicate name, and -- since that creation function only connects a
  predicate to a *single* account atom -- one `EvaluationLink` per
  participant, all carrying the JSON entry's own strength/confidence rather
  than the function's hard-coded 0.9).

  One correction against this checklist's own wording: **no
  `gnc_atomspace_set_atom_attribute()` function exists anywhere in this
  engine** (checked via `grep -r` across the whole repo before writing the
  loader) -- it's named here and in `docs/FINCOSYS_ECOSYSTEM_SYNC.md`, but
  neither `gnc-cognitive-accounting.h` nor any other header declares it.
  `gnc-fincosys-bridge.cpp` independently hit the same gap for the *other*
  schema and solved it with a private, file-local `GncAtomHandle`-keyed
  attribute side table (the cognitive AtomSpace itself has no generic
  attribute storage). This loader does the same, in its own separate table
  (exposed read-only via `gnc_cognitive_fincosys_loader_get_atom_attribute()`
  for tests/callers), rather than claiming to call a function that isn't
  real.

  **Verified, not just built**: `libgnucash/engine/test/test-cognitive-fincosys-loader.cpp`
  (15 cases -- null/invalid JSON, uninitialized AtomSpace, missing ids,
  unsupported atom types, unresolved source/target/participants, shared
  predicate dedup, truth-value override, and a full end-to-end case using
  the *actual* output of `cognitive_bridge.build_cognitive_atoms_doc()` run
  against `test_cognitive_bridge.py`'s own fixture data) passes 15/15
  against a from-source CMake build of `gnc-engine` on this environment
  (Ubuntu 24.04, gcc 13.3, glib 2.80; configured with `-DWITH_AQBANKING=OFF`
  after installing `libofx-dev`/`libdbi-dev`/`libdbd-sqlite3`/`libxml2-dev`/
  `libxslt1-dev`/`xsltproc`/`gettext`/`swig`/`guile-3.0-dev`/
  `libsecret-1-dev`/`libboost-{date-time,filesystem,locale,program-options,
  regex,system}-dev`/`libgtk-3-dev`/`libwebkit2gtk-4.1-dev` -- none of
  these are new dependencies of this loader specifically, they're what the
  top-level `CMakeLists.txt` already requires for a from-scratch build in
  this environment). `test-fincosys-bridge` (the other, pre-existing
  bridge's test suite) still passes 15/15 unchanged -- no regression. One
  unrelated, pre-existing test (`test-cognitive-accounting`'s
  `PLNDoubleEntryValidation` case) crashes on this same build; it was not
  touched by this change (`gnc-cognitive-accounting.{h,cpp}` are unmodified)
  and is out of scope here.

  **Stretch CLI wiring also landed and was verified end-to-end**:
  `gnucash-cli --import-fincosys-sync <path> --export-fincosys-sync <out>`
  (`gnucash/gnucash-commands.cpp`) now auto-detects `cognitive_atoms.json`
  by sniffing for the top-level `"schema_version"` key (unambiguous against
  the other schema's top-level `"schema"` key -- both schemas have an
  `"atoms"` key, but with incompatible shapes, so that key alone can't be
  used to tell them apart) and routes to
  `gnc_cognitive_load_fincosys_atoms()` instead of
  `gnc_cognitive_import_fincosys_json()`. Run against a real
  `cognitive_atoms.json` generated by this Python script's own
  `build_cognitive_atoms_doc()`:
  ```
  Imported 4 concept node(s), 4 inheritance link(s), and 4 evaluation
  link(s) from cognitive_atoms.json (0 atom(s), 0 link(s), and 0
  evaluation entry/entries skipped).
  Wrote merged cognitive AtomSpace to roundtrip.json.
  ```
  The re-exported `roundtrip.json` (via the *existing*
  `gnc_cognitive_export_fincosys_json()`, in the other schema) was inspected
  directly and confirmed to carry the loaded ConceptNodes, InheritanceLinks,
  a single shared PredicateNode, and EvaluationLinks with the correct
  per-transaction truth values (`{"strength": 1, "confidence": 0.9}` and
  `{"strength": 0.571429, "confidence": 0.4}`) rather than the creation
  function's hard-coded default. Note: this export does *not* include the
  `attributes` this loader captured (`entity_code`, `account_type`, ...) --
  those live in this loader's own private side table, separate from
  `gnc-fincosys-bridge.cpp`'s side table, which is the only one
  `gnc_cognitive_export_fincosys_json()` reads from. Unifying the two was
  judged out of scope for this change (see "don't touch unrelated code" in
  the task that produced this loader) and is a natural candidate for a
  future incremental change.

- [ ] Route that loader's output through `gnc_cognitive_send_message()` /
      `gnc_cognitive_broadcast_message()` so other cognitive modules
      (`GNC_MODULE_PLN`, `GNC_MODULE_ECAN`, ...) can react to newly synced
      fincosys atoms, rather than requiring a manual file read.
- [ ] Decide whether `balanced_transaction` evaluations should also drive
      `gnc_pln_validate_double_entry()` / `gnc_pln_validate_n_entry()`
      (real PLN-style validation) instead of (or in addition to) the
      plain-Python balance arithmetic this bridge currently uses.
- [ ] Extend the atom set once `fincosys-atomspace-builder`'s hypergraph
      (`hypergraph_data.json`, `entity_relation`/`event_timeline`/
      `stock_flow`/`hypergraph` schemas per fincosys's own four modeling
      paradigms) is available via `--feed`, to map e.g. fraud-indicator
      hyperedges onto `GNC_ATOM_IMPLICATION_LINK` / `GNC_ATOM_AND_LINK`.
- [ ] Revisit the `Num`-field idempotency workaround in `sync_fincosys.py`
      (see `README.md` "Idempotency approach") if/when a proper
      KVP-backed `online_id`-style slot is exposed through the Python
      bindings -- that would also be the natural place to store a
      back-reference from a `Transaction` to its `cognitive_atoms.json`
      evaluation id.
