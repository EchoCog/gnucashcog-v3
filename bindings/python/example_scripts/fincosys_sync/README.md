# fincosys_sync

Sync tool that brings canonical financial-forensics data from the
[`fincosys`](../../../../../fincosys) repository (case 2025-137857,
Peter Faucitt v. Jacqueline Faucitt et al.) into a GnuCash book, plus a
data bridge that maps the synced data onto this repo's Cognitive Engine
atom vocabulary (`libgnucash/engine/gnc-cognitive-accounting.h`).

This directory contains two independent scripts:

| Script | Requires `gnucash` bindings? | What it does |
|---|---|---|
| `sync_fincosys.py` | Only in `--apply` mode | fincosys data → validated plan → (optionally) a real GnuCash book |
| `cognitive_bridge.py` | No | plan → `cognitive_atoms.json` (GncAtomType vocabulary mapping) |

## Data flow

```
                     ┌──────────────────────────────┐
                     │  accospace (atomspace_builder)│
                     │  GnuCashSyncExporter (feed)   │   (optional; format
                     │        sync_feed.json         │    documented below)
                     └───────────────┬───────────────┘
                                     │ --feed
                                     ▼
  fincosys/data/*.json  ──--data-dir──►  sync_fincosys.py --plan-only  ──►  plan.json
  (MASTER_ENTITIES,                            │  (pure Python,                │
   MASTER_ACCOUNTS,                             │   no gnucash import)          │
   transaction_index)                           │                               │
                                                 ▼                               ▼
                                     sync_fincosys.py --apply           cognitive_bridge.py
                                     --book <path>                              │
                                     (requires `gnucash`                       ▼
                                      Python bindings)              cognitive_atoms.json
                                                 │                  (GncAtomType-vocabulary
                                                 ▼                   mapping; NOT wired into
                                        GnuCash book (Accounts        the compiled engine --
                                        + Transactions + Splits)      see FINCOSYS_COGNITIVE_
                                                                       BRIDGE.md)
```

`sync_fincosys.py` has two independent input paths and two independent
output modes, which can be mixed:

* **Input**: either `--feed <path>` (a normalized sync-feed JSON -- the
  format `fincosys-atomspace-builder`'s `GnuCashSyncExporter`
  (`atomspace_builder/exporters/gnucash_exporter.py`) produces; a real
  document captured from that exporter's own test fixture is checked in at
  `tests/fixtures/atomspace_builder_sync_feed.json`, and
  `tests/test_atomspace_builder_feed.py` verifies `load_feed()` +
  `build_plan()` + `cognitive_bridge.build_atoms()` all accept it cleanly
  -- see schema below) or `--data-dir <path>` (reads `fincosys/data/*.json`
  directly; this is the fallback loader and the one exercised by the smoke
  test in this README, for when no `fincosys-atomspace-builder` checkout is
  at hand).
* **Output**: `--plan-only` (validate + write `plan.json`, pure Python) or
  `--apply --book <path>` (drive the real `gnucash` Python bindings).

## Prerequisites

* **`--plan-only`**: Python 3.9+, standard library only. No GnuCash build
  required.
* **`--apply`**: GnuCash built with `-DWITH_PYTHON=ON` and the resulting
  `bindings/python` package importable as `gnucash` (`import gnucash`
  must succeed). See this repository's top-level build docs. If the
  module isn't importable, `--apply` exits with a clear error (exit code
  3) rather than a raw traceback.
* **`cognitive_bridge.py`**: Python 3.9+, standard library only, always.

## Usage

### 1. Validate + plan (no GnuCash build needed)

```bash
# From a fincosys checkout's data/ directory:
python3 sync_fincosys.py --plan-only \
    --data-dir /path/to/fincosys/data \
    --out plan.json

# ...or from a normalized sync-feed JSON:
python3 sync_fincosys.py --plan-only \
    --feed /path/to/sync_feed.json \
    --out plan.json
```

Exit code is `0` if the plan is clean (no issues), `1` if issues were
found (still writes the plan; inspect `plan["issues"]`), `2` on a
structural load error (bad JSON / missing required fields).

### 2. Apply to a GnuCash book

```bash
python3 sync_fincosys.py --apply --book /path/to/book.gnucash --plan plan.json

# or build the plan inline:
python3 sync_fincosys.py --apply --book /path/to/book.gnucash \
    --data-dir /path/to/fincosys/data
```

`--apply` refuses to run (exit code 4) if the plan has duplicate txids,
unbalanced transactions, or unknown account references -- fix those with
`--plan-only` first. It is **idempotent**: re-running against the same
book only creates transactions whose txid hasn't been synced yet (see
"Idempotency approach" below).

### 3. Cognitive atom bridge

```bash
python3 cognitive_bridge.py --plan plan.json --out cognitive_atoms.json
```

Pure Python; works on any plan produced by `sync_fincosys.py
--plan-only`, whether or not that plan was ever `--apply`'d.

## Sync-feed JSON schema

