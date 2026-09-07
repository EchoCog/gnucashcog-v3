#!/usr/bin/env python3
##  @file
#   @brief fincosys sync-plan -> cognitive-atom-vocabulary bridge
#   @ingroup python_bindings_examples
#
# cognitive_bridge.py -- Reads a fincosys sync plan (produced by
# sync_fincosys.py --plan-only) and emits cognitive_atoms.json, describing
# how the synced entities/accounts/transactions map onto the GncAtomType
# vocabulary defined in libgnucash/engine/gnc-cognitive-accounting.h.
#
# IMPORTANT BOUNDARY: this script is a data/documentation bridge only. It
# does not call into the compiled C++ cognitive engine, does not create
# real GncAtomHandle values, and is not wired into
# gnc_cognitive_send_message()/the AtomSpace. See
# fincosys_sync/FINCOSYS_COGNITIVE_BRIDGE.md for exactly what this does and
# does not do today.
#
# Copyright (C) 2026 GnuCash Cognitive Engine
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

"""fincosys sync-plan -> cognitive atom vocabulary bridge.

Usage:
    python3 cognitive_bridge.py --plan plan.json --out cognitive_atoms.json
"""

import argparse
import json
import sys
from typing import Any, Dict, List, Optional, Tuple

SCHEMA_VERSION = "1.0"

BALANCE_TOLERANCE = 1e-6

# Confidence assigned to a transaction's balanced_transaction evaluation
# based on the provenance of the underlying bank-statement row (see
# sync_fincosys.py load_fallback(): metadata['balance_valid'] is carried
# through from fincosys/data/transaction_index.json's own balance-chain
# verification of that row).
CONFIDENCE_BALANCE_VALID_TRUE = 0.9
CONFIDENCE_BALANCE_VALID_FALSE = 0.4
CONFIDENCE_BALANCE_VALID_UNKNOWN = 0.6

# Confidence for a transaction booked from a commerce record (QuickBooks /
# Shopify -- see commerce_import.py). These have no balance_valid provenance
# flag: they are not bank-statement rows, and their splits are derived from
# the document's own decomposition rather than balanced against a statement
# chain. What does vary is whether the capture they came from is complete.
#
# The values match accospace's "extracted" and "partial_capture" truth-value
# tiers, so a hypergraph built from these atoms and one built by accospace's
# CommerceRecordLoader from the same records agree on confidence rather than
# disagreeing for no reason.
CONFIDENCE_COMMERCE_COMPLETE = 0.9
CONFIDENCE_COMMERCE_PARTIAL = 0.6


def _entity_atom_id(entity_code: str) -> str:
    return f"entity:{entity_code}"


def _account_atom_id(account_code: str) -> str:
    return f"account:{account_code}"


def _counterparty_atom_id(source: str, external_id: str) -> str:
    return f"counterparty:{source}:{external_id}"


def _tx_atom_id(txid: str) -> str:
    return f"tx:{txid}"


def _clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))


