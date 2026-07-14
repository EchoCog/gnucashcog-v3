#!/usr/bin/env python3
"""Pure-Python tests for sync_fincosys.py (--plan-only path).

Mirrors the equivalent test suite in the gnucashm sibling repo: a small
synthetic sync-feed fixture exercises the loader/validator/plan-builder
without requiring the compiled `gnucash` Python bindings.
"""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import sync_fincosys as sf  # noqa: E402


def make_feed(accounts, transactions):
    return {
        "schema_version": "1.0",
        "accounts": accounts,
        "transactions": transactions,
    }


BASE_ACCOUNTS = [
    {"code": "ENTITY-RST", "name": "RegimA Skin Treatments CC", "parent_code": None,
     "account_type": "ASSET", "entity_code": "RST", "currency": "ZAR"},
    {"code": "BANK-111", "name": "RST Current Account", "parent_code": "ENTITY-RST",
     "account_type": "BANK", "entity_code": "RST", "currency": "ZAR"},
    {"code": "IMBALANCE-RST-PAYMENT", "name": "Imbalance-PAYMENT", "parent_code": "ENTITY-RST",
     "account_type": "BANK", "entity_code": "RST", "currency": "ZAR"},
]


def balanced_tx(txid="tx-1", amount=100.0):
    return {
        "txid": txid,
        "date": "2025-03-01",
        "description": "test transaction",
        "currency": "ZAR",
        "entity_code": "RST",
        "splits": [
            {"account_code": "BANK-111", "amount": amount, "memo": "in"},
            {"account_code": "IMBALANCE-RST-PAYMENT", "amount": -amount, "memo": "out"},
        ],
        "metadata": {"is_intercompany": False, "category": "PAYMENT", "xero_account_code": "400"},
    }


class TestLoadFeed(unittest.TestCase):
    def test_round_trip_clean_feed(self):
        feed = make_feed(BASE_ACCOUNTS, [balanced_tx("tx-1"), balanced_tx("tx-2")])
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            json.dump(feed, f)
            path = f.name
        try:
            accounts, txs, source = sf.load_feed(path)
            self.assertEqual(len(accounts), 3)
            self.assertEqual(len(txs), 2)
            self.assertEqual(source, f"feed:{path}")
        finally:
            os.unlink(path)

    def test_missing_file_raises(self):
        with self.assertRaises(sf.SyncFeedError):
            sf.load_feed("/nonexistent/path/does-not-exist.json")

    def test_account_missing_code_raises(self):
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            json.dump(make_feed([{"name": "no code here"}], []), f)
            path = f.name
        try:
            with self.assertRaises(sf.SyncFeedError):
                sf.load_feed(path)
        finally:
            os.unlink(path)

    def test_transaction_missing_txid_raises(self):
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            json.dump(make_feed(BASE_ACCOUNTS, [{"date": "2025-01-01", "splits": []}]), f)
            path = f.name
        try:
            with self.assertRaises(sf.SyncFeedError):
                sf.load_feed(path)
        finally:
            os.unlink(path)


