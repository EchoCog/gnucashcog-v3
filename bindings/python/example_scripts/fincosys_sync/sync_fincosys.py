#!/usr/bin/env python3
##  @file
#   @brief fincosys -> GnuCash sync tool
#   @ingroup python_bindings_examples
#
# sync_fincosys.py -- Sync canonical financial-forensics data from the
# `fincosys` repository (case 2025-137857, RegimA/Faucitt matter) into a
# GnuCash book, either as a dry-run "plan" (pure Python, no GnuCash bindings
# required) or applied directly against a book via the Python bindings.
#
# See fincosys_sync/README.md for the full data-flow description, the
# Imbalance-account rationale, and idempotency approach.
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

"""fincosys -> GnuCash sync tool.

Two independent modes:

  --plan-only         Pure Python. Loads fincosys data (either a normalized
                       sync-feed JSON produced by fincosys-atomspace-builder's
                       GnuCashSyncExporter, or directly from a fincosys
                       data/ directory), validates it, and writes a plan
                       JSON describing the accounts/transactions that would
                       be created. Does NOT import the `gnucash` module, so
                       it works even when GnuCash has not been built with
                       -DWITH_PYTHON=ON.

  --apply --book PATH  Requires the compiled `gnucash` Python bindings.
                       Creates/updates the Account tree and posts
                       Transactions/Splits for any fincosys txid not already
                       present in the book (idempotent re-sync).

Usage:
    python3 sync_fincosys.py --plan-only --data-dir /path/to/fincosys/data \\
        --out plan.json

    python3 sync_fincosys.py --plan-only --feed /path/to/sync_feed.json

    python3 sync_fincosys.py --apply --book /path/to/book.gnucash \\
        --plan plan.json
"""

import argparse
import json
import os
import sys
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Tuple

SCHEMA_VERSION = "1.0"

# account_type vocabulary shared by the sync-feed schema and the plan output
VALID_ACCOUNT_TYPES = {"ASSET", "LIABILITY", "BANK", "EXPENSE", "INCOME", "EQUITY"}

# Splits on a transaction must sum to (approximately) zero to be considered
# balanced. fincosys amounts are floating point ZAR values recovered from
# bank-statement OCR/extraction, so a small tolerance is required.
BALANCE_TOLERANCE = 1e-6

# category fincosys transactions fall back to when the source record has no
# `category` value at all
UNCATEGORIZED = "UNCATEGORIZED"

DEFAULT_CURRENCY = "ZAR"


class SyncFeedError(ValueError):
    """Raised when a sync feed or fincosys data directory is structurally
    invalid (missing required files/fields) -- distinct from a *content*
    validation issue (duplicate txid, unbalanced split, ...), which is
    recorded in the plan's `issues` list instead of raising."""


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------
# These dataclasses are the in-memory normalized form used by both the
# feed loader and the fincosys-data-dir fallback loader, so that everything
# downstream (validation, plan building, --apply) only has to deal with one
# shape regardless of where the data came from.

@dataclass
class AccountRec:
    code: str
    name: str
    account_type: str
    parent_code: Optional[str] = None
    entity_code: Optional[str] = None
    currency: str = DEFAULT_CURRENCY
    description: str = ""

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class SplitRec:
    account_code: str
    amount: float
    memo: str = ""

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class TransactionRec:
    txid: str
    date: str
    description: str
    splits: List[SplitRec]
    currency: str = DEFAULT_CURRENCY
    entity_code: Optional[str] = None
    metadata: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "txid": self.txid,
            "date": self.date,
            "description": self.description,
            "currency": self.currency,
            "entity_code": self.entity_code,
            "splits": [s.to_dict() for s in self.splits],
            "metadata": self.metadata,
        }


# ---------------------------------------------------------------------------
# Loader: normalized sync-feed JSON (fincosys-atomspace-builder's
# GnuCashSyncExporter output, or anything else conforming to the schema)
# ---------------------------------------------------------------------------

def _read_json(path: str) -> Any:
    if not os.path.isfile(path):
        raise SyncFeedError(f"file not found: {path}")
    with open(path, "r", encoding="utf-8") as f:
        try:
            return json.load(f)
        except json.JSONDecodeError as e:
            raise SyncFeedError(f"invalid JSON in {path}: {e}") from e