```json
{"schema_version": "1.0",
 "accounts": [{"code": "...", "name": "...", "parent_code": "... or null",
   "account_type": "ASSET|LIABILITY|BANK|EXPENSE|INCOME|EQUITY",
   "entity_code": "...", "currency": "ZAR", "description": "..."}],
 "transactions": [{"txid": "...", "date": "YYYY-MM-DD", "description": "...",
   "currency": "ZAR", "entity_code": "...",
   "splits": [{"account_code": "...", "amount": 0.0, "memo": "..."}, ...],
   "metadata": {"is_intercompany": true, "category": "...", "xero_account_code": "..."}}]}
```

This is the format `fincosys-atomspace-builder`'s `GnuCashSyncExporter`
(`atomspace_builder/exporters/gnucash_exporter.py`, alongside
`json_exporter.py`, `viz_exporter.py`, `neon_exporter.py` and
`ecosystem_sync_exporter.py` under `atomspace_builder/exporters/`)
produces. `tests/fixtures/atomspace_builder_sync_feed.json` is a real
document generated by that exporter (see `tests/fixtures/README.md` for
provenance), and `tests/test_atomspace_builder_feed.py` is a cross-repo
contract test asserting `sync_fincosys.py` and `cognitive_bridge.py` both
consume it cleanly -- so schema drift between the two repos surfaces here
as a test failure rather than at apply time.

## Imbalance-account rationale

`fincosys/data/transaction_index.json` rows are **single-sided
bank-statement lines**: one amount, one bank account, no offsetting side
(they were extracted from bank statement PDFs, not from a double-entry
ledger). GnuCash requires every `Transaction`'s `Split`s to sum to zero.

Rather than inventing a fictitious counterparty account, the `--data-dir`
loader (`load_fallback()` in `sync_fincosys.py`) synthesizes the
offsetting split into a placeholder account named `Imbalance-<category>`,
scoped per entity (account code `IMBALANCE-<entity_code>-<category>`,
e.g. `IMBALANCE-SLG-PAYMENT`). This mirrors the convention GnuCash's own
OFX/QIF importers use internally: `libgnucash/engine/Scrub.cpp`
(`get_balance_split()`) creates an account literally named
`Imbalance-<commodity-mnemonic>` of type `ACCT_TYPE_BANK` to hold the
unmatched side of a transaction. This tool mirrors that `ACCT_TYPE_BANK`
choice, but keys the account on `(entity_code, category)` instead of just
currency -- fincosys already classifies every row into a `category`
(`INCOME`, `PAYMENT`, `FEE`, `CARD_PURCHASE`, ...), and the case is
entity-scoped forensic analysis rather than personal bookkeeping, so a
single per-currency suspense account would bury the very distinctions a
forensic accountant needs (e.g. keeping `IMBALANCE-SLG-PAYMENT` distinct
from `IMBALANCE-RST-FEE`).

These accounts are placeholders for further work, not a claim that the
offsetting side is unknowable -- reconciling them against the actual
counterparty ledger entry (where one exists in fincosys's own
`intercompany_transfers.json` / `entity_relationships.json` or in Xero)
is exactly the kind of gap this sync tool is meant to make visible, not
paper over.

## The `helix` non-finding

`CLAUDE.md` for this task named `fincosys/helix` as a repository to check
for integration. `/home/user/helix` was inspected: it is
[`VectorInstitute/helix`](https://github.com/VectorInstitute/helix), a
generic open-source "give an agent a codebase, a metric, and a time
budget, wake up to results" autonomous research-loop tool (`helix.yaml` +
`program.md` + `experiments.tsv`, agent-agnostic). It has no financial
data, no case-specific content, and no existing connection to fincosys or
this sync tool. This bridge does **not** integrate with it, and no such
integration should be assumed or fabricated elsewhere in this ecosystem.

## Idempotency approach

Real GnuCash import backends (OFX/QIF) store an external transaction ID in
a KVP slot conventionally named `online_id` via `xaccTransSetOnlineID()`.
That C function is not currently exposed by this repository's Python SWIG
bindings (`bindings/python/*.i`; no KVP/slot wrapper was found there at
the time this tool was written).

As a practical stand-in, `--apply` stores each fincosys `txid` in the
`Transaction`'s **`Num`** field (`trans.SetNum(txid)` /
`xaccTransSetNum()`) -- a plain text field GnuCash already provides for
exactly this kind of external reference. Before applying, it walks the
account tree, collects every existing transaction's `Num`, and skips any
plan transaction whose txid is already present. Re-running `--apply`
against the same book is therefore safe: it only creates the delta.

**Known limitation** (documented rather than silently accepted): `Num` is
a plain user-editable text field, not a dedicated external-ID slot, so it
offers weaker collision/tamper resistance than a real `online_id` KVP
slot would. Wiring a proper KVP-backed idempotency key through the Python
bindings (via a new SWIG-exposed `xaccTransSetOnlineID`/`GetOnlineID`, or
equivalent) is an open follow-up, not something this tool works around
silently.

## Tests

```bash
cd /home/user/gnucashcog-v3
python3 -m pytest bindings/python/example_scripts/fincosys_sync/tests/ -v
```

