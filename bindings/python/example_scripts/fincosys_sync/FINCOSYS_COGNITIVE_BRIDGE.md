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

## Next Steps (tracked here, not silently assumed done)

Consistent with how open items are tracked elsewhere in this ecosystem
(see e.g. `fincosys/CLAUDE.md`'s "Open Next Steps" / "Next Steps"
sections):

- [ ] Write a small C loader (e.g. `gnc_cognitive_load_fincosys_atoms()`)
      that reads `cognitive_atoms.json` and calls
      `gnc_atomspace_create_concept_node()` /
      `gnc_atomspace_create_evaluation_link()` /
      `gnc_atomspace_set_truth_value()` to actually populate the
      in-process (simulated) AtomSpace defined in
      `gnc-cognitive-accounting.cpp`.
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