def _account_from_dict(a: Dict[str, Any]) -> AccountRec:
    if "code" not in a:
        raise SyncFeedError(f"account record missing required 'code' field: {a}")
    return AccountRec(
        code=str(a["code"]),
        name=str(a.get("name") or a["code"]),
        account_type=str(a.get("account_type") or "ASSET"),
        parent_code=(str(a["parent_code"]) if a.get("parent_code") else None),
        entity_code=(str(a["entity_code"]) if a.get("entity_code") else None),
        currency=str(a.get("currency") or DEFAULT_CURRENCY),
        description=str(a.get("description") or ""),
    )


def _split_from_dict(s: Dict[str, Any], txid: str) -> SplitRec:
    if "account_code" not in s:
        raise SyncFeedError(f"split in transaction '{txid}' missing 'account_code': {s}")
    if "amount" not in s:
        raise SyncFeedError(f"split in transaction '{txid}' missing 'amount': {s}")
    try:
        amount = float(s["amount"])
    except (TypeError, ValueError) as e:
        raise SyncFeedError(f"split in transaction '{txid}' has non-numeric amount: {s}") from e
    return SplitRec(account_code=str(s["account_code"]), amount=amount, memo=str(s.get("memo") or ""))


def _transaction_from_dict(t: Dict[str, Any]) -> TransactionRec:
    if "txid" not in t:
        raise SyncFeedError(f"transaction record missing required 'txid' field: {t}")
    txid = str(t["txid"])
    if "date" not in t:
        raise SyncFeedError(f"transaction '{txid}' missing required 'date' field")
    splits = [_split_from_dict(s, txid) for s in t.get("splits", [])]
    return TransactionRec(
        txid=txid,
        date=str(t["date"]),
        description=str(t.get("description") or ""),
        currency=str(t.get("currency") or DEFAULT_CURRENCY),
        entity_code=(str(t["entity_code"]) if t.get("entity_code") else None),
        splits=splits,
        metadata=dict(t.get("metadata") or {}),
    )


def load_feed(feed_path: str) -> Tuple[List[AccountRec], List[TransactionRec], str]:
    """Load a normalized sync-feed JSON (see fincosys_sync/README.md for the
    schema). This is the format fincosys-atomspace-builder's
    GnuCashSyncExporter is expected to produce."""
    raw = _read_json(feed_path)
    if not isinstance(raw, dict):
        raise SyncFeedError(f"sync feed {feed_path} must be a JSON object")
    accounts = [_account_from_dict(a) for a in raw.get("accounts", [])]
    txs = [_transaction_from_dict(t) for t in raw.get("transactions", [])]
    return accounts, txs, f"feed:{feed_path}"


# ---------------------------------------------------------------------------
# Loader: fincosys data/ directory fallback
# ---------------------------------------------------------------------------
# Reads data/MASTER_ENTITIES.json, data/MASTER_ACCOUNTS.json and
# data/transaction_index.json directly. transaction_index.json rows are
# single-sided bank-statement lines (one amount, one bank account) rather
# than balanced double-entry transactions, so this loader synthesizes the
# offsetting split into a per-entity `Imbalance-<category>` placeholder
# account -- the same convention GnuCash's own OFX/QIF importers use for
# unmatched sides of imported data (see Scrub.cpp: get_balance_split()
# creates an "Imbalance-<mnemonic>" account of type ACCT_TYPE_BANK). We
# mirror that ACCT_TYPE_BANK choice but key the account on
# (entity_code, category) instead of just currency, since fincosys already
# classifies every line into a category and the case is entity-scoped
# forensic analysis rather than personal bookkeeping.

def _entity_root_code(entity_code: Optional[str]) -> str:
    return f"ENTITY-{entity_code or 'UNKNOWN'}"


def _bank_account_code(account_number: Optional[str]) -> str:
    return f"BANK-{account_number or 'UNKNOWN'}"


def _imbalance_account_code(entity_code: Optional[str], category: Optional[str]) -> str:
    return f"IMBALANCE-{entity_code or 'UNKNOWN'}-{category or UNCATEGORIZED}"