class TestValidate(unittest.TestCase):
    def _load(self, accounts, txs):
        feed = make_feed(accounts, txs)
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            json.dump(feed, f)
            path = f.name
        try:
            return sf.load_feed(path)
        finally:
            os.unlink(path)

    def test_clean_fixture_has_no_issues(self):
        accounts, txs, _ = self._load(BASE_ACCOUNTS, [balanced_tx("tx-1"), balanced_tx("tx-2")])
        issues = sf.validate(accounts, txs)
        self.assertEqual(issues, [])

    def test_duplicate_txid_detected(self):
        accounts, txs, _ = self._load(BASE_ACCOUNTS, [balanced_tx("tx-dup"), balanced_tx("tx-dup")])
        issues = sf.validate(accounts, txs)
        dup_issues = [i for i in issues if i["type"] == "duplicate_txid"]
        self.assertEqual(len(dup_issues), 1)
        self.assertEqual(dup_issues[0]["txid"], "tx-dup")

    def test_unknown_account_detected(self):
        bad_tx = balanced_tx("tx-bad")
        bad_tx["splits"][1]["account_code"] = "IMBALANCE-DOES-NOT-EXIST"
        accounts, txs, _ = self._load(BASE_ACCOUNTS, [bad_tx])
        issues = sf.validate(accounts, txs)
        unknown_issues = [i for i in issues if i["type"] == "unknown_account"]
        self.assertEqual(len(unknown_issues), 1)
        self.assertEqual(unknown_issues[0]["txid"], "tx-bad")
        self.assertIn("IMBALANCE-DOES-NOT-EXIST", unknown_issues[0]["detail"])

    def test_unbalanced_transaction_detected(self):
        bad_tx = balanced_tx("tx-unbalanced")
        bad_tx["splits"][1]["amount"] = -50.0  # should be -100.0 to balance
        accounts, txs, _ = self._load(BASE_ACCOUNTS, [bad_tx])
        issues = sf.validate(accounts, txs)
        unbalanced = [i for i in issues if i["type"] == "unbalanced_transaction"]
        self.assertEqual(len(unbalanced), 1)
        self.assertEqual(unbalanced[0]["txid"], "tx-unbalanced")

    def test_unknown_parent_account_detected(self):
        accounts_with_bad_parent = BASE_ACCOUNTS + [
            {"code": "ORPHAN-ACCT", "name": "Orphan", "parent_code": "NO-SUCH-PARENT",
             "account_type": "BANK", "entity_code": "RST", "currency": "ZAR"},
        ]
        accounts, txs, _ = self._load(accounts_with_bad_parent, [balanced_tx("tx-1")])
        issues = sf.validate(accounts, txs)
        parent_issues = [i for i in issues if i["type"] == "unknown_parent_account"]
        self.assertEqual(len(parent_issues), 1)
        self.assertEqual(parent_issues[0]["account_code"], "ORPHAN-ACCT")

    def test_invalid_account_type_detected(self):
        accounts_with_bad_type = [dict(BASE_ACCOUNTS[0])]
        accounts_with_bad_type[0]["account_type"] = "NOT-A-REAL-TYPE"
        accounts, txs, _ = self._load(accounts_with_bad_type, [])
        issues = sf.validate(accounts, txs)
        type_issues = [i for i in issues if i["type"] == "invalid_account_type"]
        self.assertEqual(len(type_issues), 1)

    def test_duplicate_account_code_detected(self):
        accounts_with_dup = BASE_ACCOUNTS + [dict(BASE_ACCOUNTS[0])]
        accounts, txs, _ = self._load(accounts_with_dup, [])
        issues = sf.validate(accounts, txs)
        dup_account_issues = [i for i in issues if i["type"] == "duplicate_account_code"]
        self.assertEqual(len(dup_account_issues), 1)


class TestBuildPlan(unittest.TestCase):
    def test_plan_summary_reflects_duplicate_and_unknown_account(self):
        """The synthetic fixture required by the task: one duplicate txid,
        one unknown-account reference."""
        dup1 = balanced_tx("tx-dup")
        dup2 = balanced_tx("tx-dup")
        unknown_ref_tx = balanced_tx("tx-unknown-ref")
        unknown_ref_tx["splits"][1]["account_code"] = "IMBALANCE-GHOST"

        feed = make_feed(BASE_ACCOUNTS, [dup1, dup2, unknown_ref_tx])
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            json.dump(feed, f)
            path = f.name
        try:
            accounts, txs, source = sf.load_feed(path)
            plan = sf.build_plan(accounts, txs, source)
        finally:
            os.unlink(path)

        self.assertEqual(plan["schema_version"], sf.SCHEMA_VERSION)
        s = plan["summary"]
        self.assertEqual(s["total_accounts"], 3)
        self.assertEqual(s["total_transactions"], 3)
        self.assertFalse(s["clean"])
        self.assertEqual(s["duplicate_txid_count"], 1)
        self.assertEqual(s["unknown_account_ref_count"], 1)
        issue_types = {i["type"] for i in plan["issues"]}
        self.assertIn("duplicate_txid", issue_types)
        self.assertIn("unknown_account", issue_types)

    def test_clean_plan_is_marked_clean(self):
        feed = make_feed(BASE_ACCOUNTS, [balanced_tx("tx-1"), balanced_tx("tx-2")])
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            json.dump(feed, f)
            path = f.name
        try:
            accounts, txs, source = sf.load_feed(path)
            plan = sf.build_plan(accounts, txs, source)
        finally:
            os.unlink(path)
        self.assertTrue(plan["summary"]["clean"])
        self.assertEqual(plan["issues"], [])


