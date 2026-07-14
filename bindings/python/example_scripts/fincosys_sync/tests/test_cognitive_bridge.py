#!/usr/bin/env python3
"""Tests for cognitive_bridge.py.

Asserts the emitted atoms/evaluations JSON is well-formed against the
GncAtomType vocabulary declared in
libgnucash/engine/gnc-cognitive-accounting.h, and that all truth values
are valid probabilities.
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import cognitive_bridge as cb  # noqa: E402
import sync_fincosys as sf  # noqa: E402

# Atom types this bridge is documented to emit. (gnc-cognitive-accounting.h
# defines a larger GncAtomType enum; the bridge only uses the subset needed
# to represent entities/accounts/hierarchy/balance-evaluations.)
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


def make_plan():
    accounts = [
        sf.AccountRec(code="ENTITY-RST", name="RegimA Skin Treatments CC", account_type="ASSET",
                      parent_code=None, entity_code="RST", currency="ZAR"),
        sf.AccountRec(code="BANK-111", name="RST Current Account", account_type="BANK",
                      parent_code="ENTITY-RST", entity_code="RST", currency="ZAR"),
        sf.AccountRec(code="IMBALANCE-RST-PAYMENT", name="Imbalance-PAYMENT", account_type="BANK",
                      parent_code="ENTITY-RST", entity_code="RST", currency="ZAR"),
    ]
    balanced_tx = sf.TransactionRec(
        txid="tx-1", date="2025-03-01", description="balanced, verified",
        currency="ZAR", entity_code="RST",
        splits=[
            sf.SplitRec(account_code="BANK-111", amount=100.0, memo="in"),
            sf.SplitRec(account_code="IMBALANCE-RST-PAYMENT", amount=-100.0, memo="out"),
        ],
        metadata={"category": "PAYMENT", "balance_valid": True},
    )
    unbalanced_tx = sf.TransactionRec(
        txid="tx-2", date="2025-03-02", description="unbalanced, flagged",
        currency="ZAR", entity_code="RST",
        splits=[
            sf.SplitRec(account_code="BANK-111", amount=100.0, memo="in"),
            sf.SplitRec(account_code="IMBALANCE-RST-PAYMENT", amount=-40.0, memo="out"),
        ],
        metadata={"category": "PAYMENT", "balance_valid": False},
    )
    unknown_provenance_tx = sf.TransactionRec(
        txid="tx-3", date="2025-03-03", description="balanced, unknown provenance",
        currency="ZAR", entity_code="RST",
        splits=[
            sf.SplitRec(account_code="BANK-111", amount=10.0, memo="in"),
            sf.SplitRec(account_code="IMBALANCE-RST-PAYMENT", amount=-10.0, memo="out"),
        ],
        metadata={"category": "PAYMENT"},
    )
    return sf.build_plan(accounts, [balanced_tx, unbalanced_tx, unknown_provenance_tx], "test-source")


class TestCognitiveBridge(unittest.TestCase):
    def setUp(self):
        self.plan = make_plan()
        self.doc = cb.build_cognitive_atoms_doc(self.plan)

    def test_top_level_shape(self):
        self.assertEqual(self.doc["schema_version"], cb.SCHEMA_VERSION)
        self.assertIn("atoms", self.doc)
        self.assertIn("evaluations", self.doc)
        self.assertIsInstance(self.doc["atoms"], list)
        self.assertIsInstance(self.doc["evaluations"], list)
        self.assertEqual(self.doc["summary"]["total_atoms"], len(self.doc["atoms"]))
        self.assertEqual(self.doc["summary"]["total_evaluations"], len(self.doc["evaluations"]))

    def test_all_atom_types_known(self):
        for atom in self.doc["atoms"]:
            self.assertIn(atom["atom_type"], KNOWN_ATOM_TYPES)
        for ev in self.doc["evaluations"]:
            self.assertEqual(ev["atom_type"], "GNC_ATOM_EVALUATION_LINK")

    def test_atom_ids_unique(self):
        ids = [a["id"] for a in self.doc["atoms"]]
        self.assertEqual(len(ids), len(set(ids)), "duplicate atom ids emitted")

    def test_every_atom_has_required_fields(self):
        for atom in self.doc["atoms"]:
            self.assertIn("atom_type", atom)
            self.assertIn("id", atom)
            self.assertTrue(atom["id"])
            if atom["atom_type"] == "GNC_ATOM_CONCEPT_NODE":
                self.assertIn("label", atom)
            if atom["atom_type"] == "GNC_ATOM_INHERITANCE_LINK":
                self.assertIn("source", atom)
                self.assertIn("target", atom)

    def test_entity_concept_node_present_with_legal_name_label(self):
        entity_atoms = [a for a in self.doc["atoms"] if a["id"] == "entity:RST"]
        self.assertEqual(len(entity_atoms), 1)
        self.assertEqual(entity_atoms[0]["atom_type"], "GNC_ATOM_CONCEPT_NODE")
        self.assertEqual(entity_atoms[0]["label"], "RegimA Skin Treatments CC")
        self.assertEqual(entity_atoms[0]["attributes"]["entity_code"], "RST")

    def test_account_concept_nodes_present(self):
        account_ids = {a["id"] for a in self.doc["atoms"] if a["atom_type"] == "GNC_ATOM_CONCEPT_NODE"}
        self.assertIn("account:BANK-111", account_ids)
        self.assertIn("account:IMBALANCE-RST-PAYMENT", account_ids)

    def test_inheritance_link_from_bank_account_to_entity(self):
        links = [
            a for a in self.doc["atoms"]
            if a["atom_type"] == "GNC_ATOM_INHERITANCE_LINK"
            and a["source"] == "account:BANK-111"
            and a["target"] == "entity:RST"
        ]
        self.assertEqual(len(links), 1)

    def test_evaluation_link_per_transaction(self):
        eval_ids = {e["id"] for e in self.doc["evaluations"]}
        self.assertEqual(eval_ids, {"tx:tx-1", "tx:tx-2", "tx:tx-3"})
        for ev in self.doc["evaluations"]:
            self.assertEqual(ev["predicate"], "balanced_transaction")
            self.assertIn("account:BANK-111", ev["participants"])
            self.assertIn("account:IMBALANCE-RST-PAYMENT", ev["participants"])

    def test_truth_values_in_unit_interval(self):
        for ev in self.doc["evaluations"]:
            tv = ev["truth_value"]
            self.assertIn("strength", tv)
            self.assertIn("confidence", tv)
            self.assertGreaterEqual(tv["strength"], 0.0)
            self.assertLessEqual(tv["strength"], 1.0)
            self.assertGreaterEqual(tv["confidence"], 0.0)
            self.assertLessEqual(tv["confidence"], 1.0)

    def test_balanced_verified_transaction_has_high_strength_and_confidence(self):
        ev = next(e for e in self.doc["evaluations"] if e["id"] == "tx:tx-1")
        self.assertEqual(ev["truth_value"]["strength"], 1.0)
        self.assertEqual(ev["truth_value"]["confidence"], cb.CONFIDENCE_BALANCE_VALID_TRUE)

    def test_unbalanced_flagged_transaction_has_lower_strength_and_confidence(self):
        ev = next(e for e in self.doc["evaluations"] if e["id"] == "tx:tx-2")
        self.assertLess(ev["truth_value"]["strength"], 1.0)
        self.assertEqual(ev["truth_value"]["confidence"], cb.CONFIDENCE_BALANCE_VALID_FALSE)

    def test_unknown_provenance_transaction_gets_middle_confidence(self):
        ev = next(e for e in self.doc["evaluations"] if e["id"] == "tx:tx-3")
        self.assertEqual(ev["truth_value"]["strength"], 1.0)
        self.assertEqual(ev["truth_value"]["confidence"], cb.CONFIDENCE_BALANCE_VALID_UNKNOWN)


if __name__ == "__main__":
    unittest.main()