def load_fallback(data_dir: str) -> Tuple[List[AccountRec], List[TransactionRec], str]:
    entities_path = os.path.join(data_dir, "MASTER_ENTITIES.json")
    accounts_path = os.path.join(data_dir, "MASTER_ACCOUNTS.json")
    tx_index_path = os.path.join(data_dir, "transaction_index.json")

    entities_raw = _read_json(entities_path)
    accounts_raw = _read_json(accounts_path)
    tx_index_raw = _read_json(tx_index_path)

    entity_by_code = {e["code"]: e for e in entities_raw.get("entities", []) if "code" in e}
    bank_by_number = {
        a["account_number"]: a for a in accounts_raw.get("accounts", []) if "account_number" in a
    }
    tx_rows = tx_index_raw.get("transactions", [])
    if not isinstance(tx_rows, list):
        raise SyncFeedError(f"{tx_index_path}: 'transactions' must be a list")

    accounts: Dict[str, AccountRec] = {}

    def ensure_entity_root(entity_code: Optional[str]) -> str:
        code = _entity_root_code(entity_code)
        if code not in accounts:
            ent = entity_by_code.get(entity_code)
            name = ent["legal_name"] if ent and ent.get("legal_name") else f"Unknown Entity ({entity_code})"
            accounts[code] = AccountRec(
                code=code,
                name=name,
                account_type="ASSET",
                parent_code=None,
                entity_code=entity_code,
                currency=DEFAULT_CURRENCY,
                description=(
                    "fincosys entity root (grouping placeholder; not itself a real "
                    f"balance-sheet account) for entity_code={entity_code}"
                ),
            )
        return code

    def ensure_bank_account(account_number: Optional[str], entity_code: Optional[str]) -> str:
        # account_number is the primary key (one bank account belongs to exactly
        # one entity). MASTER_ACCOUNTS.json's own entity_code is the authoritative
        # owner; a transaction row's entity_code (e.g. the payer on an intercompany
        # line) is only used as a fallback when the account has no master record,
        # so a later row tagged with a different entity_code can't silently move
        # an already-created account under the wrong entity root.
        code = _bank_account_code(account_number)
        meta = bank_by_number.get(account_number)
        owning_entity = (meta.get("entity_code") if meta else None) or entity_code
        if code not in accounts:
            display_name = meta["account_name"] if meta and meta.get("account_name") else "FNB Account"
            atype_desc = meta["account_type"] if meta and meta.get("account_type") else "Bank Account"
            parent = ensure_entity_root(owning_entity)
            accounts[code] = AccountRec(
                code=code,
                name=f"{display_name} ({account_number})",
                account_type="BANK",
                parent_code=parent,
                entity_code=owning_entity,
                currency=DEFAULT_CURRENCY,
                description=atype_desc,
            )
        return code

    def ensure_imbalance_account(entity_code: Optional[str], category: Optional[str]) -> str:
        cat = category or UNCATEGORIZED
        code = _imbalance_account_code(entity_code, cat)
        if code not in accounts:
            parent = ensure_entity_root(entity_code)
            accounts[code] = AccountRec(
                code=code,
                name=f"Imbalance-{cat}",
                account_type="BANK",
                parent_code=parent,
                entity_code=entity_code,
                currency=DEFAULT_CURRENCY,
                description=(
                    "Placeholder for the un-synthesized offsetting side of a "
                    "single-sided bank-statement line from fincosys/data/"
                    f"transaction_index.json (category={cat}). See "
                    "fincosys_sync/README.md 'Imbalance account rationale'."
                ),
            )
        return code

    txs: List[TransactionRec] = []
    for row in tx_rows:
        txid = row.get("txid")
        if not txid:
            raise SyncFeedError(f"transaction_index.json row missing 'txid': {row}")
        entity_code = row.get("entity_code")
        account_number = row.get("account_number")
        category = row.get("category") or UNCATEGORIZED
        bank_code = ensure_bank_account(account_number, entity_code)
        imbalance_code = ensure_imbalance_account(entity_code, category)

        try:
            amount = float(row.get("amount", 0.0))
        except (TypeError, ValueError) as e:
            raise SyncFeedError(f"transaction '{txid}' has non-numeric amount: {row.get('amount')!r}") from e

        description = row.get("description") or row.get("transaction_type") or ""
        memo = row.get("counterparty") or ""

        splits = [
            SplitRec(account_code=bank_code, amount=amount, memo=memo),
            SplitRec(account_code=imbalance_code, amount=-amount, memo=f"auto-imbalance ({category})"),
        ]
        txs.append(
            TransactionRec(
                txid=str(txid),
                date=str(row.get("date")),
                description=str(description),
                currency=str(row.get("currency") or DEFAULT_CURRENCY),
                entity_code=entity_code,
                splits=splits,
                metadata={
                    "is_intercompany": row.get("is_intercompany"),
                    "category": category,
                    "xero_account_code": row.get("xero_account_code"),
                    # Provenance of the source bank-statement row, carried
                    # through so cognitive_bridge.py can weight truth-value
                    # confidence by it (see FINCOSYS_COGNITIVE_BRIDGE.md).
                    "balance_valid": row.get("balance_valid"),
                    "balance_hash": row.get("balance_hash"),
                },
            )
        )

    return list(accounts.values()), txs, f"data-dir:{data_dir}"


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------

