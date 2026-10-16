#!/usr/bin/env python3

# test_loader_equivalence.py -- The two input paths must code the same
# account the same way.
#
# tests/fixtures/loader_equivalence/ holds a small fincosys data/ directory
# and feed.json, the sync feed fincosys/accospace produced from that same
# directory (see tests/fixtures/README.md). --apply matches accounts already
# in a book by code, so if load_fallback() (--data-dir) and load_feed()
# (--feed) code an account differently, a book built with one and later
# synced with the other grows a second, parallel account tree.
#
# Pure Python: no `gnucash` import.

import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import sync_fincosys as sf  # noqa: E402

FIXTURE_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "fixtures", "loader_equivalence"
)
DATA_DIR = os.path.join(FIXTURE_DIR, "data")
FEED_PATH = os.path.join(FIXTURE_DIR, "feed.json")


def _both():
    fallback_accounts, fallback_txs, _ = sf.load_fallback(DATA_DIR)
    feed_accounts, feed_txs, _ = sf.load_feed(FEED_PATH)
    return fallback_accounts, fallback_txs, feed_accounts, feed_txs


def test_both_loaders_see_the_same_transactions():
    _, fallback_txs, _, feed_txs = _both()
    assert sorted(t.txid for t in fallback_txs) == sorted(t.txid for t in feed_txs)
    assert len(fallback_txs) == 4


def test_every_split_lands_in_the_same_account_on_either_path():
    """The property --apply depends on: same txid, same accounts."""
    _, fallback_txs, _, feed_txs = _both()
    feed_by_txid = {t.txid: t for t in feed_txs}
    for tx in fallback_txs:
        fallback_codes = sorted(s.account_code for s in tx.splits)
        feed_codes = sorted(s.account_code for s in feed_by_txid[tx.txid].splits)
        assert fallback_codes == feed_codes, tx.txid


def test_every_fallback_account_exists_in_the_feed_under_the_same_parent():
    """No account the fallback creates would be a stranger to a feed-built book.

    The feed may hold more (it currently also emits counterparty stubs as
    top-level accounts, an accospace issue -- see tests/fixtures/README.md),
    so this is containment, not equality.
    """
    fallback_accounts, _, feed_accounts, _ = _both()
    feed_parent = {a.code: a.parent_code for a in feed_accounts}
    for a in fallback_accounts:
        assert a.code in feed_parent, a.code
        assert feed_parent[a.code] == a.parent_code, a.code


def test_a_row_with_only_a_xero_code_is_bucketed_by_it():
    """The exporter buckets by category, then Xero code, then UNCATEGORIZED."""
    _, fallback_txs, _, _ = _both()
    t2 = next(t for t in fallback_txs if t.txid == "T2")
    assert "RST-Imbalance-404" in {s.account_code for s in t2.splits}