`tests/test_plan.py` mirrors the equivalent suite in the `gnucashm`
sibling repo: pure-Python coverage of `load_feed`, `load_fallback`,
`validate`, and `build_plan`, including a synthetic fixture with a
duplicate txid and an unknown-account reference. `tests/test_cognitive_bridge.py`
asserts `cognitive_atoms.json` is well-formed against the GncAtomType
vocabulary and that every `strength`/`confidence` truth value is a valid
probability in `[0, 1]`.

A `pytest.ini` in this directory pins `--import-mode=importlib` and
anchors pytest's rootdir here. Without it, pytest's default module-name
resolution for a test file this deep under `bindings/python/` walks back
up through `bindings/python/__init__.py` -- the real `gnucash` package
init, which unconditionally does `from gnucash.gnucash_core import *` --
and fails to even *collect* these tests on a machine without a compiled
GnuCash. That would defeat the entire point of `--plan-only` being
build-independent, so the ini file is required, not optional local
config.

## Smoke test

Run directly against a real fincosys checkout:

```bash
cd bindings/python/example_scripts/fincosys_sync
python3 sync_fincosys.py --plan-only --data-dir /path/to/fincosys/data --out /tmp/plan.json
python3 cognitive_bridge.py --plan /tmp/plan.json --out /tmp/cognitive_atoms.json
```

Against the `fincosys` checkout used to build this tool: 22,055
transactions loaded from `transaction_index.json`, 141 accounts
synthesized (15 entity roots + bank accounts + per-category Imbalance
accounts for the 15 `entity_code`s that actually appear in
`transaction_index.json`), 44,110 splits, zero validation issues. Note:
`MASTER_ENTITIES.json` lists 24 entities total, but only 15 of those
entity codes have any rows in `transaction_index.json` at present (the
loader only ever materializes an entity root / accounts for entities that
actually have transaction data) -- the other 9 are individuals/entities
fincosys tracks without associated bank-statement extracts yet.
`cognitive_bridge.py` then emitted 408 atoms (156 ConceptNodes + 252
InheritanceLinks) and 22,055 `balanced_transaction` evaluations, all with
`strength`/`confidence` in `[0, 1]`.

## Commerce records (QuickBooks Online / Shopify)

`commerce_import.py` is a second, independent input path. It reads
`fincosys-commerce-sync/v1` documents — the QuickBooks and Shopify records
written into the fincosys entity repositories (`fincosys/entity-rzl`, …)
under each repo's `accounting/<source>/` canonical paths — and emits the
same account/transaction shapes `sync_fincosys.py` plans and applies. The
schema is specified in `fincosys/accospace`,
`docs/COMMERCE_SYNC_SCHEMA.md`.

```bash
python3 commerce_import.py <entity-repo>/accounting/shopify/raw-json/*.json \
    --out commerce_feed.json
python3 sync_fincosys.py --feed commerce_feed.json --plan-only --out plan.json
python3 cognitive_bridge.py --plan plan.json --out cognitive_atoms.json
```

`to_sync_records()` converts the emitted dicts into this repo's
`AccountRec`/`TransactionRec` dataclasses, reusing `sync_fincosys.py`'s own
`_account_from_dict`/`_transaction_from_dict` rather than duplicating the
field mapping.

### Why it books differently from the bank-statement path

Bank-statement lines are **single-sided**, which is why that path invents
`Imbalance-<entity>-<category>` placeholders to balance them. A sales order
or invoice instead carries its own decomposition, so this books real
double-entry against named accounts with no placeholder and no plug:

```
Dr  COMM-<entity>-AR          total
    Cr  COMM-<entity>-REVENUE     subtotal
    Cr  COMM-<entity>-SHIPPING    shipping
    Cr  COMM-<entity>-TAX         tax
```

The identity `total == subtotal + shipping + tax` is checked per record, not
assumed; a record that fails it is reported with its discrepancy rather than
booked, because a document whose components don't reconcile to its own total
is a data problem to surface. `sales_period` and `product_sales_summary`
records are skipped with a reported count — they restate the same revenue as
the orders, so booking them would double-count every sale.

### What `cognitive_bridge.py` adds for commerce records

Two things, both driven off the `metadata` the importer carries through:

- **Counterparties become atoms.** A commerce document names who was billed,
  so that party gets its own `GNC_ATOM_CONCEPT_NODE`
  (`counterparty:<source>:<external_id>`, deduplicated across documents) and
  a `billed_counterparty` `GNC_ATOM_EVALUATION_LINK` to the transaction.
  This is what makes "which customers does this entity bill, and does any of
  them also appear on the bank side" answerable from the atom document.

- **Confidence comes from capture status, not `balance_valid`.** Commerce
  records are not bank-statement rows and have no balance-chain provenance;
  their splits come from the document's own decomposition. What varies is
  whether the capture was complete. A partial capture is downgraded to
  `CONFIDENCE_COMMERCE_PARTIAL`. Both tiers match accospace's
  `extracted` / `partial_capture` truth values, so a hypergraph built from
  these atoms and one built there from the same records agree.