def validate(accounts: List[AccountRec], txs: List[TransactionRec]) -> List[Dict[str, Any]]:
    """Structural + accounting validation shared by both loaders. Returns a
    list of issue dicts; does not raise -- issues are recorded in the plan
    so the operator can decide whether they're blocking."""
    issues: List[Dict[str, Any]] = []

    account_codes = set()
    for a in accounts:
        if a.code in account_codes:
            issues.append(
                {"type": "duplicate_account_code", "account_code": a.code,
                 "detail": "account code defined more than once"}
            )
        account_codes.add(a.code)
        if a.account_type not in VALID_ACCOUNT_TYPES:
            issues.append(
                {"type": "invalid_account_type", "account_code": a.code,
                 "detail": f"'{a.account_type}' not in {sorted(VALID_ACCOUNT_TYPES)}"}
            )

    for a in accounts:
        if a.parent_code and a.parent_code not in account_codes:
            issues.append(
                {"type": "unknown_parent_account", "account_code": a.code,
                 "detail": f"parent_code '{a.parent_code}' does not reference a known account"}
            )

    seen_txids: Dict[str, int] = {}
    for i, tx in enumerate(txs):
        if tx.txid in seen_txids:
            issues.append(
                {"type": "duplicate_txid", "txid": tx.txid,
                 "detail": f"txid also appears at transaction index {seen_txids[tx.txid]} "
                           f"(this index: {i})"}
            )
        else:
            seen_txids[tx.txid] = i

        total = 0.0
        for s in tx.splits:
            total += s.amount
            if s.account_code not in account_codes:
                issues.append(
                    {"type": "unknown_account", "txid": tx.txid,
                     "detail": f"split references unknown account_code '{s.account_code}'"}
                )
        if not tx.splits:
            issues.append({"type": "no_splits", "txid": tx.txid, "detail": "transaction has zero splits"})
        elif abs(total) > BALANCE_TOLERANCE:
            issues.append(
                {"type": "unbalanced_transaction", "txid": tx.txid,
                 "detail": f"splits sum to {total:.6f}, expected 0 (tolerance {BALANCE_TOLERANCE})"}
            )

    return issues


# ---------------------------------------------------------------------------
# Plan building
# ---------------------------------------------------------------------------

def build_plan(accounts: List[AccountRec], txs: List[TransactionRec], source: str) -> Dict[str, Any]:
    issues = validate(accounts, txs)

    duplicate_txids = sorted({i["txid"] for i in issues if i["type"] == "duplicate_txid"})
    unknown_account_txids = sorted({i["txid"] for i in issues if i["type"] == "unknown_account"})
    unbalanced_txids = sorted({i["txid"] for i in issues if i["type"] == "unbalanced_transaction"})

    return {
        "schema_version": SCHEMA_VERSION,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "source": source,
        "summary": {
            "total_accounts": len(accounts),
            "total_transactions": len(txs),
            "total_splits": sum(len(t.splits) for t in txs),
            "duplicate_txid_count": len(duplicate_txids),
            "unknown_account_ref_count": len(unknown_account_txids),
            "unbalanced_transaction_count": len(unbalanced_txids),
            "issue_count": len(issues),
            "clean": len(issues) == 0,
        },
        "accounts": [a.to_dict() for a in accounts],
        "transactions": [t.to_dict() for t in txs],
        "issues": issues,
    }


def _load_accounts_and_txs(args: argparse.Namespace) -> Tuple[List[AccountRec], List[TransactionRec], str]:
    if args.feed:
        return load_feed(args.feed)
    if args.data_dir:
        return load_fallback(args.data_dir)
    raise SyncFeedError("either --feed or --data-dir must be given")


# ---------------------------------------------------------------------------
# --plan-only
# ---------------------------------------------------------------------------