def build_atoms(plan: Dict[str, Any]) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    accounts = plan.get("accounts", [])
    transactions = plan.get("transactions", [])

    atoms: List[Dict[str, Any]] = []
    seen_atom_ids = set()

    def add_atom(atom: Dict[str, Any]) -> None:
        if atom["id"] in seen_atom_ids:
            return
        seen_atom_ids.add(atom["id"])
        atoms.append(atom)

    # -- entity ConceptNodes -------------------------------------------------
    # An entity's "legal name" is only directly available in this plan when
    # the fallback loader materialized an ENTITY-<code> root account for it
    # (see sync_fincosys.load_fallback: ensure_entity_root()); feed-sourced
    # plans may reference entity_code without ever declaring such a root
    # account, so we also pick up entity codes referenced only via
    # account.entity_code / transaction.entity_code and fall back to the
    # bare code as the label.
    entity_label_by_code: Dict[str, str] = {}
    for a in accounts:
        if a.get("code") == f"ENTITY-{a.get('entity_code')}":
            entity_label_by_code[a["entity_code"]] = a.get("name") or a["entity_code"]

    all_entity_codes = set(entity_label_by_code)
    for a in accounts:
        if a.get("entity_code"):
            all_entity_codes.add(a["entity_code"])
    for t in transactions:
        if t.get("entity_code"):
            all_entity_codes.add(t["entity_code"])

    for entity_code in sorted(all_entity_codes):
        add_atom(
            {
                "atom_type": "GNC_ATOM_CONCEPT_NODE",
                "id": _entity_atom_id(entity_code),
                "label": entity_label_by_code.get(entity_code, entity_code),
                "attributes": {"entity_code": entity_code},
            }
        )

    # -- account ConceptNodes + InheritanceLinks to their entity -----------
    account_codes = set()
    for a in accounts:
        code = a["code"]
        account_codes.add(code)
        add_atom(
            {
                "atom_type": "GNC_ATOM_CONCEPT_NODE",
                "id": _account_atom_id(code),
                "label": a.get("name") or code,
                "attributes": {
                    "account_type": a.get("account_type"),
                    "currency": a.get("currency"),
                    "parent_code": a.get("parent_code"),
                },
            }
        )

    for a in accounts:
        code = a["code"]
        entity_code = a.get("entity_code")
        # Skip the entity root account linking to itself.
        if entity_code and code != f"ENTITY-{entity_code}":
            add_atom(
                {
                    "atom_type": "GNC_ATOM_INHERITANCE_LINK",
                    "id": f"inherit:{_account_atom_id(code)}->{_entity_atom_id(entity_code)}",
                    "source": _account_atom_id(code),
                    "target": _entity_atom_id(entity_code),
                }
            )
        # Also encode the raw account-tree hierarchy (parent_code), which is
        # what GNC_ATOM_INHERITANCE_LINK is documented as representing
        # ("account type hierarchy") in gnc-cognitive-accounting.h -- this
        # is in addition to, not a replacement for, the entity link above.
        parent_code = a.get("parent_code")
        if parent_code and parent_code in account_codes:
            link_id = f"inherit:{_account_atom_id(code)}->{_account_atom_id(parent_code)}"
            add_atom(
                {
                    "atom_type": "GNC_ATOM_INHERITANCE_LINK",
                    "id": link_id,
                    "source": _account_atom_id(code),
                    "target": _account_atom_id(parent_code),
                }
            )

    # -- per-transaction EvaluationLinks ------------------------------------
    evaluations: List[Dict[str, Any]] = []
    for t in transactions:
        txid = t["txid"]
        splits = t.get("splits", [])
        participants = []
        seen_participants = set()
        for s in splits:
            pid = _account_atom_id(s["account_code"])
            if pid not in seen_participants:
                seen_participants.add(pid)
                participants.append(pid)

        total = sum(float(s.get("amount", 0.0)) for s in splits)
        gross = sum(abs(float(s.get("amount", 0.0))) for s in splits) or 1.0
        if abs(total) <= BALANCE_TOLERANCE:
            strength = 1.0
        else:
            strength = _clamp01(1.0 - (abs(total) / gross))

        metadata = t.get("metadata") or {}
        commerce_source = metadata.get("commerce_source")
        if commerce_source:
            # A commerce document, not a bank-statement row: balance_valid
            # does not apply. What matters is whether the capture it came
            # from is complete (see commerce_import.py).
            confidence = (
                CONFIDENCE_COMMERCE_PARTIAL
                if metadata.get("capture_status") == "partial"
                else CONFIDENCE_COMMERCE_COMPLETE
            )
        else:
            balance_valid = metadata.get("balance_valid")
            if balance_valid is True:
                confidence = CONFIDENCE_BALANCE_VALID_TRUE
            elif balance_valid is False:
                confidence = CONFIDENCE_BALANCE_VALID_FALSE
            else:
                confidence = CONFIDENCE_BALANCE_VALID_UNKNOWN

        evaluations.append(
            {
                "atom_type": "GNC_ATOM_EVALUATION_LINK",
                "id": _tx_atom_id(txid),
                "predicate": "balanced_transaction",
                "participants": participants,
                "truth_value": {
                    "strength": round(strength, 6),
                    "confidence": round(confidence, 6),
                },
            }
        )

        # -- commerce counterparties ---------------------------------------
        # A commerce document names who was billed. That party is a real
        # concept in the graph -- it is how "which customers does this
        # entity bill, and does any of them also appear on the bank side"
        # becomes answerable -- so give it a ConceptNode of its own and an
        # EvaluationLink to the transaction, deduplicated across documents.
        counterparty_id = metadata.get("counterparty_external_id")
        if commerce_source and counterparty_id:
            atom_id = _counterparty_atom_id(commerce_source, str(counterparty_id))
            add_atom(
                {
                    "atom_type": "GNC_ATOM_CONCEPT_NODE",
                    "id": atom_id,
                    "label": metadata.get("counterparty_name") or str(counterparty_id),
                    "attributes": {
                        "commerce_source": commerce_source,
                        "external_id": str(counterparty_id),
                        "role": "counterparty",
                    },
                }
            )
            evaluations.append(
                {
                    "atom_type": "GNC_ATOM_EVALUATION_LINK",
                    "id": f"billed:{atom_id}->{txid}",
                    "predicate": "billed_counterparty",
                    "participants": [atom_id, _tx_atom_id(txid)],
                    "truth_value": {
                        "strength": 1.0,
                        "confidence": round(confidence, 6),
                    },
                }
            )

    return atoms, evaluations


def build_cognitive_atoms_doc(plan: Dict[str, Any]) -> Dict[str, Any]:
    atoms, evaluations = build_atoms(plan)
    return {
        "schema_version": SCHEMA_VERSION,
        "source_plan": plan.get("source"),
        "generated_from_plan_generated_at": plan.get("generated_at"),
        "summary": {
            "total_atoms": len(atoms),
            "total_evaluations": len(evaluations),
        },
        "atoms": atoms,
        "evaluations": evaluations,
    }


def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description=(
            "Emit cognitive_atoms.json mapping a fincosys sync plan onto this repo's "
            "GncAtomType vocabulary (data/documentation bridge only -- see "
            "FINCOSYS_COGNITIVE_BRIDGE.md)."
        )
    )
    p.add_argument("--plan", required=True, metavar="PATH", help="plan JSON from sync_fincosys.py --plan-only")
    p.add_argument("--out", metavar="PATH", default="cognitive_atoms.json", help="output path (default: cognitive_atoms.json)")
    return p


def main(argv=None) -> int:
    args = build_arg_parser().parse_args(argv)
    try:
        with open(args.plan, "r", encoding="utf-8") as f:
            plan = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f"error: could not read plan '{args.plan}': {e}", file=sys.stderr)
        return 2

    doc = build_cognitive_atoms_doc(plan)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2, sort_keys=False)

    print(f"cognitive atom bridge ({doc['source_plan']})")
    print(f"  atoms:       {doc['summary']['total_atoms']}")
    print(f"  evaluations: {doc['summary']['total_evaluations']}")
    print(f"  written:     {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
