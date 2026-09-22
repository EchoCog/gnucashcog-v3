#!/usr/bin/env python3

# test_commerce_cognitive.py -- covers the commerce-record handling added to
# cognitive_bridge.py: counterparty ConceptNodes, the billed_counterparty
# predicate, and the capture-status confidence path that replaces
# balance_valid for records that are not bank-statement rows.
#
# The end of the file walks the whole loop on real captured Shopify records:
#
#   commerce document -> commerce_import.convert()
#                     -> sync_fincosys.build_plan()
#                     -> cognitive_bridge.build_cognitive_atoms_doc()
#
# and asserts the resulting atom document is well-formed -- every evaluation
# referencing an atom the same run declared. cognitive_atoms.json is read
# back by this repo's own importer (gnucash/gnucash-commands.cpp
# auto-detects it, see FINCOSYS_COGNITIVE_BRIDGE.md), which skips any link
# whose endpoints it cannot resolve and reports the count. A dangling
# reference is therefore silent edge loss, not an error, which is exactly
# the kind of thing a test should catch rather than a reader.

import json
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import cognitive_bridge as cb  # noqa: E402
import commerce_import as ci  # noqa: E402
import sync_fincosys as sf  # noqa: E402

FIXTURE_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "fixtures",
    "commerce_shopify_rzl.json",
)


def _commerce_plan(capture_status="complete", counterparty=True):
    """A minimal plan shaped like one built from commerce records."""
    metadata = {
        "commerce_source": "shopify",
        "record_type": "sales_order",
        "document_number": "#10318",
        "capture_status": capture_status,
    }
    if counterparty:
        metadata["counterparty_external_id"] = "7554686779459"
        metadata["counterparty_name"] = "Kat Buckley"

    return {
        "accounts": [
            {"code": "COMM-RZL-AR", "name": "Accounts Receivable",
             "account_type": "ASSET", "entity_code": "RZL", "currency": "GBP"},
            {"code": "COMM-RZL-REVENUE", "name": "Sales Revenue",
             "account_type": "INCOME", "entity_code": "RZL", "currency": "GBP"},
        ],
        "transactions": [{
            "txid": "COMMERCE:SHOPIFY_RZL_ORDER_10318",
            "date": "2026-08-01",
            "description": "shopify #10318",
            "currency": "GBP",
            "entity_code": "RZL",
            "splits": [
                {"account_code": "COMM-RZL-AR", "amount": 412.45},
                {"account_code": "COMM-RZL-REVENUE", "amount": -412.45},
            ],
            "metadata": metadata,
        }],
    }


def _by_id(items):
    return {i["id"]: i for i in items}


# -- counterparties ---------------------------------------------------------


def test_a_commerce_counterparty_becomes_a_concept_node():
    atoms, evaluations = cb.build_atoms(_commerce_plan())

    atom = _by_id(atoms)["counterparty:shopify:7554686779459"]
    assert atom["atom_type"] == "GNC_ATOM_CONCEPT_NODE"
    assert atom["label"] == "Kat Buckley"
    assert atom["attributes"]["role"] == "counterparty"
    assert atom["attributes"]["commerce_source"] == "shopify"


def test_the_counterparty_is_linked_to_its_transaction():
    _, evaluations = cb.build_atoms(_commerce_plan())

    link = _by_id(evaluations)[
        "billed:counterparty:shopify:7554686779459->COMMERCE:SHOPIFY_RZL_ORDER_10318"
    ]
    assert link["predicate"] == "billed_counterparty"
    assert link["participants"] == [
        "counterparty:shopify:7554686779459",
        cb._tx_atom_id("COMMERCE:SHOPIFY_RZL_ORDER_10318"),
    ]


def test_counterparties_are_deduplicated_across_transactions():
    plan = _commerce_plan()
    second = json.loads(json.dumps(plan["transactions"][0]))
    second["txid"] = "COMMERCE:SHOPIFY_RZL_ORDER_10319"
    plan["transactions"].append(second)

    atoms, evaluations = cb.build_atoms(plan)

    parties = [a for a in atoms if a["id"].startswith("counterparty:")]
    assert len(parties) == 1
    billed = [e for e in evaluations if e["predicate"] == "billed_counterparty"]
    assert len(billed) == 2


def test_a_record_without_a_counterparty_produces_no_party_atom():
    atoms, evaluations = cb.build_atoms(_commerce_plan(counterparty=False))

    assert not [a for a in atoms if a["id"].startswith("counterparty:")]
    assert not [e for e in evaluations if e["predicate"] == "billed_counterparty"]