def cmd_plan_only(args: argparse.Namespace) -> int:
    try:
        accounts, txs, source = _load_accounts_and_txs(args)
    except SyncFeedError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    plan = build_plan(accounts, txs, source)

    out_path = args.out or "fincosys_sync_plan.json"
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(plan, f, indent=2, sort_keys=False)

    s = plan["summary"]
    print(f"fincosys sync plan ({plan['source']})")
    print(f"  accounts:      {s['total_accounts']}")
    print(f"  transactions:  {s['total_transactions']}")
    print(f"  splits:        {s['total_splits']}")
    print(
        f"  issues:        {s['issue_count']}  "
        f"(duplicate_txid={s['duplicate_txid_count']} "
        f"unknown_account={s['unknown_account_ref_count']} "
        f"unbalanced={s['unbalanced_transaction_count']})"
    )
    print(f"  plan written:  {out_path}")
    if not s["clean"]:
        print("  status:        ISSUES FOUND (see 'issues' in the plan JSON)")
    else:
        print("  status:        clean")
    return 0 if s["clean"] else 1


# ---------------------------------------------------------------------------
# --apply  (requires the compiled `gnucash` Python bindings)
# ---------------------------------------------------------------------------

_APPLY_IMPORT_ERROR_MSG = """\
error: the 'gnucash' Python module is not importable.

--apply drives the real GnuCash engine and therefore requires GnuCash to be
built with -DWITH_PYTHON=ON and the resulting bindings/python package on
PYTHONPATH (see this repo's top-level build docs / bindings/python/README).

--plan-only does not have this requirement -- use it to validate fincosys
data and inspect the sync plan without a GnuCash build.

(ImportError: {error})
"""


def _collect_existing_txids(root_account) -> set:
    """Idempotency lookup: fincosys txids are stored in each Transaction's
    Num field on --apply (see fincosys_sync/README.md 'Idempotency
    approach' for why Num rather than a KVP slot is used here). Walk every
    split under the account tree and collect the Num of its parent
    transaction."""
    seen = set()
    for account in _walk_accounts(root_account):
        for split in account.GetSplitList():
            trans = split.GetParent()
            num = trans.GetNum()
            if num:
                seen.add(num)
    return seen


def _walk_accounts(account):
    yield account
    for child in account.get_children():
        yield from _walk_accounts(child)


def _find_child_by_code(root_account, code: str):
    for account in _walk_accounts(root_account):
        if account.GetCode() == code:
            return account
    return None


def _apply_plan(plan: Dict[str, Any], book_path: str) -> int:
    from gnucash import Account, GncNumeric, Session, SessionOpenMode, Split, Transaction
    from gnucash.gnucash_core_c import (
        ACCT_TYPE_ASSET,
        ACCT_TYPE_BANK,
        ACCT_TYPE_EQUITY,
        ACCT_TYPE_EXPENSE,
        ACCT_TYPE_INCOME,
        ACCT_TYPE_LIABILITY,
    )

    type_map = {
        "ASSET": ACCT_TYPE_ASSET,
        "LIABILITY": ACCT_TYPE_LIABILITY,
        "BANK": ACCT_TYPE_BANK,
        "EXPENSE": ACCT_TYPE_EXPENSE,
        "INCOME": ACCT_TYPE_INCOME,
        "EQUITY": ACCT_TYPE_EQUITY,
    }

    open_mode = (
        SessionOpenMode.SESSION_NORMAL_OPEN
        if os.path.exists(book_path)
        else SessionOpenMode.SESSION_NEW_STORE
    )
    session = Session(book_path, open_mode)
    try:
        book = session.book
        root = book.get_root_account()
        table = book.get_table()

        currency_cache: Dict[str, Any] = {}

        def get_currency(mnemonic: str):
            if mnemonic not in currency_cache:
                c = table.lookup("CURRENCY", mnemonic)
                if c is None:
                    raise SyncFeedError(f"unknown currency '{mnemonic}' in book commodity table")
                currency_cache[mnemonic] = c
            return currency_cache[mnemonic]

        accounts_by_code = {a["code"]: a for a in plan["accounts"]}
        gnc_account_by_code: Dict[str, Any] = {}

        def get_or_create_account(code: str):
            if code in gnc_account_by_code:
                return gnc_account_by_code[code]
            spec = accounts_by_code[code]
            acct = _find_child_by_code(root, code)
            if acct is None:
                acct = Account(book)
                acct.SetCode(code)
                acct.SetName(spec["name"])
                acct.SetType(type_map[spec["account_type"]])
                acct.SetCommodity(get_currency(spec.get("currency") or DEFAULT_CURRENCY))
                acct.SetDescription(spec.get("description") or "")
                parent_code = spec.get("parent_code")
                parent = get_or_create_account(parent_code) if parent_code else root
                parent.append_child(acct)
            gnc_account_by_code[code] = acct
            return acct

        # accounts_by_code iteration order is insertion order (Py3.7+); a
        # child may reference a parent_code not yet visited, but
        # get_or_create_account() recurses to create parents on demand.
        for code in accounts_by_code:
            get_or_create_account(code)

        existing_txids = _collect_existing_txids(root)

        created = 0
        skipped = 0
        for tx in plan["transactions"]:
            txid = tx["txid"]
            if txid in existing_txids:
                skipped += 1
                continue

            currency = get_currency(tx.get("currency") or DEFAULT_CURRENCY)
            trans = Transaction(book)
            trans.BeginEdit()
            trans.SetCurrency(currency)
            year, month, day = (int(p) for p in tx["date"].split("-"))
            trans.SetDate(day, month, year)
            trans.SetDescription(tx.get("description") or "")
            # Idempotency key. See _collect_existing_txids() docstring.
            trans.SetNum(txid)

            for s in tx["splits"]:
                split = Split(book)
                # GncNumeric(float) converts via double_to_gnc_numeric with
                # GNC_DENOM_AUTO | GNC_HOW_DENOM_FIXED, i.e. an exact decimal
                # denominator -- appropriate for ZAR cent-precision amounts.
                value = GncNumeric(float(s["amount"]))
                split.SetValue(value)
                split.SetAmount(value)
                split.SetAccount(gnc_account_by_code[s["account_code"]])
                split.SetMemo(s.get("memo") or "")
                split.SetParent(trans)

            trans.CommitEdit()
            created += 1

        session.save()
        print(f"applied to {book_path}: {created} transaction(s) created, {skipped} already synced (skipped)")
        return 0
    finally:
        session.end()


