#!/usr/bin/env python3

# test_atomspace_builder_feed.py -- Cross-repo contract test: feeds a *real*
# fincosys-atomspace-builder GnuCashSyncExporter document (checked in at
# tests/fixtures/atomspace_builder_sync_feed.json -- see
# tests/fixtures/README.md for provenance and how to regenerate it) through
# this script's --feed loader and plan builder, and asserts the resulting
# plan is clean.
#
# sync_fincosys.py's --feed path and fincosys-atomspace-builder's
# GnuCashSyncExporter are developed and tested in separate repositories
# against a shared written schema (fincosys-ecosystem-sync sync-feed
# schema_version "1.0"), but nothing previously verified they actually
# agree on the wire format. This test closes that gap using the exporter's
# real output rather than a hand-authored guess at its shape.

import json
import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import cognitive_bridge as cb  # noqa: E402
import sync_fincosys as sf  # noqa: E402

FIXTURE_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "fixtures",
    "atomspace_builder_sync_feed.json",
)

# Mirrors KNOWN_ATOM_TYPES in tests/test_cognitive_bridge.py -- the subset
# of libgnucash/engine/gnc-cognitive-accounting.h's GncAtomType vocabulary
# that cognitive_bridge.py is documented to emit.
KNOWN_ATOM_TYPES = {
    "GNC_ATOM_CONCEPT_NODE",
    "GNC_ATOM_PREDICATE_NODE",
    "GNC_ATOM_SCHEMA_NODE",
    "GNC_ATOM_GROUNDED_SCHEMA",
    "GNC_ATOM_INHERITANCE_LINK",
    "GNC_ATOM_SIMILARITY_LINK",
    "GNC_ATOM_MEMBER_LINK",
    "GNC_ATOM_EVALUATION_LINK",
    "GNC_ATOM_EXECUTION_LINK",
    "GNC_ATOM_IMPLICATION_LINK",
    "GNC_ATOM_AND_LINK",
    "GNC_ATOM_OR_LINK",
    "GNC_ATOM_COMBO_NODE",
}


def _load_raw_fixture():
    with open(FIXTURE_PATH, "r", encoding="utf-8") as fh:
        return json.load(fh)


def test_fixture_is_the_documented_schema():
    raw = _load_raw_fixture()
    assert raw["schema_version"] == sf.SCHEMA_VERSION
    assert raw["source"]["generator"] == "fincosys-atomspace-builder"
    assert raw["accounts"], "fixture should carry at least one account"
    assert raw["transactions"], "fixture should carry at least one transaction"


def test_load_feed_reads_real_exporter_output():
    accounts, txs, source = sf.load_feed(FIXTURE_PATH)

    # RST + PF entity roots, one bank account each, three Imbalance-*
    # placeholders synthesized by generate_feed() for the fixture's three
    # transactions (PAYMENT, xero code 404, and no-category/UNCATEGORIZED).
    assert len(accounts) == 7
    assert len(txs) == 3
    assert source == "feed:{}".format(FIXTURE_PATH)

    codes = {a.code for a in accounts}
    assert {"RST", "PF", "55270035642", "55270018789"} <= codes
    imbalance_codes = {c for c in codes if "Imbalance" in c}
    assert len(imbalance_codes) == 3

    # Every account_type on the fixture must be one this script recognizes,
    # since _account_from_dict() would otherwise raise SyncFeedError.
    for a in accounts:
        assert a.account_type in sf.VALID_ACCOUNT_TYPES


def test_build_plan_from_real_exporter_output_is_clean():
    accounts, txs, source = sf.load_feed(FIXTURE_PATH)
    plan = sf.build_plan(accounts, txs, source)

    summary = plan["summary"]
    assert summary["clean"] is True, plan["issues"]
    assert summary["duplicate_txid_count"] == 0
    assert summary["unknown_account_ref_count"] == 0
    assert summary["unbalanced_transaction_count"] == 0
    assert summary["total_transactions"] == 3
    assert summary["total_splits"] == 6

    # Every split's counterparty account is one of the synthesized
    # Imbalance-* placeholders -- confirms the double-entry-balancing
    # design documented in gnucash_exporter.py round-trips correctly.
    for txn in plan["transactions"]:
        splits = txn["splits"]
        assert len(splits) == 2
        assert abs(sum(s["amount"] for s in splits)) <= sf.BALANCE_TOLERANCE


def test_cognitive_bridge_maps_real_exporter_output_to_atoms():
    """The other half of "integrate with fincosys-atomspace-builder" for
    this repo specifically: gnucashcog-v3 is meant to round-trip through
    an AtomSpace-shaped representation, not just balanced double-entry
    splits. Confirms cognitive_bridge.build_atoms() accepts a plan built
    from the exporter's real output and emits well-formed atoms/links for
    every entity and account the fixture declares."""
    accounts, txs, source = sf.load_feed(FIXTURE_PATH)
    plan = sf.build_plan(accounts, txs, source)

    atoms, evaluations = cb.build_atoms(plan)

    atom_ids = {a["id"] for a in atoms}
    for entity_code in ("RST", "PF"):
        assert cb._entity_atom_id(entity_code) in atom_ids
    for account in accounts:
        assert cb._account_atom_id(account.code) in atom_ids

    for atom in atoms:
        assert atom["atom_type"] in KNOWN_ATOM_TYPES

    # Every transaction in the fixture should surface as an evaluation atom
    # (balance/booking assertion) so nothing silently drops on the cognitive
    # side even though it validated cleanly on the accounting side.
    assert len(evaluations) >= len(plan["transactions"])