def test_a_bank_transaction_produces_no_counterparty_atom():
    """Only commerce records name a billed party; statement rows do not."""
    plan = _commerce_plan()
    plan["transactions"][0]["metadata"] = {"balance_valid": True}

    atoms, _ = cb.build_atoms(plan)

    assert not [a for a in atoms if a["id"].startswith("counterparty:")]


# -- confidence -------------------------------------------------------------


def test_a_complete_commerce_capture_gets_the_complete_confidence():
    _, evaluations = cb.build_atoms(_commerce_plan("complete"))

    tx = _by_id(evaluations)[cb._tx_atom_id("COMMERCE:SHOPIFY_RZL_ORDER_10318")]
    assert tx["truth_value"]["confidence"] == pytest.approx(
        cb.CONFIDENCE_COMMERCE_COMPLETE
    )


def test_a_partial_commerce_capture_is_downgraded():
    _, evaluations = cb.build_atoms(_commerce_plan("partial"))

    tx = _by_id(evaluations)[cb._tx_atom_id("COMMERCE:SHOPIFY_RZL_ORDER_10318")]
    assert tx["truth_value"]["confidence"] == pytest.approx(
        cb.CONFIDENCE_COMMERCE_PARTIAL
    )
    assert cb.CONFIDENCE_COMMERCE_PARTIAL < cb.CONFIDENCE_COMMERCE_COMPLETE


def test_commerce_confidence_does_not_use_the_balance_valid_path():
    """balance_valid is a bank-statement provenance flag and must not decide
    confidence for a commerce record that happens to carry one."""
    plan = _commerce_plan("complete")
    plan["transactions"][0]["metadata"]["balance_valid"] = False

    _, evaluations = cb.build_atoms(plan)

    tx = _by_id(evaluations)[cb._tx_atom_id("COMMERCE:SHOPIFY_RZL_ORDER_10318")]
    assert tx["truth_value"]["confidence"] == pytest.approx(
        cb.CONFIDENCE_COMMERCE_COMPLETE
    )


def test_bank_transactions_still_use_balance_valid():
    plan = _commerce_plan()
    plan["transactions"][0]["metadata"] = {"balance_valid": False}

    _, evaluations = cb.build_atoms(plan)

    tx = _by_id(evaluations)[cb._tx_atom_id("COMMERCE:SHOPIFY_RZL_ORDER_10318")]
    assert tx["truth_value"]["confidence"] == pytest.approx(
        cb.CONFIDENCE_BALANCE_VALID_FALSE
    )


def test_commerce_confidence_tiers_match_accospace():
    """accospace's CommerceRecordLoader uses (1.0, 0.9) for a complete
    capture and (1.0, 0.6) for a partial one. Building a hypergraph from
    these atoms and building one there from the same records should not
    disagree about confidence."""
    assert cb.CONFIDENCE_COMMERCE_COMPLETE == 0.9
    assert cb.CONFIDENCE_COMMERCE_PARTIAL == 0.6


# -- full loop on real records ---------------------------------------------


@pytest.mark.skipif(
    not os.path.exists(FIXTURE_PATH), reason="commerce fixture not present"
)
def test_real_records_traverse_the_whole_loop_to_a_wellformed_atom_document():
    document = ci.load_commerce_document(FIXTURE_PATH)
    accounts, transactions, report = ci.convert(document, source_path=FIXTURE_PATH)
    assert report["rejected"] == []

    account_recs, txn_recs = ci.to_sync_records(accounts, transactions)
    plan = sf.build_plan(account_recs, txn_recs, "commerce-fixture")
    assert plan["summary"]["clean"], plan["issues"][:5]

    doc = cb.build_cognitive_atoms_doc(plan)

    declared = {a["id"] for a in doc["atoms"]} | {
        e["id"] for e in doc["evaluations"]
    }
    # Every evaluation must reference atoms this same document declares:
    # the importer skips links it cannot resolve rather than failing.
    for evaluation in doc["evaluations"]:
        for participant in evaluation["participants"]:
            assert participant in declared, (evaluation["id"], participant)

    assert doc["summary"]["total_atoms"] == len(doc["atoms"])
    assert doc["summary"]["total_evaluations"] == len(doc["evaluations"])

    billed = [e for e in doc["evaluations"] if e["predicate"] == "billed_counterparty"]
    parties = [a for a in doc["atoms"] if a["id"].startswith("counterparty:")]
    assert len(billed) == len(transactions)
    # The fixture has repeat customers, so parties must be fewer than orders.
    assert 0 < len(parties) < len(transactions)