def cmd_apply(args: argparse.Namespace) -> int:
    if not args.plan and not (args.feed or args.data_dir):
        print(
            "error: --apply requires --plan <plan.json> (from a prior --plan-only run) "
            "or --feed/--data-dir to build the plan inline",
            file=sys.stderr,
        )
        return 2
    if not args.book:
        print("error: --apply requires --book <path>", file=sys.stderr)
        return 2

    try:
        import gnucash  # noqa: F401  (import-only availability check)
    except ImportError as e:
        print(_APPLY_IMPORT_ERROR_MSG.format(error=e), file=sys.stderr)
        return 3

    try:
        if args.plan:
            plan = json.loads(open(args.plan, "r", encoding="utf-8").read())
        else:
            accounts, txs, source = _load_accounts_and_txs(args)
            plan = build_plan(accounts, txs, source)
    except SyncFeedError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    s = plan["summary"]
    if not s["clean"]:
        print(
            f"error: plan has {s['issue_count']} blocking issue(s) (duplicate txids, "
            "unbalanced transactions, unknown account references, unknown parent accounts, "
            "invalid account types, duplicate account codes, or splitless transactions); "
            "run --plan-only and fix the source data first",
            file=sys.stderr,
        )
        return 4

    try:
        return _apply_plan(plan, args.book)
    except SyncFeedError as e:
        print(f"error: {e}", file=sys.stderr)
        return 5


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Sync fincosys financial-forensics data into a GnuCash book.",
    )
    mode = p.add_mutually_exclusive_group(required=True)
    mode.add_argument("--plan-only", action="store_true", help="validate + write a plan JSON, no gnucash import")
    mode.add_argument("--apply", action="store_true", help="apply a plan to a GnuCash book (requires gnucash bindings)")

    source = p.add_mutually_exclusive_group()
    source.add_argument("--feed", metavar="PATH", help="normalized sync-feed JSON (GnuCashSyncExporter output)")
    source.add_argument("--data-dir", metavar="PATH", help="fincosys data/ directory (fallback loader)")

    p.add_argument("--plan", metavar="PATH", help="(--apply) plan JSON from a prior --plan-only run")
    p.add_argument("--book", metavar="PATH", help="(--apply) GnuCash book URL/path to apply the plan to")
    p.add_argument("--out", metavar="PATH", help="(--plan-only) where to write the plan JSON (default: fincosys_sync_plan.json)")
    return p


def main(argv=None) -> int:
    args = build_arg_parser().parse_args(argv)
    if args.plan_only:
        return cmd_plan_only(args)
    return cmd_apply(args)


if __name__ == "__main__":
    sys.exit(main())
