#!/usr/bin/env python3

# test_apply_across_loaders.py -- --apply against a book built by the other
# loader, or by this one before its account codes changed.
#
# _apply_plan() needs the compiled `gnucash` bindings, which this suite never
# requires. It is driven here against a small in-memory stand-in installed as
# `gnucash` / `gnucash.gnucash_core_c` for the duration of each test. The
# stand-in implements only what _apply_plan calls, and keeps each book by
# path so a second --apply sees what the first one wrote, as a real file
# would.

import json
import os
import sys
import types

import pytest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import sync_fincosys as sf  # noqa: E402

FIXTURE_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "fixtures", "loader_equivalence"
)
DATA_DIR = os.path.join(FIXTURE_DIR, "data")
FEED_PATH = os.path.join(FIXTURE_DIR, "feed.json")
LEGACY_PLAN_PATH = os.path.join(FIXTURE_DIR, "legacy_data_dir_plan.json")


# ---------------------------------------------------------------------------
# In-memory stand-in for the parts of the gnucash bindings _apply_plan uses
# ---------------------------------------------------------------------------

class _Account:
    def __init__(self, book):
        self.code = ""
        self.name = ""
        self.type = None
        self.children = []
        self.splits = []

    def SetCode(self, code):
        self.code = code

    def GetCode(self):
        return self.code

    def SetName(self, name):
        self.name = name

    def SetType(self, t):
        self.type = t

    def SetCommodity(self, c):
        pass

    def SetDescription(self, d):
        pass

    def append_child(self, child):
        self.children.append(child)

    def get_children(self):
        return list(self.children)

    def GetSplitList(self):
        return list(self.splits)


class _Transaction:
    def __init__(self, book):
        self.num = ""

    def BeginEdit(self):
        pass

    def CommitEdit(self):
        pass

    def SetCurrency(self, c):
        pass

    def SetDate(self, d, m, y):
        pass

    def SetDescription(self, d):
        pass

    def SetNum(self, n):
        self.num = n

    def GetNum(self):
        return self.num


class _Split:
    def __init__(self, book):
        self.parent = None

    def SetValue(self, v):
        pass

    def SetAmount(self, v):
        pass

    def SetMemo(self, m):
        pass

    def SetAccount(self, account):
        account.splits.append(self)

    def SetParent(self, trans):
        self.parent = trans

    def GetParent(self):
        return self.parent


class _Book:
    def __init__(self):
        self.root = _Account(self)

    def get_root_account(self):
        return self.root

    def get_table(self):
        return types.SimpleNamespace(lookup=lambda kind, mnemonic: mnemonic)


@pytest.fixture
def gnucash(monkeypatch, tmp_path):
    """Install the stand-in; yield a function returning the book at a path."""
    books = {}

    class Session:
        def __init__(self, path, mode):
            self.book = books.setdefault(path, _Book())
            self.path = path

        def save(self):
            open(self.path, "w").close()  # so the next --apply opens, not creates

        def end(self):
            pass

    core = types.ModuleType("gnucash")
    core.Account = _Account
    core.Transaction = _Transaction
    core.Split = _Split
    core.Session = Session
    core.GncNumeric = float
    core.SessionOpenMode = types.SimpleNamespace(
        SESSION_NORMAL_OPEN="open", SESSION_NEW_STORE="new"
    )
    consts = types.ModuleType("gnucash.gnucash_core_c")
    for name in ("ASSET", "BANK", "EQUITY", "EXPENSE", "INCOME", "LIABILITY"):
        setattr(consts, f"ACCT_TYPE_{name}", name)
    core.gnucash_core_c = consts
    monkeypatch.setitem(sys.modules, "gnucash", core)
    monkeypatch.setitem(sys.modules, "gnucash.gnucash_core_c", consts)
    yield lambda path: books[path]


def _codes(book):
    return [a.code for a in sf._walk_accounts(book.root) if a is not book.root]


def _txids_by_account(book):
    return {
        a.code: sorted(s.GetParent().GetNum() for s in a.splits)
        for a in sf._walk_accounts(book.root)
        if a.splits
    }