class TestFallbackLoader(unittest.TestCase):
    """Exercises load_fallback() against a tiny synthetic fincosys data/
    directory (not the real fincosys repo -- that's covered by the
    separate smoke test)."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        entities = {
            "entities": [
                {"code": "RST", "legal_name": "RegimA Skin Treatments CC"},
                {"code": "SLG", "legal_name": "Strategic Logistics Group CC"},
            ]
        }
        accounts = {
            "accounts": [
                {"account_number": "111", "account_name": "RST Current", "account_type": "Business Account",
                 "entity_code": "RST"},
            ]
        }
        tx_index = {
            "transactions": [
                {"txid": "t1", "date": "2025-01-01", "account_number": "111", "entity_code": "RST",
                 "amount": 500.0, "transaction_type": "CREDIT", "category": "INCOME",
                 "description": "sale", "is_intercompany": False, "xero_account_code": "200",
                 "balance_valid": True, "balance_hash": "abc123"},
                {"txid": "t1", "date": "2025-01-01", "account_number": "111", "entity_code": "RST",
                 "amount": 500.0, "transaction_type": "CREDIT", "category": "INCOME",
                 "description": "sale (duplicate row)", "is_intercompany": False,
                 "xero_account_code": "200", "balance_valid": True, "balance_hash": "abc123"},
                {"txid": "t2", "date": "2025-01-02", "account_number": "999-unmapped", "entity_code": "SLG",
                 "amount": -50.0, "transaction_type": "DEBIT", "category": "FEE",
                 "description": "bank fee", "is_intercompany": False, "xero_account_code": None,
                 "balance_valid": False, "balance_hash": "def456"},
            ]
        }
        with open(os.path.join(self.tmpdir, "MASTER_ENTITIES.json"), "w") as f:
            json.dump(entities, f)
        with open(os.path.join(self.tmpdir, "MASTER_ACCOUNTS.json"), "w") as f:
            json.dump(accounts, f)
        with open(os.path.join(self.tmpdir, "transaction_index.json"), "w") as f:
            json.dump(tx_index, f)

    def test_fallback_loader_synthesizes_imbalance_accounts_and_flags_duplicate(self):
        accounts, txs, source = sf.load_fallback(self.tmpdir)
        self.assertEqual(source, f"data-dir:{self.tmpdir}")
        codes = {a.code for a in accounts}
        self.assertIn("ENTITY-RST", codes)
        self.assertIn("BANK-111", codes)
        self.assertIn("IMBALANCE-RST-INCOME", codes)
        # account_number "999-unmapped" isn't in MASTER_ACCOUNTS.json, loader
        # should still synthesize a BANK account for it rather than fail.
        self.assertIn("BANK-999-unmapped", codes)
        self.assertIn("ENTITY-SLG", codes)

        self.assertEqual(len(txs), 3)
        plan = sf.build_plan(accounts, txs, source)
        # the two "t1" rows are a genuine duplicate txid in the source data
        self.assertEqual(plan["summary"]["duplicate_txid_count"], 1)
        # every split account_code was synthesized by the loader itself, so
        # there should be no unknown-account issues
        self.assertEqual(plan["summary"]["unknown_account_ref_count"], 0)

        t2 = next(t for t in txs if t.txid == "t2")
        self.assertEqual(t2.metadata["balance_valid"], False)
        self.assertEqual(t2.metadata["category"], "FEE")


if __name__ == "__main__":
    unittest.main()