def _apply(accounts, txs, source, path):
    plan = sf.build_plan(accounts, txs, source)
    assert plan["summary"]["clean"], plan["issues"]
    assert sf._apply_plan(plan, path) == 0


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

def test_a_data_dir_book_synced_from_the_feed_keeps_one_tree(gnucash, tmp_path):
    """Build with --data-dir, sync with --feed: one tree, nothing re-posted."""
    path = str(tmp_path / "book.gnucash")

    _apply(*sf.load_fallback(DATA_DIR), path)
    after_data_dir = _codes(gnucash(path))
    before = _txids_by_account(gnucash(path))

    _apply(*sf.load_feed(FEED_PATH), path)
    book = gnucash(path)

    # The feed may add accounts the fallback never creates (accospace's
    # counterparty stubs, see tests/fixtures/README.md), but it must not
    # add a second copy of any account the book already has.
    new = [c for c in _codes(book) if c not in after_data_dir]
    assert all(c.startswith("CP_") for c in new), new
    assert len(_codes(book)) == len(set(_codes(book)))
    assert _txids_by_account(book) == before


def test_a_book_with_the_old_codes_is_adopted_not_duplicated(gnucash, tmp_path):
    """A book the old --data-dir loader built is renamed in place on --apply.

    legacy_data_dir_plan.json is the plan the loader produced from the same
    data directory before its codes were aligned with the feed (see
    tests/fixtures/README.md), so the book below is the one a user who ran
    the old version actually has.
    """
    path = str(tmp_path / "book.gnucash")
    with open(LEGACY_PLAN_PATH, "r", encoding="utf-8") as fh:
        legacy_plan = json.load(fh)
    assert sf._apply_plan(legacy_plan, path) == 0
    assert "ENTITY-RST" in _codes(gnucash(path))

    _apply(*sf.load_fallback(DATA_DIR), path)
    _apply(*sf.load_feed(FEED_PATH), path)
    book = gnucash(path)
    codes = _codes(book)

    # One tree: every account appears once, under its new code.
    assert len(codes) == len(set(codes))
    assert {"RST", "PF", "55270035642", "55270018789"} <= set(codes)
    # Every transaction still posted exactly once, where the old book put it.
    assert _txids_by_account(book) == {
        "55270035642": ["T1", "T2", "T4"],
        "55270018789": ["T3"],
        "RST-Imbalance-PAYMENT": ["T1"],
        "PF-Imbalance-UNCATEGORIZED": ["T3"],
        "PF-Imbalance-TRANSFER": ["T4"],
        # T2 has a Xero code but no category. The old loader bucketed it as
        # UNCATEGORIZED; the aligned one buckets it by Xero code, as the
        # exporter does. An UNCATEGORIZED placeholder may also hold rows that
        # really are uncategorized, so it is not renamed to 404: it keeps its
        # old code, and RST-Imbalance-404 starts empty for later rows.
        "IMBALANCE-RST-UNCATEGORIZED": ["T2"],
    }
    # That one placeholder is the only account left under an old code.
    assert [c for c in codes if c.startswith(("ENTITY-", "BANK-", "IMBALANCE-"))] == [
        "IMBALANCE-RST-UNCATEGORIZED"
    ]


def test_legacy_code_covers_every_shape_the_old_loader_produced():
    assert sf._legacy_code("RST", {"parent_code": None}) == "ENTITY-RST"
    assert sf._legacy_code("UNASSIGNED", {"parent_code": None}) == "ENTITY-UNKNOWN"
    assert sf._legacy_code("111", {"parent_code": "RST"}) == "BANK-111"
    assert sf._legacy_code("RST-Imbalance-FEE", {"parent_code": "RST"}) == "IMBALANCE-RST-FEE"
    assert (
        sf._legacy_code("UNASSIGNED-Imbalance-FEE", {"parent_code": "UNASSIGNED"})
        == "IMBALANCE-UNKNOWN-FEE"
    )
